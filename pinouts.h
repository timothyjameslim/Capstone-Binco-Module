//
// Created by Timothy James Lim on 1/11/25.
//

#ifndef BINCO_PINOUTS_H
#define BINCO_PINOUTS_H

// Board: W5500-EVB-PICO (RP2040 + W5500)

// SPI for W5500
#define WIZNET_SPI         spi0
#define PIN_WIZNET_MISO    16
#define PIN_WIZNET_CS      17
#define PIN_WIZNET_SCK     18
#define PIN_WIZNET_MOSI    19

// W5500 control pins
#define PIN_WIZNET_RST     20
#define PIN_WIZNET_INT     21

// On-board LED
#define PIN_LED            25

// VBUS / power sense (optional)
#define PIN_VBUS_SENSE     24
#define PIN_ADC_VSYS       29

#endif //BINCO_PINOUTS_H
