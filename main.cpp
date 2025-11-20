#include "pico/stdlib.h"
#include <cstring>
#include <cstdio>
#include "pinouts.h"
#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/socket.h"
#include "drivers/lcd1602.h"
#include "drivers/Adafruit_NAU7802.h"
#include "hardware/i2c.h"
#include <cmath>

// UDP payload
struct __attribute__((packed)) BinData {
    char     name[16];
    uint16_t ID;
    uint16_t quantity;
    float    weight_g;      // now send grams directly
};

// debug helpers
void wait_for_one(void);

//helper functions
float filter_IIR(float x);
float sma20(float x);

//global variable
#define SMA_MOVING 50
float sma_buf[SMA_MOVING] = {0};
int sma_index = 0;
int sma_count = 0;

int main() {
    stdio_init_all();
    sleep_ms(500);

    LCD1602 lcd;
    lcd.init();

    // optional debug gate
    wait_for_one();

    /* ---------------------------- ADC SHIT ---------------------------- */
    // low-level ADC + high-level service
    NAU7802 nau(i2c1, ADC_SDA, ADC_SCL, 0x2A);
    float zero_offset = 0.0f;
    float scale = 1.0f;

    if (!nau.begin()) {
        printf("NAU7802 init failed\n");
        while (true) { sleep_ms(1000); }
    }
    printf("NAU7802 Working!\n");

    int32_t min_val = INT32_MAX;
    int32_t max_val = INT32_MIN;

    //nau.setGain(0);      // gain = 128 (max)
    //nau.setLDO(2);       // set LDO to 3.0V
    //nau.setRate(2);      // 40 SPS

    nau.calibrate(0);

    printf("Stabilizing...\n");

// throw away first 10 samples
    for (int i = 0; i < 50; i++) {
        while (!nau.available()) {}
        nau.read();
    }
    //printf("Measuring noise...\n");

    for (int i = 0; i < 500; i++) {
        while (!nau.available()) {}
        int32_t r = nau.read();
        float filtered = filter_IIR((float)r);
        //printf("raw=%ld filtered=%.2f\n", r, filtered);

        if (filtered < min_val) min_val = filtered;
        if (filtered > max_val) max_val = filtered;

        //printf("%ld\n", r);   // optional
    }

    printf("Noise range: %ld counts (min=%ld max=%ld)\n", max_val - min_val, min_val, max_val);

    //wait_for_one();

    printf("Taring...\n");

    zero_offset = 0;
    for (int i = 0; i < 200; i++) {
        int32_t raw = nau.read();
        float f = filter_IIR((float)raw);
        zero_offset += f;
        sleep_ms(5);
    }
    zero_offset /= 200.0f;

    printf("zero_offset = %.2f\n", zero_offset);
    printf("Place 6.56g weight...\n");
    wait_for_one();

    float cal_val = 0.0f;
    bool cal = false;
    while(!cal){
        for (int i = 0; i < 200; i++) {
            int32_t raw = nau.read();
            float f = filter_IIR((float)raw);
            cal_val += f;
            sleep_ms(5);
        }
        cal_val /= 200.0f;

        if (scale > 0.0f) {
            cal = true;   // good calibration → exit loop
        } else {
            printf("Bad calibration (scale negative). Retrying...\n");
            sleep_ms(300);
        }
    }

    float delta = cal_val - zero_offset;
    scale = 6.56f / delta;   // grams per ADC count

    printf("Calibration OK: scale = %f grams per count\n", scale);
    //wait_for_one();

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
        snprintf(line1, sizeof(line1), "Qty:%u  W:%.1f", d.quantity, d.weight_g);
        lcd.setCursor(0, 1);
        lcd.print(line1);
    }

    //----uhh weight stuff---
    const float COIN_WEIGHT = 6.56f;
    const float TOL = 0.05f;

    float lower = COIN_WEIGHT * (1.0f - TOL);
    float upper = COIN_WEIGHT * (1.0f + TOL);

    while (true) {
        // 1. get latest weight in grams (internally averages, tares, etc.)
        int32_t raw = nau.read();
        //IIR Smoothing
        float filtered = filter_IIR((float)raw);
        float net_counts = filtered - zero_offset;

        //SMA20 for stability
        float smooth_counts = sma20(net_counts);

        // Apply calibration
        float grams = smooth_counts * scale;

        // Estimate item count before rounding
        float approx_count = grams / 6.56f;

        d.weight_g = grams;
        d.quantity = approx_count;

        printf("grams=%.3f\n", d.weight_g);

        // 2. send over UDP
        sendto(sock,
               reinterpret_cast<uint8_t*>(&d),
               sizeof(d),
               dst_ip,
               dst_port);

        // 3. update LCD only if changed
        if (memcmp(&d, &last, sizeof(BinData)) != 0) {
            lcd.clear();
            char line0[17];
            snprintf(line0, sizeof(line0), "%-11sID:%02u", d.name, d.ID);
            lcd.setCursor(0, 0);
            lcd.print(line0);

            char line1[17];
            snprintf(line1, sizeof(line1), "Qty:%u  W:%.1f", d.quantity, d.weight_g);
            lcd.setCursor(0, 1);
            lcd.print(line1);

            last = d;
        }

        sleep_ms(100);  // 10 Hz loop
    }
}

// Helper function, this function allows me to manually trigger the program when I am ready in serial
// Remove this from main code during actualy deployment, it is only meant for debugging and development use.
void wait_for_one() {
    printf("Enter 1 to continue...\n");
    int c;
    do {
        c = getchar_timeout_us(0);
    } while (c != '1');
    printf("Continuing...\n");
}

float filter_IIR(float x) {
    static float y = 0.0f;  // initial state
    const float k = 0.05f;  // smoothing factor 0.01 (200 samples) 0.05 (40 samples)

    y = k * x + (1.0f - k) * y;
    return y;
}

//moving average sampler 20
float sma20(float x) {
    sma_buf[sma_index] = x;
    sma_index = (sma_index + 1) % SMA_MOVING;

    if (sma_count < SMA_MOVING) sma_count++;

    float sum = 0.0f;
    for (int i = 0; i < sma_count; i++) {
        sum += sma_buf[i];
    }
    return sum / sma_count;
}