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

// debug helpers
void wait_for_one(void);

// helper functions
static void init_i2c_adc();
static void fill_header(BincoHeader &hdr, MsgType type);
CommandPacket qt_instruction(uint8_t sock);

/* ================= GLOBAL STATE ================= */

static BincoState g_state = STATE_IDLE;
static uint16_t g_device_id = 1;
static uint16_t g_sequence = 0;

static int32_t g_tare_offset = 0;
static float   g_grams_per_count = 1.0f;
static float   g_manual_offset = 0.0f;

int main()
{
    stdio_init_all();
    sleep_ms(1500);

    /* LED */
    gpio_init(T_LED);
    gpio_set_dir(T_LED, GPIO_OUT);

    /* LCD */
    LCD1602 lcd;
    lcd.init();
    lcd.clear();
    lcd.print("Waiting Setup");

    /* NETWORK */
    if (!w5500_init())
        while(true);

    const uint8_t sock = 0;
    socket(sock, Sn_MR_UDP, 5001, 0);

    uint8_t dst_ip[4] = {192,168,10,1};
    uint16_t dst_port = 5001;


    /* ADC */
    init_i2c_adc();

    Adafruit_NAU7802 adc;
    if (!adc.begin(i2c1))
        while(true);

    adc.calibrate(NAU7802_CALMOD_INTERNAL);

    /* ======================= TARE ======================= */
    printf("Ensure NOTHING is on the scale.\n");
    printf("Wait for Qt Instruction...\n");
    CommandPacket cmd;
    do {
        cmd = qt_instruction(sock);
    } while(cmd.command != CMD_TARE);
    printf("CMD_TARE");

    const int TARE_SAMPLES = 64;
    int64_t tare_sum = 0;

    for (int i = 0; i < TARE_SAMPLES; i++) {
        while (!adc.available()) {}
        tare_sum += adc.read();
    }

    int32_t tare_offset = tare_sum / TARE_SAMPLES;
    printf("Tare offset = %ld counts\n", tare_offset);


    /* ======================= CALIBRATION ======================= */
    const float REF_WEIGHT = 5.3f;

    printf("Place %.2fg weight on scale.\n", REF_WEIGHT);
    printf("Wait for Qt Instruction...\n");
    do {
        cmd = qt_instruction(sock);
    } while(cmd.command != CMD_CALI);
    printf("CMD_CALI");

    int64_t cal_sum = 0;
    for (int i = 0; i < TARE_SAMPLES; i++) {
        while (!adc.available()) {}
        cal_sum += adc.read();
    }

    int32_t cal_avg = cal_sum / TARE_SAMPLES;
    int32_t delta = cal_avg - tare_offset;

    if (delta <= 0) {
        printf("Calibration failed! delta=%ld\n", delta);
        while (true) {}
    }

    float grams_per_count = REF_WEIGHT / (float) delta;
    printf("Calibration OK\n");
    printf("grams_per_count = %.8f g/count\n", grams_per_count);

    /* ===== LOOP ===== */

    while(true)
    {
        /* --- ADC READ --- */
        while(!adc.available())
            tight_loop_contents();

        int32_t raw = adc.read();

        float grams =
                (raw - g_tare_offset) * g_grams_per_count
                + g_manual_offset;

        if(grams < 0) grams = 0;

        /* --- TELEMETRY --- */
        TelemetryPacket pkt{};
        fill_header(pkt.header, MSG_TELEMETRY);

        strncpy(pkt.name, "Binco1", sizeof(pkt.name));
        pkt.weight_g = grams;
        pkt.quantity = 1;
        pkt.state    = g_state;

        sendto(sock,
               reinterpret_cast<uint8_t*>(&pkt),
               sizeof(pkt),
               dst_ip,
               dst_port);

        /* --- LCD --- */
        lcd.clear();

        char line0[17];
        snprintf(line0, sizeof(line0), "ID:%02u", g_device_id);
        lcd.setCursor(0,0);
        lcd.print(line0);

        char line1[17];
        snprintf(line1, sizeof(line1), "W:%.1fg", grams);
        lcd.setCursor(0,1);
        lcd.print(line1);

        /* --- RECEIVE COMMAND --- */
        CommandPacket cmd{};
        uint8_t src_ip[4];
        uint16_t src_port;

        int len = recvfrom(sock,
                           reinterpret_cast<uint8_t*>(&cmd),
                           sizeof(cmd),
                           src_ip,
                           &src_port);

        if(len == sizeof(CommandPacket) &&
           cmd.header.device_id == g_device_id)
        {
            switch(cmd.command)
            {
                case CMD_LED_SET:
                    gpio_put(T_LED, cmd.flag);
                    break;

                case CMD_SET_OFFSET:
                    g_manual_offset = cmd.value;
                    break;

                case CMD_TARE:
                {
                    g_state = STATE_WAIT_REFERENCE;

                    int64_t sum = 0;
                    for(int i=0;i<64;i++)
                    {
                        while(!adc.available()){}
                        sum += adc.read();
                    }

                    g_tare_offset = sum / 64;
                    break;
                }

                case CMD_REFERENCE_CAL:
                {
                    float ref_weight = cmd.value;

                    int64_t sum = 0;
                    for(int i=0;i<64;i++)
                    {
                        while(!adc.available()){}
                        sum += adc.read();
                    }

                    int32_t avg = sum / 64;
                    int32_t delta = avg - g_tare_offset;

                    if(delta > 0)
                    {
                        g_grams_per_count = ref_weight / delta;
                        g_state = STATE_RUNNING;
                    }
                    break;
                }

                default:
                    break;
            }
        }
        sleep_ms(100);
    }
}

/* ======================= HELPERS ======================= */

// Helper function, this function allows me to manually trigger the program when I am ready in serial
// Remove this from main code during actual deployment, it is only meant for debugging and development use.
void wait_for_one() {
    printf("Enter 1 to continue...\n");
    int c;
    do {
        c = getchar_timeout_us(0);
    } while (c != '1');
    printf("Continuing...\n");
}

static void init_i2c_adc() {
    i2c_init(i2c1, 400 * 1000);   // 400 kHz recommended for NAU7802
    gpio_set_function(ADC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(ADC_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(ADC_SDA);
    gpio_pull_up(ADC_SCL);
}

CommandPacket qt_instruction(uint8_t sock)
{
    CommandPacket cmd{};
    uint8_t src_ip[4];
    uint16_t src_port;

    while (true)
    {
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