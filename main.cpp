#include "pico/stdlib.h"
#include <cstring>
#include <cstdio>
#include <cmath>

#include "packets.h"
#include "pinouts.h"
#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/socket.h"
#include "drivers/lcd1602.h"
#include "drivers/Adafruit_NAU7802.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"

// debug helpers
void wait_for_one(void);

// helper functions
static void init_i2c_adc();
static void send_discovery();
static bool check_setup_command(CommandPacket &out);
static void send_console(const char* msg);
static void check_restart();

CommandPacket qt_instruction(uint8_t sock);

constexpr uint8_t DATA_SOCK = 0;
static uint16_t g_device_id = 1;
static bool g_is_setup = false;
static CommandPacket g_cached_cmd{};
static bool g_has_cached_cmd = false;

int main()
{
    stdio_init_all();
    sleep_ms(100);
    ////printf("BINCO BOOT\n");
    send_console("BINCO BOOT\n");

    /* ---------- LED ---------- */
    gpio_init(T_LED);
    gpio_set_dir(T_LED, GPIO_OUT);
    gpio_put(T_LED, 0);

    /* ---------- ADC ---------- */
    init_i2c_adc();

    Adafruit_NAU7802 adc;
    if (!adc.begin(i2c1))
    {
        ////printf("ADC Init Failed\n");
        send_console("ADC Init Failed\n");
        while(true);
    }

    adc.calibrate(NAU7802_CALMOD_INTERNAL);

    ////printf("Waiting Setup\n");
    send_console("Waiting Setup\n");

    /* ---------- NETWORK ---------- */
    if (!w5500_init())
    {
        ////printf("W5500 Init Failed\n");
        send_console("W5500 Init Failed\n");
        while(true);
    }

    if (socket(DATA_SOCK, Sn_MR_UDP, 5001, 0) != DATA_SOCK)
    {
        //printf("Socket Init Failed\n");
        send_console("Socket Init Failed\n");
        while(true);
    }

//    printf("Network Ready\n");
    send_console("Network Ready\n");
    wiz_NetInfo netinfo;
    ctlnetwork(CN_GET_NETINFO, &netinfo);

//    printf("IP: %d.%d.%d.%d\n",
//           netinfo.ip[0], netinfo.ip[1],
//           netinfo.ip[2], netinfo.ip[3]);
//
//    printf("MASK: %d.%d.%d.%d\n",
//           netinfo.sn[0], netinfo.sn[1],
//           netinfo.sn[2], netinfo.sn[3]);

    /* ===== DISCOVERY PHASE ===== */

    while(!g_is_setup)
    {
        send_discovery();

        CommandPacket setup_cmd;

        if(check_setup_command(setup_cmd))
        {
            if(setup_cmd.command == CMD_SETUP)
            {
                g_is_setup = true;
                //printf("Setup command received\n");
                send_console("Setup command received\n");
            }
        }

        sleep_ms(500);
    }

    CommandPacket cmd;


/* ===== WAIT ZERO CALIBRATE ===== */

    //printf("Remove all weight\n");
    send_console("Remove all weight\n");

    /* ===== WAIT BASE CALIBRATE ===== */

    //printf("Waiting CMD_CALI\n");
    send_console("When done, press Base Calibration\n");

    do {
        cmd = qt_instruction(DATA_SOCK);
    } while(cmd.command != CMD_CALI);

    //printf("CMD_CALI RECEIVED\n");

    const int SAMPLES = 64;

/* ===== TARE ===== */

    int64_t tare_sum = 0;

    for (int i = 0; i < SAMPLES; i++)
    {
        while (!adc.available()) {
            check_restart();
        }
        tare_sum += adc.read();
    }

    int32_t tare_offset = tare_sum / SAMPLES;

    //printf("Tare offset = %ld\n", tare_offset);

/* ===== CALIBRATION REFERENCE ===== */

    //printf("Place calibration weight\n");
    send_console("Place calibration weight\n");
    send_console("When done, press Zero Calibration\n");

    do {
        cmd = qt_instruction(DATA_SOCK);
    } while(cmd.command != CMD_TARE);

    //printf("CMD_TARE RECEIVED\n");
    send_console("Please wait\n");

    int64_t cal_sum = 0;

    for (int i = 0; i < SAMPLES; i++)
    {
        while (!adc.available()) {
            check_restart();
        }
        cal_sum += adc.read();
    }

    int32_t cal_avg = cal_sum / SAMPLES;

    int32_t delta = cal_avg - tare_offset;

    if(delta <= 0)
    {
        //printf("Calibration failed\n");
        send_console("Calibration failed\n");
        while(true){
            check_restart();
        }
    }

    float grams_per_count = 5.3f / delta;

    //printf("Calibration OK\n");
    send_console("Calibration OK\n");
    //printf("grams_per_count = %.8f\n", grams_per_count);
    char msg[64];
    //printf(msg, sizeof(msg), "grams_per_count = %.8f", grams_per_count);
    send_console(msg);

    BincoData data{};
    const char nm[] = "Binco1";
    for(int i = 0; i < 16 && nm[i]; ++i)
        data.name[i] = nm[i];

    data.ID = g_device_id;

    while(true)
    {
        check_restart();
        /* --- Read ADC --- */
        while (!adc.available()) {
            check_restart();
        }

        int32_t raw = adc.read();

        float grams = (raw - tare_offset) * grams_per_count;

        if(grams < 0)
            grams = 0;

        /* --- Fill packet --- */
        data.weight_g = grams;
        data.quantity = 0;      // update if needed later
        data.state    = STATE_RUNNING;

        /* --- Send to Qt --- */
        uint8_t qt_ip[4] = {192,168,10,1};

        sendto(
                DATA_SOCK,
                (uint8_t*)&data,
                sizeof(data),
                qt_ip,
                5001
        );

        sleep_ms(100);   // update rate

    }
}

