# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current Project Status

**Read this first.** Full status, open items, and implementation notes are in:
`Holiday Pixel Lights project status 4122026/CLAUDE_STATUS.md`

**Summary (as of 2026-04-14):**
- BLE connection, FRAM persistence, WarmWhite effect, battery protection — all implemented and committed
- Hardware verified: BLE ✅  FRAM ✅  WarmWhite ✅  Battery/power circuit — compiled, not yet tested
- Next session: ~2026-09-01 (Holiday Season)
- MBP open items: read `charEffectId` on reconnect (out-of-sync fix); subscribe to `charBattery` notifications for battery alerts

**Two-machine workflow:** MBA owns Arduino firmware (`HolidayDisplay.ino`, `BLEPeripheral.ino`). MBP owns iOS app (`BLECentralIOS/BLECentralIOS.xcodeproj`). Sync via GitHub — always `git pull` before starting work.

---

## Project Overview

Holiday Pixel Display is a modular smart-lighting system for holiday displays featuring seasonal effect libraries, persistent memory, and intelligent power management. Built on Arduino Nano 33 BLE with SK6812 RGBW LEDs.

**GitHub Repository:** https://github.com/bobh/holiday-pixel-display

## Build & Upload Commands

This project uses VSCode with the Arduino extension. The Arduino CLI can also be used directly:

```bash
# Compile (verify) the sketch
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble .

# Upload to connected Arduino
arduino-cli upload -p /dev/tty.usbmodem1101 --fqbn arduino:mbed_nano:nano33ble .
```

Build output goes to `./build/` directory (gitignored).

## Hardware Configuration

- **Board:** Arduino Nano 33 BLE (Plain version recommended, not Sense)
- **MCU:** NRF52840 (ARM Cortex-M4)
- **LEDs:** SK6812 RGBW (Warm White), typically 4-8 pixels on pin D3
- **Memory:** FM24CL16B I2C FRAM — **implemented and working**. Magic byte `0xA5` at `0x0000`, `DisplayConfig` at `0x0001`. Call `Wire.begin()` before FRAM access — NRF52840 BLE stack breaks I2C after `BLE.begin()`.
- **Power:** Dual-source (battery + AC adapter) with automatic AC priority via Schottky diodes. Battery protection **implemented** (see `Battery_Protection_Design.md`).

**Pin Assignments:**
- `D3`: SK6812 pixel data (via 74HCT14 level shifter)
- `A0`: Battery voltage monitoring (100kΩ ÷2 divider) — **implemented**
- `A1`: AC rail voltage monitoring (100kΩ ÷2 divider) — **implemented**
- `D5`: TPS61023 boost converter EN — HIGH=on, LOW=off, **confirmed polarity** — **implemented**
- `SDA/SCL`: I2C for FRAM communication

**Critical Hardware Notes:**
- Arduino 3.3V rail limited to ~50mA total (FRAM uses up to 5mA)
- Power must connect to VIN pin, NOT the 5V pin (5V pin is USB-only output)
- 1N4001 diode D3 rated for 1A: safe for 2-8 pixels (~480mA); remove and jumper for larger strings
- If using Nano 33 BLE Sense variant, disable all unused sensors in setup() to save power

## Code Architecture

### Two-File Structure

Firmware is split across two `.ino` files in the same sketch directory (Arduino concatenates them):
- `HolidayDisplay.ino` — PixelDisplay class, Effect enum, power management, setup/loop
- `BLEPeripheral.ino` — BLE service/characteristics, FRAM persistence, battery notification

### PixelDisplay Class

Central class managing LED control and effect rendering (lines 32-244):

- **Initialization:** `begin()` - starts I2C, initializes NeoPixelBus, loads effect from FRAM
- **Runtime:** `update()` - called from `loop()`, handles frame pacing (~50 FPS / 20ms intervals) and effect rendering
- **Effect Control:** `setEffect()` / `getEffect()` - switches effects and persists to FRAM

