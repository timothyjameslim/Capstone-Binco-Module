# Binco Firmware

Binco is RP2040 firmware for an instrumented storage bin. It reads weight from a load cell (through a NAU7802 ADC), displays live bin status on a 16x2 LCD, and broadcasts bin telemetry over UDP via a W5500 Ethernet interface.

## What this system does

- Samples the load cell continuously and converts ADC counts to grams.
- Publishes a compact UDP payload (`BinData`) with:
  - bin name
  - bin ID
  - quantity field
  - current measured weight (grams)
- Shows the same bin identity and live weight on the LCD1602.

For deeper architecture and code-walk details, see **[SYSTEM_OVERVIEW.md](./SYSTEM_OVERVIEW.md)**.

## Repository layout

- `main.cpp` – application startup, calibration flow, runtime loop, UDP send, LCD updates
- `drivers/w5500_net.cpp` – W5500 low-level bring-up + static network configuration
- `pinouts.h` – GPIO mapping for Ethernet, LCD, and ADC wiring
- `drivers/` – W5500 stack, sockets, LCD and ADC driver code
- `port/` – RP2040-specific Wiznet chip-port implementation

## Deploying one bin

1. Build and flash this firmware to the W5500-EVB-PICO.
2. Open serial console.
3. Follow prompts:
   - tare with empty scale
   - place reference weight for calibration
4. Confirm UDP packets arrive on your collector host/port.

## Deploying to multiple bins

To deploy multiple physical bins on the same network, each firmware image should have a unique identity and network configuration.

### 1) Give each bin a unique application identity

In `main.cpp`, update the hardcoded values before build:

- `nm` (bin name)
- `d.ID` (bin ID)
- optionally `d.quantity` default

These fields are included in the UDP payload and shown on the LCD.

### 2) Give each bin a unique network identity

In `drivers/w5500_net.cpp`, set unique values per device:

- `net.mac` – must be unique per bin
- `net.ip` – static IP for that bin
- `net.sn` / `net.gw` / `net.dns` – subnet/router settings for your site

In `main.cpp`, set destination endpoint for your server:

- `dst_ip`
- `dst_port`

### 3) Keep packet format stable across bins

`BinData` is packed and transmitted raw over UDP. If you change this struct, update receivers at the same time.

### 4) Recommended production cleanup

`wait_for_one()` gates startup on serial input. For unattended deployments, remove or bypass these debug pauses in `main.cpp`.

## Suggested multi-bin rollout process

1. Create a per-bin config sheet (Bin Name, Bin ID, MAC, Device IP, Collector IP/Port).
2. Apply values in source for a single bin profile.
3. Build + flash.
4. Label the physical unit with its Bin ID/IP.
5. Repeat for each unit.

## Build notes

- This project uses CMake + Pico SDK.
- `CMakeLists.txt` currently contains a user-local `PICO_SDK_PATH`; update it for your build environment before compiling.
