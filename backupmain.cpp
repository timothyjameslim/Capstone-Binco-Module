#include "pico/stdlib.h"
#include <cstring>     // for memcmp
#include <cstdio>      // for snprintf
#include "pinouts.h"
#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/socket.h"
#include "drivers/lcd1602.h"
#include "drivers/Adafruit_NAU7802.h"
#include "hardware/i2c.h"


//Establish the struct data to be sent over udp
struct __attribute__((packed)) BinData {
    char     name[16];
    uint16_t ID;
    uint16_t quantity;
    uint16_t weight;
};

//debug helpers declaration
void i2c_scan_except(i2c_inst_t *bus, uint8_t skip_addr);
void wait_for_one(void);

int main() {

    stdio_init_all();
    sleep_ms(500);


    LCD1602 lcd;
    lcd.init();

    //Standby for ADC initialization and calibration
    wait_for_one();
    NAU7802 adc(i2c1, ADC_SDA, ADC_SCL, 0x2A);

    if (!adc.begin()) {
        printf("NAU7802 init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    adc.calibrate();

    // i2c_scan_except(i2c1, 0); //all addresses are found and correct

    // init W5500
    if (!w5500_init()) {
        while (true) {
            sleep_ms(500);
        }
    }

    // open udp socket 0 on local port 5000
    const uint8_t sock = 0;
    if (socket(sock, Sn_MR_UDP, 5000, 0) != sock) {
        while (true) {
            sleep_ms(500);
        }
    }

    // destination
    uint8_t  dst_ip[4] = {192, 168, 1, 121};
    uint16_t dst_port  = 5001;

    // current data
    BinData d{};
    const char nm[] = "Binco1";
    for (int i = 0; i < 16 && nm[i]; ++i) {
        d.name[i] = nm[i];
    }
    d.ID = 01;
    d.quantity = 1;
    d.weight   = 0;

    // last data for change detection
    BinData last = d;

    // initial LCD draw
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(d.name);
    lcd.setCursor(0, 1);
    {
        // first line: name (left) and ID (right)
        char line0[17];
        snprintf(line0, sizeof(line0), "%-11sID:%02u", d.name, d.ID);
        lcd.setCursor(0, 0);
        lcd.print(line0);

        // second line: quantity and weight
        char line1[17];
        snprintf(line1, sizeof(line1), "Qty:%u    W:%u", d.quantity, d.weight);
        lcd.setCursor(0, 1);
        lcd.print(line1);
    }

    while (true) {
        // 1) Read Loadcell ADC
        int32_t raw = adc.read();          // blocking until data ready
        printf("Raw Loadcell reading from ADC: %ld\n", raw);

        if (raw < 0) {
            raw = 0;                       // simple clamp for now
        }

        // 2) convert to uint16_t for packet
        uint16_t weight_u16 = (raw > 65535) ? 65535 : (uint16_t)raw;
        d.weight = weight_u16;

        sleep_ms(1000);

        // send struct over UDP
        sendto(sock,
               reinterpret_cast<uint8_t*>(&d),
               sizeof(d),
               dst_ip,
               dst_port);

        // update LCD only if struct changed
        if (memcmp(&d, &last, sizeof(BinData)) != 0) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(d.name);
            lcd.setCursor(0, 1);
            char line[17];
            snprintf(line, sizeof(line), "Q:%u W:%u", d.quantity, d.weight);
            lcd.print(line);
            last = d;
        }
    }
}

//------- Debug Helpers --------

// scan i2c bus and skip one address
void i2c_scan_except(i2c_inst_t *bus, uint8_t skip_addr) {
    printf("I2C scan (skip 0x%02X):\n", skip_addr);
    for (uint8_t addr = 1; addr < 0x7F; ++addr) {
        if (addr == skip_addr) {
            continue;
        }
        uint8_t dummy;
        int res = i2c_read_blocking(bus, addr, &dummy, 1, false);
        if (res >= 0) {
            printf("  found: 0x%02X\n", addr);
        }
    }
    printf("scan done.\n");
}

//real-time debugger support (comment out when not using)
void wait_for_one() {
    printf("Enter 1 to continue...\n");
    int c;
    do {
        c = getchar_timeout_us(0);
    } while (c != '1');
    printf("Continuing...\n");
}