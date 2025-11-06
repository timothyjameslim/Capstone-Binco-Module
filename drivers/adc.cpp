//
// Created by Timothy James Lim on 6/11/25.
//

#include "adc.h"
#include "hardware/i2c.h"

// I2C Initial Settings
static constexpr i2c_inst_t *ADC_I2C = i2c1;
static constexpr uint8_t ADC_ADDR = 0x2A;



static void adc_write_reg(uint8_t reg, uint8_t val){
    // buf = | Device Address(8bit)|.....Data.....(8bit)|
    uint8_t buf[2] = {reg, val};
    // This handles the bus transaction to the specified address by blocking all other activity in the bus
    i2c_write_blocking(ADC_I2C, ADC_ADDR, buf, 2, false);
    //then it stops the transaction relinquishing control of the bus.
}

static uint8_t adc_read_reg(uint8_t reg)
{
    // first, write the register address you want to read from
    i2c_write_blocking(ADC_I2C, ADC_ADDR, &reg, 1, true);
    //it does not return control of the bus.
    uint8_t v = 0;

    // now read one byte back from that register
    i2c_read_blocking(ADC_I2C, ADC_ADDR, &v, 1, false);
    //loads all the data into v?
    // now relinquish control of the bus.
    return v;
}

void adc::init(){
    i2c_init(ADC_I2C, 400 * 1000);
    gpio_set_function(ADC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(ADC_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(ADC_SDA);
    gpio_pull_up(ADC_SCL);

    // minimal NAU7802 power-up sequence
    adc_write_reg(0x00, 0x02);   // reset
    sleep_ms(2);
    adc_write_reg(0x00, 0x01);   // power up
    sleep_ms(2);
}

void adc::clear()
{
    offset = 0;
}

void adc::tare()
{
    const int samples = 16;
    int64_t sum = 0;

    for(int i = 0; i < samples; ++i)
    {
        sum += read_raw();
        sleep_ms(5);
    }

    offset = static_cast<int32_t>(sum / samples);
}

int32_t adc::read_raw()
{
    // NAU7802: 0x12..0x14
    uint8_t start = 0x12;
    i2c_write_blocking(ADC_I2C, ADC_ADDR, &start, 1, true);

    uint8_t data[3] = {0};
    i2c_read_blocking(ADC_I2C, ADC_ADDR, data, 3, false);

    int32_t value = (static_cast<int32_t>(data[0]) << 16) |
                    (static_cast<int32_t>(data[1]) << 8)  |
                    (static_cast<int32_t>(data[2]) << 0);

    // sign extend 24-bit
    if (value & 0x800000)
    {
        value |= 0xFF000000;
    }

    value -= offset;
    return value;
}

float adc::read()
{
    int32_t raw = read_raw();
    return static_cast<float>(raw);
}

