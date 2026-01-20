/**************************************************************************/
/*!
  @file     Adafruit_NAU7802.cpp

  @mainpage NAU7802 I2C 24-bit ADC Driver (Pico SDK)

  @section intro Introduction

  This driver is a **Pico SDK–based rewrite** of the original
  Adafruit NAU7802 I2C 24-bit ADC library, adapted for use in
  **non-Arduino environments** (e.g. CLion + RP2040 Pico SDK).

  The original Arduino implementation targets the Adafruit NAU7802
  breakout board:
  https://www.adafruit.com/products/4538

  This revision removes Arduino and Adafruit BusIO dependencies and
  implements direct I²C access using `hardware/i2c.h`. GPIO and I²C
  pin configuration are intentionally handled by the application
  layer to keep the driver hardware-agnostic.

  @section authors Authors

  Original Author:
  Limor Fried (Adafruit Industries)

  Pico SDK Rewrite & Maintenance:
  Timothy James Lim

  @section notes Notes

  This file is a **modified derivative work** of the original Adafruit
  NAU7802 Arduino library. Functionality and register behavior are
  preserved where possible, while the implementation has been
  refactored for deterministic timing and compatibility with
  RP2040-based systems.

  Original Github:
  https://github.com/adafruit/Adafruit_NAU7802

*/
/**************************************************************************/

#include "Adafruit_NAU7802.h"
#include "hardware/i2c.h"

/*!
    @brief  Instantiates a new NAU7802 class
*/
/**************************************************************************/
Adafruit_NAU7802::Adafruit_NAU7802() {}

/**************************************************************************/
/*!
    @brief  Sets up the I2C connection and tests that the sensor was found.
    @param i2c_instance Pointer to RP2040 I2C peripheral (i2c0 / i2c1)
    @return true if sensor was found, otherwise false.
*/
/**************************************************************************/
bool Adafruit_NAU7802::begin(i2c_inst_t *i2c_instance) {
    i2c = i2c_instance;

    if (!reset()) return false;
    if (!enable(true)) return false;

    uint8_t rev;
    if (!readReg(NAU7802_REVISION_ID, rev)) return false;
    if ((rev & 0x0F) != 0x0F) return false;

    if (!setLDO(NAU7802_3V0)) return false;
    if (!setGain(NAU7802_GAIN_128)) return false;
    if (!setRate(NAU7802_RATE_10SPS)) return false;


    if (!writeMasked(NAU7802_ADC, 0b11 << 4, 0b11 << 4)) return false;   // Disable chopper
    if (!writeMasked(NAU7802_PGA, 0 << 6, 1 << 6)) return false;        // Low ESR caps

    return true;
}

/**************************************************************************/
/*!
    @brief  Whether to have the sensor enabled and working or in power down mode
    @param  flag True to be in powered mode, False for power down mode
    @return False if something went wrong with I2C comms
*/
/**************************************************************************/
bool Adafruit_NAU7802::enable(bool flag) {
    if (!flag) {
        writeMasked(NAU7802_PU_CTRL, 0, (1 << 2) | (1 << 1));
        return true;
    }

    writeMasked(NAU7802_PU_CTRL, (1 << 1) | (1 << 2),
                (1 << 1) | (1 << 2));
    sleep_ms(600);
    writeMasked(NAU7802_PU_CTRL, 1 << 4, 1 << 4);

    uint8_t reg;
    readReg(NAU7802_PU_CTRL, reg);
    return reg & (1 << 3);
}

/**************************************************************************/
/*!
    @brief Whether there is new ADC data to read
    @return True when there's new data available
*/
/**************************************************************************/
bool Adafruit_NAU7802::available() {
    uint8_t reg;
    readReg(NAU7802_PU_CTRL, reg);
    return reg & (1 << 5);
}

/**************************************************************************/
/*!
    @brief  Set which channel for ADC
    @param channel Set to 0 for CH1, 1 for CH2
    @returns False if any I2C error occured
*/
/**************************************************************************/
bool Adafruit_NAU7802::setChannel(uint8_t channel) {
    if (channel > 1) channel = 1;
    return writeMasked(NAU7802_CTRL2, channel << 7, 1 << 7);
}

