# System Overview

This document explains how Binco firmware is structured and how data flows from sensor to network.

## High-level architecture

The firmware has four primary responsibilities:

1. Hardware bring-up (serial, LCD, I2C ADC, Ethernet)
2. Scale calibration (tare + reference weight)
3. Continuous sampling and conversion to grams
4. Output updates (UDP telemetry + LCD)

## Runtime flow (`main.cpp`)

1. Initialize stdio and LCD.
2. Optional debug gate (`wait_for_one`).
3. Initialize I2C and NAU7802 ADC.
4. Discard unstable startup samples.
5. Tare routine:
   - collect average counts with empty scale
   - store as `tare_offset`
6. Calibration routine:
   - read counts with known reference mass
   - compute `grams_per_count`
7. Initialize W5500 and open UDP socket.
8. Enter infinite loop:
   - read ADC sample
   - compute `grams = (raw - tare_offset) * grams_per_count`
   - clamp negative noise to zero
   - update `BinData.weight_g`
   - send UDP payload via `sendto`
   - refresh LCD only when payload changed

## Data model and protocol

The UDP payload is the packed struct:

- `name[16]`
- `ID` (`uint16_t`)
- `quantity` (`uint16_t`)
- `weight_g` (`float`)

Because this is binary UDP payload (not JSON), sender and receiver must agree on field order, type widths, and packing.

## Networking model

- W5500 network stack is initialized in `drivers/w5500_net.cpp`.
- IP setup is currently static (`NETINFO_STATIC`).
- Device sends UDP packets to `dst_ip:dst_port` configured in `main.cpp`.

## Where to modify for multi-bin deployment

### Per-bin identity (application layer)

In `main.cpp`:

- `const char nm[] = "Binco1";`
- `d.ID = ...;`
- `d.quantity = ...;`

These affect on-screen label and telemetry metadata.

### Per-bin network (transport layer)

In `drivers/w5500_net.cpp`:

- `net.mac`
- `net.ip`
- `net.sn`
- `net.gw`
- `net.dns`

In `main.cpp`:

- `dst_ip`
- `dst_port`

## Pin mapping

Board mappings are centralized in `pinouts.h`:

- W5500 SPI and control lines
- LCD I2C pins
- ADC I2C pins

Change pin definitions there if hardware wiring changes.

## Common extension points

- Add filtering/averaging to stabilize weight readings in loop.
- Replace static network values with a config mechanism or provisioning mode.
- Add non-blocking startup (remove manual serial waits).
- Add reliability features (sequence numbers, receiver ACK, heartbeat) if needed.

