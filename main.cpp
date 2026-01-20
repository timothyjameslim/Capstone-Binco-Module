#include "pico/stdlib.h"
#include <cstring>
#include <cstdio>
#include <cmath>

#include "pinouts.h"
#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/socket.h"
#include "drivers/lcd1602.h"
#include "drivers/Adafruit_NAU7802.h"
#include "hardware/i2c.h"

// UDP payload
struct __attribute__((packed)) BinData {
    char     name[16];
    uint16_t ID;
    uint16_t quantity;
    float    weight_g;      // now send grams directly
};

// debug helpers
void wait_for_one(void);

// helper functions
static void init_i2c_adc();

// ======================= MAIN =======================
int main() {
    stdio_init_all();
    sleep_ms(1500);

    LCD1602 lcd;
    lcd.init();

    // optional debug gate
    wait_for_one();

    /* ---------------------------- ADC SHIT ---------------------------- */
    // low-level ADC only (no service abstraction)

    init_i2c_adc();

    Adafruit_NAU7802 adc;

    if (!adc.begin(i2c1)) {
        printf("NAU7802 init failed\n");
        while (true) { sleep_ms(1000); }
    }

    adc.calibrate(NAU7802_CALMOD_INTERNAL);
    printf("NAU7802 ready\n");

    // discard early unstable samples
    for (int i = 0; i < 20; i++) {
        while (!adc.available()) {}
        adc.read();
    }

    /* ======================= TARE ======================= */
    printf("Ensure NOTHING is on the scale.\n");
    printf("Press 1 to tare...\n");
    wait_for_one();

    const int TARE_SAMPLES = 64;
    int64_t tare_sum = 0;

    for (int i = 0; i < TARE_SAMPLES; i++) {
        while (!adc.available()) {}
        tare_sum += adc.read();
    }

    int32_t tare_offset = tare_sum / TARE_SAMPLES;
    printf("Tare offset = %ld counts\n", tare_offset);

    /* ======================= CALIBRATION ======================= */
    const float REF_WEIGHT = 6.56f;

    printf("Place %.2fg weight on scale.\n", REF_WEIGHT);
    printf("Press 1 to calibrate...\n");
    wait_for_one();

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

    float grams_per_count = REF_WEIGHT / (float)delta;
    printf("Calibration OK\n");
    printf("grams_per_count = %.8f g/count\n", grams_per_count);

    /* ---------------------------- W5500 ---------------------------- */
    if (!w5500_init()) {
        while (true) { sleep_ms(250); }
    }

    const uint8_t sock = 0;
    if (socket(sock, Sn_MR_UDP, 5001, 0) != sock) {
        while (true) { sleep_ms(250); }
    }

    uint8_t  dst_ip[4] = {192, 168, 10, 1};
    uint16_t dst_port  = 5001;

    BinData d{};
    const char nm[] = "Binco1";
    for (int i = 0; i < 16 && nm[i]; ++i) d.name[i] = nm[i];
    d.ID       = 1;
    d.quantity = 1;
    d.weight_g = 0.0f;

    BinData last = d;

    // initial LCD
    {
        char line0[17];
        snprintf(line0, sizeof(line0), "%-11sID:%02u", d.name, d.ID);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(line0);

        char line1[17];
        snprintf(line1, sizeof(line1), "Qty:%u  W:%.1f",
                 d.quantity, d.weight_g);
        lcd.setCursor(0, 1);
        lcd.print(line1);
    }

    /* ======================= RUNTIME ======================= */
    while (true) {
        while (!adc.available()) {
            tight_loop_contents();
        }

        int32_t raw = adc.read();
        float grams = (raw - tare_offset) * grams_per_count;

        // clamp tiny noise only
        if (grams < 0.0f) grams = 0.0f;

        d.weight_g = grams;

        printf("raw grams = %.3f\n", grams);

        // send over UDP
        sendto(sock,
               reinterpret_cast<uint8_t*>(&d),
               sizeof(d),
               dst_ip,
               dst_port);

        // update LCD only if changed
        if (memcmp(&d, &last, sizeof(BinData)) != 0) {
            lcd.clear();

            char line0[17];
            snprintf(line0, sizeof(line0), "%-11sID:%02u", d.name, d.ID);
            lcd.setCursor(0, 0);
            lcd.print(line0);

            char line1[17];
            snprintf(line1, sizeof(line1), "Qty:%u  W:%.1f",
                     d.quantity, d.weight_g);
            lcd.setCursor(0, 1);
            lcd.print(line1);

            last = d;
        }

        sleep_ms(100);  // 10 Hz loop
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