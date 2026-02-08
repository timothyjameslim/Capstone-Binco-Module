#pragma once

#include <stdint.h>

/* ============================================================
   BINCO PROTOCOL VERSION
   ============================================================ */

#define BINCO_PROTOCOL_VERSION 1

/* ============================================================
   MESSAGE TYPES
   ============================================================ */

enum MsgType : uint8_t
{
    MSG_TELEMETRY = 1,
    MSG_COMMAND,
    MSG_STATUS,
    MSG_DISCOVERY
};

/* ============================================================
   COMMAND TYPES (App → BINCO)
   ============================================================ */

enum CommandType : uint8_t
{
    CMD_LED_SET = 1,
    CMD_TARE,
    CMD_CALI,
    CMD_REFERENCE_CAL,
    CMD_SET_OFFSET,
    CMD_SAVE_CONFIG
};

/* ============================================================
   DEVICE STATES
   ============================================================ */

enum BincoState : uint8_t
{
    STATE_IDLE = 0,
    STATE_WAIT_TARE,
    STATE_WAIT_REFERENCE,
    STATE_RUNNING,
    STATE_ERROR
};

/* ============================================================
   COMMON PACKET HEADER
   ============================================================ */

struct __attribute__((packed)) BincoHeader
{
    uint8_t  version;       // protocol version
    uint8_t  msg_type;      // MsgType
    uint16_t device_id;     // BINCO ID
    uint16_t sequence;      // incrementing packet counter
};

/* ============================================================
   TELEMETRY PACKET (BINCO → APP)
   ============================================================ */

struct __attribute__((packed)) TelemetryPacket
{
    BincoHeader header;

    char name[16];

    float weight_g;
    uint16_t quantity;

    BincoState state;
};

/* ============================================================
   COMMAND PACKET (APP → BINCO)
   ============================================================ */

struct __attribute__((packed)) CommandPacket
{
    BincoHeader header;

    CommandType command;

    float value;     // used for calibration weight / offset
    uint8_t flag;    // LED toggle etc
};

/* ============================================================
   STATUS PACKET (BINCO → APP)
   ============================================================ */

struct __attribute__((packed)) StatusPacket
{
    BincoHeader header;

    uint8_t status_code;
    char message[32];
};

/* ============================================================
   DISCOVERY PACKET (BINCO → APP)
   ============================================================ */

struct __attribute__((packed)) DiscoveryPacket
{
    BincoHeader header;

    char name[16];
    BincoState state;
};

/* ============================================================
   CALIBRATION DATA (FLASH STORAGE ONLY)
   ============================================================ */

struct __attribute__((packed)) CalibrationData
{
    int32_t tare_offset;
    float   grams_per_count;
    float   manual_offset;
};