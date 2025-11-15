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
#include "Adafruit_NAU7802.h"
#include "adc_service.h"

// UDP payload
struct __attribute__((packed)) BinData {
    char     name[16];
    uint16_t ID;
    uint16_t quantity;
    float    weight_g;      // now send grams directly
};

// debug helpers
void wait_for_one(void);

int main() {
    stdio_init_all();
    sleep_ms(500);

    LCD1602 lcd;
    lcd.init();

    // optional debug gate
    //wait_for_one();

    // low-level ADC + high-level service
    /*NAU7802 nau(i2c1, ADC_SDA, ADC_SCL, 0x2A);
    ADCService adc(nau);

    if (!adc.init()) {
        printf("NAU7802 init failed\n");
        while (true) { sleep_ms(1000); }
    }*/

    // empty scale
    //adc.tare();
    // if you want: put known weight and call adc.calibrate_with_weight(x)

    // W5500
    if (!w5500_init()) {
        while (true) { sleep_ms(500); }
    }

    const uint8_t sock = 0;
    if (socket(sock, Sn_MR_UDP, 5001, 0) != sock) {
        while (true) { sleep_ms(500); }
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

    while (true) {
        // 1. get latest weight in grams (internally averages, tares, etc.)
        float grams = 1000;
        d.weight_g  = grams;

        printf("grams=%.2f\n", grams);

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