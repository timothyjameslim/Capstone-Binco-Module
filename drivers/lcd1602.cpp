//
// Created by Timothy James Lim on 2/11/25.
//

#include "lcd1602.h"
#include "hardware/i2c.h"

// I2C
static constexpr i2c_inst_t *LCD_I2C = i2c1;
// change if your scan shows 0x3F or 0x20
static constexpr uint8_t LCD_ADDR = 0x27;

LCD1602::LCD1602()
        : addr_(LCD_ADDR)
{}

void LCD1602::init() {
    // init I2C1 on GP14/GP15
    i2c_init(LCD_I2C, 100000);
    gpio_set_function(LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_SDA);
    gpio_pull_up(LCD_SCL);

    sleep_ms(50);

    // HD44780 4-bit init sequence (PCF8574)
    write4bits(0x30, false);
    sleep_ms(5);
    write4bits(0x30, false);
    sleep_ms(5);
    write4bits(0x30, false);
    sleep_ms(5);
    write4bits(0x20, false);   // 4-bit
    sleep_ms(5);

    writeCmd(0x28); // function set: 4-bit, 2 line, 5x8
    writeCmd(0x0C); // display ON, cursor OFF, blink OFF
    writeCmd(0x06); // entry mode: inc, no shift
    writeCmd(0x01); // clear
    sleep_ms(2);
}

void LCD1602::clear() {
    writeCmd(0x01);
    sleep_ms(2);
}

void LCD1602::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[2] = {0x00, 0x40};
    if (row > 1) row = 1;
    writeCmd(0x80 | (row_offsets[row] + col));
}

void LCD1602::print(const char *s) {
    while (*s) {
        writeData(static_cast<uint8_t>(*s++));
    }
}

void LCD1602::print(const std::string &s) {
    for (char c : s) {
        writeData(static_cast<uint8_t>(c));
    }
}

void LCD1602::writeCmd(uint8_t cmd) {
    send(cmd, false);
}

void LCD1602::writeData(uint8_t data) {
    send(data, true);
}

// ---------------- PCF8574 low-level ----------------

void LCD1602::expanderWrite(uint8_t data) {
    // keep backlight on (BIT3 = 0x08)
    uint8_t d = data | 0x08;
    i2c_write_blocking(LCD_I2C, addr_, &d, 1, false);
}

void LCD1602::pulseEnable(uint8_t data) {
    expanderWrite(data | 0x04); // EN=1
    sleep_us(1);
    expanderWrite(data & ~0x04); // EN=0
    sleep_us(50);
}

void LCD1602::write4bits(uint8_t value, bool rs) {
    uint8_t data = (value & 0xF0);
    if (rs) data |= 0x01;       // RS
    expanderWrite(data);
    pulseEnable(data);
}

void LCD1602::send(uint8_t value, bool rs) {
    // high nibble
    write4bits(value & 0xF0, rs);
    // low nibble
    write4bits((value << 4) & 0xF0, rs);
}