/**************************************************************************/
/*!
  @file     Adafruit_NAU7802.cpp
  @brief    Minimal NAU7802 24-bit ADC driver for Raspberry Pi Pico (Pico SDK)
            Pico-SDK rewrite of the Adafruit NAU7802 driver style.
            Uses hardware/i2c.h and board pin mappings from pinouts.h.
*/
/**************************************************************************/

#include "Adafruit_NAU7802.h"
#include "hardware/i2c.h"
#include "../pinouts.h"

/**************************************************************************/
/*!
    @brief  NAU7802 register map (subset)
*/
/**************************************************************************/
static constexpr uint8_t REG_PU_CTRL     = 0x00;  ///< Power-up / status
static constexpr uint8_t REG_CTRL1       = 0x01;  ///< Gain, LDO
static constexpr uint8_t REG_CTRL2       = 0x02;  ///< Sample rate, cal
static constexpr uint8_t REG_ADCO_B2     = 0x12;  ///< ADC output MSB (3 bytes total)
static constexpr uint8_t REG_ADC         = 0x15;  ///< ADC / chopper control
static constexpr uint8_t REG_PGA         = 0x1B;  ///< PGA control
static constexpr uint8_t REG_POWER       = 0x1C;  ///< Power control
static constexpr uint8_t REG_REVISION_ID = 0x1F;  ///< Revision, low nibble = 0xF

/**************************************************************************/
/*!
    @brief  Construct a new NAU7802 driver object
    @param  bus  Pico I2C instance (i2c0 or i2c1)
    @param  sda  GPIO number for SDA
    @param  scl  GPIO number for SCL
    @param  addr 7-bit I2C address, default 0x2A
*/
/**************************************************************************/
NAU7802::NAU7802(i2c_inst_t *bus, uint sda, uint scl, uint8_t addr)
        : bus_(bus), sda_(sda), scl_(scl), addr_(addr) {}

/**************************************************************************/
/*!
    @brief  Low-level helper to write 1 byte to a register
    @param  reg Register address
    @param  val Value to write
    @return true on success, false on I2C error
*/
/**************************************************************************/
bool NAU7802::writeReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    int res = i2c_write_blocking(bus_, addr_, buf, 2, false);
    return (res == 2);
}

/**************************************************************************/
/*!
    @brief  Low-level helper to read 1 byte from a register
    @param  reg Register address
    @param  val Reference to store read value
    @return true on success, false on I2C error
*/
/**************************************************************************/
bool NAU7802::readReg(uint8_t reg, uint8_t &val) {
    // write register address, keep bus
    if (i2c_write_blocking(bus_, addr_, &reg, 1, true) < 0) {
        return false;
    }
    // read single byte, release bus
    if (i2c_read_blocking(bus_, addr_, &val, 1, false) < 0) {
        return false;
    }
    return true;
}

/**************************************************************************/
/*!
    @brief  Perform the NAU7802 reset sequence (RR bit)
    @return true if device reports ready, false otherwise
*/
/**************************************************************************/
bool NAU7802::reset() {
    uint8_t pu = 0;

    // read current PU_CTRL
    if (!readReg(REG_PU_CTRL, pu)) {
        return false;
    }
    // set RR (bit0) to reset all registers
    if (!writeReg(REG_PU_CTRL, pu | 0x01)) {
        return false;
    }
    sleep_ms(10);

    // clear RR, set PUD (bit1) to bring up digital
    if (!writeReg(REG_PU_CTRL, 0x02)) {
        return false;
    }
    sleep_ms(1);

    // read back to check PU_READY (bit3)
    if (!readReg(REG_PU_CTRL, pu)) {
        return false;
    }
    return (pu & 0x08) != 0;
}

/**************************************************************************/
/*!
    @brief  Power up or power down the NAU7802
    @param  on true to enable analog+digital, false to power down
    @return true on success
*/
/**************************************************************************/
bool NAU7802::enable(bool on) {
    uint8_t pu = 0;
    if (!readReg(REG_PU_CTRL, pu)) {
        return false;
    }

    if (!on) {
        // clear analog (bit2) and digital (bit1)
        pu &= ~(1 << 2);
        pu &= ~(1 << 1);
        return writeReg(REG_PU_CTRL, pu);
    }

    // turn on digital and analog
    pu |= (1 << 1); // PUD
    pu |= (1 << 2); // PUA
    if (!writeReg(REG_PU_CTRL, pu)) {
        return false;
    }

    // wait for analog to settle
    sleep_ms(600);

    // set PU_START (bit4)
    pu |= (1 << 4);
    if (!writeReg(REG_PU_CTRL, pu)) {
        return false;
    }

    // confirm PU_READY (bit3)
    if (!readReg(REG_PU_CTRL, pu)) {
        return false;
    }
    return (pu & (1 << 3)) != 0;
}

