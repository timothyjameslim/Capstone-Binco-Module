#include "pico/stdlib.h"
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
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
void wait_for_one();

// helper functions
static void init_i2c_adc();
static void send_discovery();
static bool check_setup_command(CommandPacket &out);
static void send_console(const char* fmt, ...);
static void check_restart();
CommandPacket qt_instruction(uint8_t sock);

constexpr uint8_t DATA_SOCK = 0;
static uint16_t g_device_id = 0;
static bool g_is_setup = false;
static CommandPacket g_cached_cmd{};
static bool g_has_cached_cmd = false;
static float g_unit_weight = 6.56f;
static float g_unit_offset = 0.02f;
float filtered_grams = 0.0f;
const float alpha = 0.2f;   // 0.1–0.3 (lower = smoother)
static bool  g_profile_loaded = false;

int main()
{
    stdio_init_all();
    printf("BINCO BOOT\n");
    send_console("BINCO BOOT\n");

    /* ---------- LED ---------- */
    //gpio_init(T_LED);
    //gpio_set_dir(T_LED, GPIO_OUT);
    //gpio_put(T_LED, false);

    /* ---------- LCD ---------- */
    LCD1602 lcd;
    printf("Starting LCD Init\n");
    lcd.init();
    printf("LCD Init Complete\n");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BINCO READY");
    sleep_ms(1000);

    /* ---------- ADC ---------- */
    init_i2c_adc();

    Adafruit_NAU7802 adc;
    if (!adc.begin(i2c1))
    {
        printf("ADC Init Failed\n");
        send_console("ADC Init Failed\n");
        while(true);
    }
    printf("ADC Init success\n");

    adc.calibrate(NAU7802_CALMOD_INTERNAL);

    printf("Waiting Setup\n");
    send_console("Waiting Setup\n");

    /* ---------- NETWORK ---------- */
    if (!w5500_init())
    {
        printf("W5500 Init Failed\n");
        send_console("W5500 Init Failed\n");
        while(true);
    }

    if (socket(DATA_SOCK, Sn_MR_UDP, 5001, 0) != DATA_SOCK)
    {
        printf("Socket Init Failed\n");
        send_console("Socket Init Failed\n");
        while(true);
    }

    printf("Network Ready\n");
    send_console("Network Ready\n");
    wiz_NetInfo netinfo;
    ctlnetwork(CN_GET_NETINFO, &netinfo);

    printf("IP: %d.%d.%d.%d\n",
           netinfo.ip[0], netinfo.ip[1],
           netinfo.ip[2], netinfo.ip[3]);

    printf("MASK: %d.%d.%d.%d\n",
           netinfo.sn[0], netinfo.sn[1],
           netinfo.sn[2], netinfo.sn[3]);

    uint8_t last_octet = netinfo.ip[3];
    const uint8_t BASE_IP = 111;

    if (last_octet >= BASE_IP)
    {
        g_device_id = last_octet - BASE_IP + 1;
    }
    else
    {
        g_device_id = 1;
    }

    printf("Derived BINCO ID: %d\n", g_device_id);

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
                printf("Setup command received\n");
                send_console("Setup command received\n");
                //send_console("Waiting for Profile");
            }
        }

        sleep_ms(500);
    }

    /* ===== PROFILE PHASE ===== */

    int got_weight = 0;
    int got_offset = 0;

    printf("Waiting Profile\n");

    while(!g_profile_loaded)
    {
        CommandPacket cmd;

        if(check_setup_command(cmd))
        {
            printf("PROFILE cmd=%d val=%.3f\n", cmd.command, cmd.value);

            if(cmd.command == CMD_SET_UNIT_WEIGHT)
            {
                g_unit_weight = cmd.value;
                got_weight = 1;
            }
            else if(cmd.command == CMD_SET_UNIT_OFFSET)
            {
                g_unit_offset = cmd.value;
                got_offset = 1;
            }

            if(got_weight && got_offset)
            {
                g_profile_loaded = true;
                printf("Profile loaded\n");
            }
        }

        sleep_ms(50);
    }

    CommandPacket cmd;