/**************************************************************************/
/*!
    @brief Read the stored 24-bit ADC output value.
    @return Signed integer with ADC output result, extended to a int32_t
*/
/**************************************************************************/

int32_t Adafruit_NAU7802::read() {
    uint8_t reg = NAU7802_ADCO_B2;
    uint8_t buf[3];

    i2c_write_blocking(i2c, NAU7802_I2CADDR_DEFAULT, &reg, 1, true);
    i2c_read_blocking(i2c, NAU7802_I2CADDR_DEFAULT, buf, 3, false);

    int32_t val = (buf[0] << 16) | (buf[1] << 8) | buf[2];
    if (val & 0x800000) val |= 0xFF000000;
    return val;
}

/**************************************************************************/
/*!
    @brief Perform a soft reset
    @return False if there was any I2C comms error
*/
/**************************************************************************/
bool Adafruit_NAU7802::reset() {
    writeMasked(NAU7802_PU_CTRL, 1 << 0, 1 << 0);
    sleep_ms(10);
    writeMasked(NAU7802_PU_CTRL, 0 << 0, 1 << 0);
    writeMasked(NAU7802_PU_CTRL, 1 << 1, 1 << 1);
    sleep_ms(1);

    uint8_t ready;
    readReg(NAU7802_PU_CTRL, ready);
    return ready & (1 << 3);
}

/**************************************************************************/
/*!
    @brief  The desired LDO voltage setter
    @param voltage The LDO setting: NAU7802_4V5, NAU7802_4V2, NAU7802_3V9,
    NAU7802_3V6, NAU7802_3V3, NAU7802_3V0, NAU7802_2V7, NAU7802_2V4, or
    NAU7802_EXTERNAL if we are not using the internal LDO
    @return False if there was any I2C comms error
*/
/**************************************************************************/
bool Adafruit_NAU7802::setLDO(NAU7802_LDOVoltage voltage) {
    if (voltage == NAU7802_EXTERNAL)
        return writeMasked(NAU7802_PU_CTRL, 0, 1 << 7);

    writeMasked(NAU7802_PU_CTRL, 1 << 7, 1 << 7);
    return writeMasked(NAU7802_CTRL1, voltage << 3, 0b111 << 3);
}

/**************************************************************************/
/*!
    @brief  The desired LDO voltage getter
    @returns The voltage setting: NAU7802_4V5, NAU7802_4V2, NAU7802_3V9,
    NAU7802_3V6, NAU7802_3V3, NAU7802_3V0, NAU7802_2V7, NAU7802_2V4, or
    NAU7802_EXTERNAL if we are not using the internal LDO
*/
/**************************************************************************/
NAU7802_LDOVoltage Adafruit_NAU7802::getLDO() {
    uint8_t pu, ctrl1;
    readReg(NAU7802_PU_CTRL, pu);
    if (!(pu & (1 << 7))) return NAU7802_EXTERNAL;
    readReg(NAU7802_CTRL1, ctrl1);
    return (NAU7802_LDOVoltage)((ctrl1 >> 3) & 0x07);
}

/**************************************************************************/
/*!
    @brief  The desired ADC gain setter
    @param  gain Desired gain: NAU7802_GAIN_1, NAU7802_GAIN_2, NAU7802_GAIN_4,
    NAU7802_GAIN_8, NAU7802_GAIN_16, NAU7802_GAIN_32, NAU7802_GAIN_64,
    or NAU7802_GAIN_128
    @returns False if there was any error during I2C comms
*/
/**************************************************************************/
bool Adafruit_NAU7802::setGain(NAU7802_Gain gain) {
    return writeMasked(NAU7802_CTRL1, gain, 0b111);
}