/**************************************************************************/
/*!
    @brief  Initialize I2C pins (from pinouts.h) and configure the NAU7802
    @return true if device is detected and configured, false otherwise
*/
/**************************************************************************/
bool NAU7802::begin() {
    // init I2C on board-defined ADC pins
    i2c_init(bus_, 400000);
    gpio_set_function(ADC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(ADC_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(ADC_SDA);
    gpio_pull_up(ADC_SCL);

    // reset and enable
    if (!reset()) {
        return false;
    }
    if (!enable(true)) {
        return false;
    }

    // check revision ID, low nibble should be 0xF
    uint8_t rev = 0;
    if (!readReg(REG_REVISION_ID, rev)) {
        return false;
    }
    if ((rev & 0x0F) != 0x0F) {
        return false;
    }

    // set LDO to 3.0V and gain to 128
    uint8_t ctrl1 = 0;
    if (!readReg(REG_CTRL1, ctrl1)) {
        return false;
    }

    // LDO bits [5:3] = 0b101 (3.0V)
    ctrl1 &= ~(0x07 << 3);
    ctrl1 |= (0x05 << 3);

    // gain bits [2:0] = 0b111 (128)
    ctrl1 &= ~0x07;
    ctrl1 |= 0x07;

    if (!writeReg(REG_CTRL1, ctrl1)) {
        return false;
    }

    // sample rate to 10SPS: CTRL2 bits [6:4] = 0
    uint8_t ctrl2 = 0;
    if (!readReg(REG_CTRL2, ctrl2)) {
        return false;
    }
    ctrl2 &= ~(0x07 << 4);
    if (!writeReg(REG_CTRL2, ctrl2)) {
        return false;
    }

    return true;
}

/**************************************************************************/
/*!
    @brief  Check whether the ADC has a fresh conversion ready
    @return true if ready (PU_CTRL bit5 = 1), false if not ready or I2C error
*/
/**************************************************************************/
bool NAU7802::available() {
    uint8_t pu = 0;
    if (!readReg(REG_PU_CTRL, pu)) {
        return false;
    }
    // CR bit = bit5
    return (pu & (1 << 5)) != 0;
}

/**************************************************************************/
/*!
    @brief  Read the 24-bit conversion result from the ADC
    @note   This function blocks until data is ready.
    @return Signed 32-bit value (24-bit sign-extended)
*/
/**************************************************************************/
int32_t NAU7802::read() {
    // wait for data ready
    while (!available()) {
        sleep_ms(1);
    }

    // request 3 data bytes starting at ADCO_B2
    uint8_t start_reg = REG_ADCO_B2;
    uint8_t data[3] = {0, 0, 0};

    // write register pointer, keep bus
    i2c_write_blocking(bus_, addr_, &start_reg, 1, true);
    // read 3 bytes, release bus
    i2c_read_blocking(bus_, addr_, data, 3, false);

    // combine into 24-bit
    int32_t val = (static_cast<int32_t>(data[0]) << 16) |
                  (static_cast<int32_t>(data[1]) << 8)  |
                  (static_cast<int32_t>(data[2]) << 0);

    // sign-extend from 24 to 32 bits
    if (val & 0x800000) {
        val |= 0xFF000000;
    }

    return val;
}

/**************************************************************************/
/*!
    @brief  Run one of the internal calibration modes
    @param  mode 0 = internal, 2 = offset, 3 = gain (see datasheet)
    @return true on success, false on I2C error or cal error
*/
/**************************************************************************/
bool NAU7802::calibrate(uint8_t mode) {
    uint8_t ctrl2 = 0;
    if (!readReg(REG_CTRL2, ctrl2)) {
        return false;
    }

    // set CALMOD bits [1:0]
    ctrl2 &= ~0x03;
    ctrl2 |= (mode & 0x03);

    // set CAL_START bit2
    ctrl2 |= (1 << 2);
    if (!writeReg(REG_CTRL2, ctrl2)) {
        return false;
    }

    // wait for CAL_START to clear
    do {
        sleep_ms(10);
        if (!readReg(REG_CTRL2, ctrl2)) {
            return false;
        }
    } while (ctrl2 & (1 << 2));

    // check CAL_ERR bit3
    return (ctrl2 & (1 << 3)) == 0;
}

/**************************************************************************/
/*!
    @brief  Set the ADC sample rate
    @param  rate 3-bit value placed in CTRL2[6:4]
    @return true on success
*/
/**************************************************************************/
bool NAU7802::setRate(uint8_t rate) {
    uint8_t ctrl2 = 0;
    if (!readReg(REG_CTRL2, ctrl2)) {
        return false;
    }
    ctrl2 &= ~(0x07 << 4);
    ctrl2 |= ((rate & 0x07) << 4);
    return writeReg(REG_CTRL2, ctrl2);
}

/**************************************************************************/
/*!
    @brief  Set the ADC gain
    @param  gain 3-bit value placed in CTRL1[2:0]
    @return true on success
*/
/**************************************************************************/
bool NAU7802::setGain(uint8_t gain) {
    uint8_t ctrl1 = 0;
    if (!readReg(REG_CTRL1, ctrl1)) {
        return false;
    }
    ctrl1 &= ~0x07;
    ctrl1 |= (gain & 0x07);
    return writeReg(REG_CTRL1, ctrl1);
}

/**************************************************************************/
/*!
    @brief  Set the internal LDO voltage
    @param  ldo 3-bit value placed in CTRL1[5:3]
    @return true on success
*/
/**************************************************************************/
bool NAU7802::setLDO(uint8_t ldo) {
    uint8_t ctrl1 = 0;
    if (!readReg(REG_CTRL1, ctrl1)) {
        return false;
    }
    ctrl1 &= ~(0x07 << 3);
    ctrl1 |= ((ldo & 0x07) << 3);
    return writeReg(REG_CTRL1, ctrl1);
}