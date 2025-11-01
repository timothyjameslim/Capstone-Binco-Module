// port/wizchip_port_pico.cpp
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "../pinouts.h"
extern "C" {
#include "../drivers/wizchip_conf.h"
}

extern "C" {

// global for IRQ save
static uint32_t irq_save = 0;

void crit_enter(void) {
    irq_save = save_and_disable_interrupts();
}

void crit_exit(void) {
    restore_interrupts(irq_save);
}

void cs_select(void) {
    gpio_put(PIN_WIZNET_CS, 0);
}

void cs_deselect(void) {
    gpio_put(PIN_WIZNET_CS, 1);
}

uint8_t spi_rb(void) {
    uint8_t rx = 0;
    spi_read_blocking(WIZNET_SPI, 0x00, &rx, 1);
    return rx;
}

void spi_wb(uint8_t b) {
    spi_write_blocking(WIZNET_SPI, &b, 1);
}

// THIS was missing extern "C" before
void wizchip_port_init(void) {
    // SPI
    spi_init(WIZNET_SPI, 10 * 1000 * 1000);         //10MHz
    gpio_set_function(PIN_WIZNET_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_WIZNET_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_WIZNET_SCK,  GPIO_FUNC_SPI);

    // CS
    gpio_init(PIN_WIZNET_CS);
    gpio_set_dir(PIN_WIZNET_CS, GPIO_OUT);
    gpio_put(PIN_WIZNET_CS, 1);

    // RESET
    gpio_init(PIN_WIZNET_RST);
    gpio_set_dir(PIN_WIZNET_RST, GPIO_OUT);
    gpio_put(PIN_WIZNET_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_WIZNET_RST, 1);
    sleep_ms(200);

    // INT (optional)
    gpio_init(PIN_WIZNET_INT);
    gpio_set_dir(PIN_WIZNET_INT, GPIO_IN);
    gpio_pull_up(PIN_WIZNET_INT);

    // register callbacks
    reg_wizchip_cris_cbfunc(crit_enter, crit_exit);
    reg_wizchip_cs_cbfunc(cs_select, cs_deselect);
    reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
}

} // extern "C"