/**************************************************************************/
/*!
    @brief  The desired ADC gain getter
    @returns The gain: NAU7802_GAIN_1, NAU7802_GAIN_2, NAU7802_GAIN_4,
    NAU7802_GAIN_8, NAU7802_GAIN_16, NAU7802_GAIN_32, NAU7802_GAIN_64,
    or NAU7802_GAIN_128
*/
/**************************************************************************/
NAU7802_Gain Adafruit_NAU7802::getGain() {
    uint8_t reg;
    readReg(NAU7802_CTRL1, reg);
    return (NAU7802_Gain)(reg & 0b111);
}

/**************************************************************************/
/*!
    @brief  The desired conversion rate setter
    @param rate The desired rate: NAU7802_RATE_10SPS, NAU7802_RATE_20SPS,
    NAU7802_RATE_40SPS, NAU7802_RATE_80SPS, or NAU7802_RATE_320SPS
    @returns False if any I2C error occured
*/
/**************************************************************************/
bool Adafruit_NAU7802::setRate(NAU7802_SampleRate rate) {
    return writeMasked(NAU7802_CTRL2, rate << 4, 0b111 << 4);
}

/**************************************************************************/
/*!
    @brief  The desired conversion rate getter
    @returns The rate: NAU7802_RATE_10SPS, NAU7802_RATE_20SPS,
    NAU7802_RATE_40SPS, NAU7802_RATE_80SPS, or NAU7802_RATE_320SPS
*/
/**************************************************************************/
NAU7802_SampleRate Adafruit_NAU7802::getRate() {
    uint8_t reg;
    readReg(NAU7802_CTRL2, reg);
    return (NAU7802_SampleRate)((reg >> 4) & 0b111);
}

/**************************************************************************/
/*!
    @brief  Enable or disable optional PGA filters. NOTE - this should only
    be used for single channel operation.
    @param enable Use true to enable or false to disable.
    @returns False if any I2C error occured
*/
/**************************************************************************/
bool Adafruit_NAU7802::setPGACap(bool enable) {
    return writeMasked(NAU7802_POWER, enable << 7, 1 << 7);
}

/**************************************************************************/
/*!
    @brief Enable or disable optional PGA bypass.
    @param enable Use true to enable or false to disable
    @return False if any I2C error occurred
*/
/**************************************************************************/
bool Adafruit_NAU7802::setPGABypass(bool enable) {
    return writeMasked(NAU7802_PGA, enable << 4, 1 << 4);
}

/**************************************************************************/
/*!
    @brief  Perform the internal calibration procedure
    @param mode The calibration mode to perform: NAU7802_CALMOD_INTERNAL,
    NAU7802_CALMOD_OFFSET or NAU7802_CALMOD_GAIN
    @returns True on calibrations success
*/
/**************************************************************************/
bool Adafruit_NAU7802::calibrate(NAU7802_Calibration mode) {
    writeMasked(NAU7802_CTRL2, mode, 0b11);
    writeMasked(NAU7802_CTRL2, 1 << 2, 1 << 2);

    while (true) {
        uint8_t reg;
        readReg(NAU7802_CTRL2, reg);
        if (!(reg & (1 << 2)))
            return !(reg & (1 << 3));
        sleep_ms(10);
    }
}

bool Adafruit_NAU7802::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    return i2c_write_blocking(
            i2c,
            NAU7802_I2CADDR_DEFAULT,
            buf,
            2,
            false
    ) == 2;
}

bool Adafruit_NAU7802::readReg(uint8_t reg, uint8_t &value) {
    if (i2c_write_blocking(
            i2c,
            NAU7802_I2CADDR_DEFAULT,
            &reg,
            1,
            true
    ) != 1) {
        return false;
    }

    return i2c_read_blocking(
            i2c,
            NAU7802_I2CADDR_DEFAULT,
            &value,
            1,
            false
    ) == 1;
}

bool Adafruit_NAU7802::writeMasked(uint8_t reg,
                                   uint8_t value,
                                   uint8_t mask) {
    uint8_t current;
    if (!readReg(reg, current)) {
        return false;
    }

    current &= ~mask;
    current |= (value & mask);

    return writeReg(reg, current);
}