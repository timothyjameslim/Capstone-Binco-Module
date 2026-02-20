#pragma once
#include <stdint.h>

/* ===== BINCO STATE ===== */

enum BincoState : uint8_t
{
    STATE_IDLE,
    STATE_WAIT_TARE,
    STATE_WAIT_CALI,
    STATE_WAIT_REFERENCE,
    STATE_RUNNING,
    STATE_ERROR
};

/* ===== COMMAND TYPES ===== */

enum CommandType : uint8_t
{
    CMD_TARE,
    CMD_CALI,
    CMD_SETUP,
    CMD_RESTART,
    CMD_SET_OFFSET,
    CMD_LED_SET
};

/* ===== STANDARD BINCO DATA ===== */

struct __attribute__((packed)) BincoData
{
    char name[16];
    uint16_t ID;
    uint16_t quantity;
    float weight_g;
    BincoState state;
};

/* ===== COMMAND PACKET ===== */

struct __attribute__((packed)) CommandPacket
{
    uint16_t ID;

    CommandType command;

    float value;
    uint8_t flag;
};

struct __attribute__((packed)) ConsolePacket
{
    uint16_t ID;
    char text[128];
};