/* ===== WAIT ZERO CALIBRATE ===== */

    send_console("Unit Weight = %.3f", g_unit_weight);
    send_console("Unit Offset = %.3f", g_unit_offset);

    //printf("Remove all weight\n");
    send_console("Remove all weight\n");

    /* ===== WAIT BASE CALIBRATE ===== */

    //printf("Waiting CMD_CALI\n");
    send_console("When done, press Base Calibration\n");

    do {
        cmd = qt_instruction(DATA_SOCK);
    } while(cmd.command != CMD_CALI);

    //printf("CMD_CALI RECEIVED\n");
    send_console("Please wait\n");

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

    printf("CMD_TARE RECEIVED\n");
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

    float grams_per_count = g_unit_weight / delta;

    //printf("Calibration OK\n");
    send_console("Calibration OK\n");
    printf("grams_per_count = %.8f\n", grams_per_count);
    char msg[64];
    printf(msg, sizeof(msg), "grams_per_count = %.8f", grams_per_count);
    send_console(msg);

    BincoData data{}, last{};
    const char nm[] = "binco3";
    for(int i = 0; i < 16 && nm[i]; ++i)
        data.name[i] = nm[i];

    data.ID = g_device_id;

    while(true)
    {
        check_restart();

        while (!adc.available()) {
            check_restart();
        }

        int32_t raw = adc.read();

        float grams_raw = (raw - tare_offset) * grams_per_count;
        if (grams_raw < 0) grams_raw = 0;

        static float filtered_grams = 0.0f;
        static bool filter_init = false;

        if (!filter_init) {
            filtered_grams = grams_raw;
            filter_init = true;
        } else {
            filtered_grams = alpha * grams_raw + (1.0f - alpha) * filtered_grams;
        }

        float grams = filtered_grams;

        float display_weight = roundf(grams * 100.0f) / 100.0f;

        float unit_nominal = g_unit_weight;
        float tolerance = g_unit_offset;

        uint32_t qty_up = (uint32_t)ceilf(grams / unit_nominal);

        uint32_t qty = 0;
        if (qty_up == 0) {
            qty = 0;
        } else {
            float unit_actual = grams / qty_up;

            if (fabs(unit_actual - unit_nominal) <= tolerance) {
                qty = qty_up;
            } else {
                qty = qty_up - 1;
            }
        }

        send_console("Weight %.2f", display_weight);

        data.weight_g = display_weight;
        data.quantity = qty;
        data.state = STATE_RUNNING;

        if (memcmp(&data, &last, sizeof(BincoData)) != 0) {
            lcd.clear();

            char line0[17];
            snprintf(line0, sizeof(line0), "%-11sID:%02u", data.name, data.ID);
            lcd.setCursor(0, 0);
            lcd.print(line0);

            char line1[17];
            snprintf(line1, sizeof(line1), "Qty:%u W:%.1f",
                     data.quantity, data.weight_g);
            lcd.setCursor(0, 1);
            lcd.print(line1);

            last = data;
        }

        uint8_t qt_ip[4] = {192,168,10,1};

        sendto(
            DATA_SOCK,
            (uint8_t*)&data,
            sizeof(data),
            qt_ip,
            5001
        );

        sleep_ms(100);
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
    printf("Scanning I2C...\n");

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        int ret = i2c_write_blocking(i2c1, addr, nullptr, 0, false);
        if (ret >= 0)
        {
            printf("Found device at 0x%02X\n", addr);
        }
    }

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

        // Reject cached packet if it is not for this Binco
        if (g_cached_cmd.ID != g_device_id) {
            CommandPacket invalid{};
            return invalid;
        }

        // Reject commands that require profile before it is loaded
        bool requires_profile =
            (g_cached_cmd.command == CMD_CALI) ||
            (g_cached_cmd.command == CMD_TARE);

        if (requires_profile && !g_profile_loaded) {
            send_console("Rejected cmd=%d, profile not loaded", g_cached_cmd.command);
            CommandPacket invalid{};
            return invalid;
        }

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
            // Ignore packets not meant for this Binco
            if (cmd.ID != g_device_id) {
                continue;
            }

            // Reject commands that require profile before it is loaded
            bool requires_profile =
                (cmd.command == CMD_CALI) ||
                (cmd.command == CMD_TARE);

            if (requires_profile && !g_profile_loaded) {
                send_console("Rejected cmd=%d, profile not loaded", cmd.command);
                continue;
            }

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

    printf("Discovery Mode\n");

    const char nm[] = "binco3";
    for(int i = 0; i <16 && nm[i]; ++i) data.name[i] = nm[i];
    data.ID = g_device_id;
    data.quantity  = 0;
    data.weight_g  = 0.0f;
    data.state     = STATE_IDLE;

    uint8_t broadcast_ip[4] = {192,168,10,1};

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

    /* after profile load, enforce ID match */
    if(out.ID != g_device_id)
        return false;

    return true;
}

static void send_console(const char* fmt, ...)
{
    ConsolePacket pkt{};
    pkt.ID = g_device_id;

    char buffer[64];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::snprintf(pkt.text, sizeof(pkt.text), "%s", buffer);

    uint8_t qt_ip[4] = {192,168,10,1};
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