**Key Internal State:**
- `strip`: NeoPixelBus instance using `NeoGrbwFeature, Neo800KbpsMethod`
- `currentEffect`: Active effect from enum
- `globalBrightness`: Master brightness (0-255), currently hardcoded to 50
- Effect-specific parameters (e.g., `fireBaseRed`, `fireFlickerStrength`)

### Effect System

Effects defined as `enum class Effect`:
- Fire (0) — implemented: deep red/orange flicker with thermal drift
- WarmWhite (4) — implemented: slow 4s sine-wave breathing, W channel dominant
- Candle (1), Ember (2), Sparkle (3) — placeholders
- LOTR_ColdWhite (10), LOTR_Palantir (11), LOTR_ManyColor (12) — placeholders

Each effect is a case in the `update()` switch statement. Effects render by:
1. Computing color values (RGBW)
2. Applying global brightness via `scaleBrightness()`
3. Setting all pixels and calling `strip.Show()`

**W Channel Design Philosophy:**
The W channel in SK6812 RGBW is significantly brighter than RGB channels and can wash out colors. Design guideline:
- **Saturated colors:** Use pure RGB (e.g., R, G, B, W=0)
- **Warm/white effects:** Use W-dominant combinations

Current Fire effect uses only RGB channels (W=0) for deep red/orange tones.

### FRAM Persistence

Implemented in `BLEPeripheral.ino`. FM24CL16B at I2C address `0x50`.
Layout: magic byte `0xA5` at `0x0000`, `DisplayConfig` struct (16 bytes) at `0x0001`.
**Important:** call `Wire.begin()` immediately before any FRAM access — `BLE.begin()` leaves I2C broken on NRF52840.

### Power Management

Implemented in `HolidayDisplay.ino` via `setupPower()` / `updatePower()`.
Five states: OK, BATTERY_WARNING, BATTERY_CUTOFF, HARDWARE_FAULT, ON_AC.
Full design: `Battery_Protection_Design.md`.
`notifyBattery()` in `BLEPeripheral.ino` sends 1-byte `PowerStatus` code on state transitions — voltage values never leave the Arduino.

## Effect Implementation Guidelines

When adding new effects:

1. Add to `Effect` enum (line 21-27)
2. Add case to `switch(currentEffect)` in `update()` (line 83-153)
3. Use frame pacing - `update()` is called every loop but only renders when `frameIntervalMs` elapsed
4. Compute RGBW values, then scale via `scaleBrightness()` before setting pixels
5. Call `strip.Show()` at end of effect to push data to LEDs
6. Consider W channel brightness - use sparingly or set to 0 for saturated colors

**Planned Effects (from README):**
- Halloween: Pumpkin Glow, Witch-Green Pulse, Ghost Fade, Spooky Strobe
- Thanksgiving: Harvest Ember, Warm Hearth Glow, Autumn Fade
- Christmas: Cozy Fireplace (current Fire), Snow-Sparkle White, Red-Green Pulse
- New Year: Gold Sparkle, Champagne Flicker, Midnight Blue Fade

## Memory Constraints

Current usage (as of 2026-04-14 build):
- **Flash:** 348,112 bytes (35% of 983,040) — room for more effects
- **RAM:** 71,728 bytes (27% of 262,144) — generous headroom

The NeoPixelBus library uses DMA for efficient pixel updates without CPU blocking.

## Future Architecture

Per README, planned enhancements include:
- Complete 13+ seasonal effect library
- BLE service for wireless control (effect selection, brightness, battery status)
- Multi-unit synchronization
- Scheduling with timers
- Mobile app integration

Consider refactoring to separate files when adding BLE (e.g., `effects.h`, `ble_service.h`, `power_mgmt.h`).

## Dependencies

- **NeoPixelBus by Makuna** - LED control library
- **Wire** - I2C communication (built-in)
- **Arduino mbed_nano core** v4.5.0 - Board support package

## VSCode Configuration

IntelliSense may show red squiggles on `#include <Wire.h>` despite successful compilation. This is a known Arduino/VSCode quirk and can be ignored - the compiler has the correct paths.

The `.vscode/c_cpp_properties.json` is configured with all necessary include paths for the ARM toolchain and libraries.
