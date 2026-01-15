#ifndef NAU7802_H
#define NAU7802_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

/** Default NAU7802 I2C address */
#define NAU7802_I2CADDR_DEFAULT 0x2A

#define NAU7802_PU_CTRL        0x00
#define NAU7802_CTRL1          0x01
#define NAU7802_CTRL2          0x02
#define NAU7802_ADCO_B2        0x12
#define NAU7802_ADC            0x15
#define NAU7802_PGA            0x1B
#define NAU7802_POWER          0x1C
#define NAU7802_REVISION_ID    0x1F

typedef enum {
    NAU7802_4V5,
    NAU7802_4V2,
    NAU7802_3V9,
    NAU7802_3V6,
    NAU7802_3V3,
    NAU7802_3V0,
    NAU7802_2V7,
    NAU7802_2V4,
    NAU7802_EXTERNAL,
} NAU7802_LDOVoltage;

typedef enum {
    NAU7802_GAIN_1,
    NAU7802_GAIN_2,
    NAU7802_GAIN_4,
    NAU7802_GAIN_8,
    NAU7802_GAIN_16,
    NAU7802_GAIN_32,
    NAU7802_GAIN_64,
    NAU7802_GAIN_128,
} NAU7802_Gain;

typedef enum {
    NAU7802_RATE_10SPS  = 0,
    NAU7802_RATE_20SPS  = 1,
    NAU7802_RATE_40SPS  = 2,
    NAU7802_RATE_80SPS  = 3,
    NAU7802_RATE_320SPS = 7,
} NAU7802_SampleRate;

typedef enum {
    NAU7802_CALMOD_INTERNAL = 0,
    NAU7802_CALMOD_OFFSET  = 2,
    NAU7802_CALMOD_GAIN    = 3,
} NAU7802_Calibration;

class Adafruit_NAU7802 {
public:
    Adafruit_NAU7802();

    bool begin(i2c_inst_t *i2c_instance);
    bool reset();
    bool enable(bool flag);
    bool available();
    int32_t read();

    bool setChannel(uint8_t channel);
    bool setLDO(NAU7802_LDOVoltage voltage);
    NAU7802_LDOVoltage getLDO();
    bool setGain(NAU7802_Gain gain);
    NAU7802_Gain getGain();
    bool setRate(NAU7802_SampleRate rate);
    NAU7802_SampleRate getRate();
    bool setPGACap(bool enable);
    bool setPGABypass(bool enable);
    bool calibrate(NAU7802_Calibration mode);

private:
    i2c_inst_t *i2c = nullptr;

    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t &value);
    bool writeMasked(uint8_t reg, uint8_t value, uint8_t mask);
};

#endif