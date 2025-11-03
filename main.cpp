#include "pico/stdlib.h"
#include <cstring>     // for memcmp
#include <cstdio>      // for snprintf
#include "pinouts.h"
#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/socket.h"
#include "drivers/lcd1602.h"

//Establish the struct data to be sent over udp
struct __attribute__((packed)) BinData {
    char     name[16];
    uint16_t ID;
    uint16_t quantity;
    uint16_t weight;
};

int main() {
    stdio_init_all();
    sleep_ms(500);

    LCD1602 lcd;
    lcd.init();

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
    d.quantity = 999;
    d.weight   = 999;

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