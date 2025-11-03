//
// Created by Timothy James Lim on 1/11/25.
// Created a separate init as the w5500.cpp/h is the original driver from Wiznet and I do not want to modify it
//

// drivers/w5500_net.cpp
#include "pico/stdlib.h"
#include "../port/wizchip_port_pico.h"   // your port init

extern "C" {
#include "../drivers/wizchip_conf.h"
}

extern "C" int w5500_init(void){
    // 1) Low level: SPI + reset + callbacks
    wizchip_port_init();

    // 2) Allocate internal W5500 memory per socket
    uint8_t tx_size[8] = {2,2,2,2,2,2,2,2};
    uint8_t rx_size[8] = {2,2,2,2,2,2,2,2};
    if(wizchip_init(tx_size,rx_size) != 0){
        return 0;
    }

    // 3) Set network parameters
    wiz_NetInfo net = {
            .mac  = {0x00,0x08,0xDC,0x11,0x22,0x33},
            .ip   = {192,168,1,120},
            .sn   = {255,255,255,0},
            .gw   = {0,0,0,0},
            .dns  = {8,8,8,8},
            .dhcp = NETINFO_STATIC,
    };
    ctlnetwork(CN_SET_NETINFO, &net);

    // 4) Sanity: Version Check
    uint8_t ver = getVERSIONR();
    stdio_printf("W5500 version = 0x%02X\n", ver);
    if(ver!=0x04){
        return 0;
    }
    return 1;
}