/* ======================= HELPERS ======================= */

void wait_for_one()
{
    //printf("Enter 1 to continue...\n");

    int c;
    do {
        c = getchar_timeout_us(0);
    } while (c != '1');

    //printf("Continuing...\n");
}

static void init_i2c_adc()
{
    i2c_init(i2c1, 400 * 1000);

    gpio_set_function(ADC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(ADC_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(ADC_SDA);
    gpio_pull_up(ADC_SCL);
}

CommandPacket qt_instruction(uint8_t sock)
{
    // If check_restart() already consumed a command that isn't restart,
    // return it here first.
    if (g_has_cached_cmd) {
        g_has_cached_cmd = false;
        return g_cached_cmd;
    }

    CommandPacket cmd{};
    uint8_t src_ip[4];
    uint16_t src_port;

    while (true)
    {
        // allow restart while blocking
        check_restart();

        int len = recvfrom(sock,
                           (uint8_t*)&cmd,
                           sizeof(cmd),
                           src_ip,
                           &src_port);

        if (len == sizeof(CommandPacket))
        {
            return cmd;
        }

        sleep_ms(50);
    }
}

static void send_discovery()
{
    BincoData data{};
    uint8_t dst_ip[4] = {192,168,10,1};
    uint16_t dst_port = 5001;


    const char nm[] = "Binco1";
    for(int i = 0; i <16 && nm[i]; ++i) data.name[i] = nm[i];
    data.ID = g_device_id;
    data.quantity  = 0;
    data.weight_g  = 0.0f;
    data.state     = STATE_IDLE;

    uint8_t broadcast_ip[4] = {192,168,255,255};

    int ret = sendto(
            DATA_SOCK,
            (uint8_t*)&data,
            sizeof(data),
            broadcast_ip,
            5000
    );

    //printf("sendto ret = %d\n", ret);
    send_console("sendto ret = 25");
}

static bool check_setup_command(CommandPacket &out)
{
    if (getSn_RX_RSR(DATA_SOCK) < sizeof(CommandPacket))
        return false;

    uint8_t src_ip[4];
    uint16_t src_port;

    int len = recvfrom(
            DATA_SOCK,
            (uint8_t*)&out,
            sizeof(CommandPacket),
            src_ip,
            &src_port
    );

    if(len != sizeof(CommandPacket))
        return false;

    if(out.ID != g_device_id)
        return false;

    return true;
}

static void send_console(const char* msg)
{
    ConsolePacket pkt{};

    pkt.ID = g_device_id;

    std::snprintf(pkt.text, sizeof(pkt.text), "%s", msg);

    uint8_t qt_ip[4] = {192,168,10,1};   // Qt host
    uint16_t qt_port = 5001;

    sendto(
            DATA_SOCK,
            (uint8_t*)&pkt,
            sizeof(pkt),
            qt_ip,
            qt_port
    );
}

static void check_restart()
{
    // if nothing waiting, do nothing
    if (getSn_RX_RSR(DATA_SOCK) < sizeof(CommandPacket))
        return;

    CommandPacket cmd{};
    uint8_t ip[4];
    uint16_t port;

    int len = recvfrom(DATA_SOCK,
                       (uint8_t*)&cmd,
                       sizeof(cmd),
                       ip,
                       &port);

    if (len != sizeof(CommandPacket))
        return;

    // ignore packets not for this device
    if (cmd.ID != g_device_id)
        return;

    // handle restart
    if (cmd.command == CMD_RESTART)
    {
        send_console("Restarting Binco...\n");
        sleep_ms(200);              // let UDP send out
        watchdog_enable(1, 1);
        while (true) {}
    }

    // otherwise cache it so qt_instruction() can still read it
    g_cached_cmd = cmd;
    g_has_cached_cmd = true;
}