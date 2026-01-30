# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
- **Memory:** FM24CL16B I2C FRAM for persistent configuration (not yet implemented)
- **Power:** Dual-source (battery + AC adapter) with automatic AC priority via Schottky diodes

**Pin Assignments:**
- `D3`: SK6812 pixel data (via 74HCT14 level shifter)
- `A0`: Battery voltage monitoring (100kΩ divider) - not yet implemented
- `A1`: AC adapter presence detection (100kΩ divider) - not yet implemented
- `D5`: PowerBoost enable control - not yet implemented
- `SDA/SCL`: I2C for FRAM communication

**Critical Hardware Notes:**
- Arduino 3.3V rail limited to ~50mA total (FRAM uses up to 5mA)
- Power must connect to VIN pin, NOT the 5V pin (5V pin is USB-only output)
- 1N4001 diode D3 rated for 1A: safe for 2-8 pixels (~480mA); remove and jumper for larger strings
- If using Nano 33 BLE Sense variant, disable all unused sensors in setup() to save power

## Code Architecture

### Single-File Structure

The entire firmware is in `HolidayLightsTestHarness.ino` as a monolithic sketch. This is intentional for this test harness phase.

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

Effects defined as `enum class Effect` (lines 21-27):
- Fire (implemented) - deep red/orange flicker with thermal drift
- Candle, Ember, Sparkle, WarmWhite (placeholders)

Each effect is a case in the `update()` switch statement. Effects render by:
1. Computing color values (RGBW)
2. Applying global brightness via `scaleBrightness()`
3. Setting all pixels and calling `strip.Show()`

**W Channel Design Philosophy:**
The W channel in SK6812 RGBW is significantly brighter than RGB channels and can wash out colors. Design guideline:
- **Saturated colors:** Use pure RGB (e.g., R, G, B, W=0)
- **Warm/white effects:** Use W-dominant combinations

Current Fire effect uses only RGB channels (W=0) for deep red/orange tones.

### FRAM Persistence (Stubbed)

Functions `loadEffectFromFram()` and `saveEffectToFram()` (lines 219-237) are stubbed placeholders. They're designed for FM24CL16B I2C FRAM:
- **Address:** `FRAM_EFFECT_ADDR = 0x0000`
- **Data:** Single byte storing `Effect` enum value
- **Protocol:** Standard I2C via Wire library

When implementing:
1. Wire.beginTransmission() to FRAM I2C address
2. Write address MSB/LSB (FM24CL16B has 2KB = 11-bit addressing)
3. Read/write effect byte
4. Wire.endTransmission()

### Power Management (Not Implemented)

Pins A0, A1, D5 are allocated but unused. Future implementation should:
- Read A0 voltage divider to monitor battery level
- Read A1 to detect AC adapter presence
- Control D5 to enable/disable PowerBoost based on power state

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

Current usage (as of last build):
- **Flash:** 104,032 bytes (10% of 983,040) - plenty of room for effects
- **RAM:** 46,456 bytes (17% of 262,144) - generous headroom

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
