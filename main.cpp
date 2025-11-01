#include "pico/stdlib.h"
#include "pinouts.h"
#include "port/wizchip_port_pico.h"
#include <iostream>

#include "drivers/w5500.h"
#include "drivers/w5500_net.h"
#include "drivers/wizchip_conf.h"
#include "drivers/socket.h"

//Establish the struct data to be sent over udp
struct __attribute__((packed)) BinData {
    char name[16];
    uint16_t quantity;
    uint16_t weight;
};

int main() {

    //Inits
    stdio_init_all();
    sleep_ms(500);
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    //give init some time
    if (!w5500_init()) {
        // init failed, hang here
        while(true) {
            sleep_ms(500);

        }
    }

    //open udp socket 0 on local port 5000
    const uint8_t sock = 0;
    if (socket(sock, Sn_MR_UDP, 5000, 0) != sock) {
        while (true) {
            sleep_ms(500);
        }
    }

    // destination: your PC
    uint8_t dst_ip[4] = {192,168,1,10};   // change this
    uint16_t dst_port = 5001;             // change this

    BinData d{};
    const char nm[] = "binco";
    for (int i = 0; i < 16 && nm[i]; ++i) d.name[i] = nm[i];

    d.quantity = 0;
    d.weight   = 0;

    while (true) {
        d.quantity++;
        d.weight = d.quantity * 10;

        sendto(sock,
               reinterpret_cast<uint8_t*>(&d),
               sizeof(d),
               dst_ip,
               dst_port);

        sleep_ms(500);
    }

}
