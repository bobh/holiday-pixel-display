# HolidayDisplay Project Status - 2026-05-15

## Current State

- Breadboard hardware is functioning.
- SK6812 RGBW pixel string, pixel power path, Nano GPIO, and 74HCT14 data buffering have been verified functional.
- Operational firmware can preview and run effects from the iPhone app.
- The iPhone app now installs and runs on the attached iPhone 13 mini.
- Arduino firmware and iPhone app both compile successfully.

## Key Troubleshooting Conclusions

- The original "dead pixels" symptom was not caused by the LED string.
- Pixel power and data signaling were ultimately verified good.
- The operational sketch initially appeared power sensitive because firmware state/configuration and power monitoring could suppress pixel updates.
- Onboard Arduino LED meanings:
  - Blue: BLE central connected/configuring.
  - Green: configured/saved config loaded.
  - Yellow: unconfigured/no valid saved config.
  - Red: battery warning/cutoff logic owns the LED.
- USB-only bench power with no AC rail and no battery sense was being treated as battery cutoff after 15 seconds. Firmware now avoids latching red in the no-battery/no-AC bench condition.

## FRAM / NVRAM Status

- The FM24CL16B driver was corrected.
- Correct addressing model:
  - I2C block addresses: `0x50` through `0x57`
  - Upper address bits are encoded in the I2C address.
  - Only one word-address byte is sent.
- Breadboard FRAM diagnostic result:
  - `0x50` through `0x57` ACK.
  - Non-destructive write/read/restore test passed.
- PWB FRAM diagnostic result:
  - No ACK at `0x50` through `0x57`.
  - PWB rework must be checked.
- Next PWB checks:
  - FM24CL16B `VDD` to `GND`.
  - FRAM ground continuity to Nano ground.
  - SDA idle high.
  - SCL idle high.
  - Pullups to 3.3 V.
  - SDA/SCL rework and trace isolation from the original swapped routing.

## Firmware Changes Made

- `HolidayDisplay.ino`
  - Added `VBATT_PRESENT_MIN`.
  - Avoids battery cutoff latch when AC is absent and battery is not sensed.
  - Samples power immediately during setup.
  - Moves `setupPower()` earlier in `setup()`.
  - Refactored Fire and WarmWhite rendering into helpers.
  - Added real `Ember` effect.
  - Added real `Sparkle` effect.
  - Made effects more exaggerated for an 8-pixel strip:
    - Fire: active orange/red flicker.
    - Ember: dim slow red coal glow.
    - Sparkle: mostly black with sparse bright flashes.
    - WarmWhite: smooth white breathing.
  - Placeholder effects still fall back visibly to Fire.

- `BLEPeripheral.ino`
  - Corrected FM24CL16B addressing.
  - Save config now verifies FRAM write/readback.
  - `Save Config` characteristic now supports read/write/notify.
  - Board only reports configured after successful FRAM verification.

- `FRAM_diag/FRAM_diag.ino`
  - Added non-destructive FM24CL16B diagnostic sketch.
  - Scans `0x50` through `0x57`.
  - Dumps config bytes.
  - Performs non-destructive write/read/restore test at `0x07F0`.

- `FRAM_erase/FRAM_erase.ino`
  - Updated to corrected FM24CL16B addressing.

## iPhone App Changes Made

- `BLEManager.swift`
  - `Save & Next` no longer optimistically marks configured.
  - App waits for the Arduino save ACK.
  - ACK `1`: mark configured and disconnect.
  - ACK `0`: stay connected and show save failure.
  - Fixed main actor warning around `saveConfigUUID`.

- `ControlView.swift`
  - `Save & Next` now calls `saveConfig(disconnectOnSuccess: true)`.
  - Button disables while saving.

## Known Current Behavior

- On breadboard, after save/reset, the app can show Configured.
- BLE may stop responding after Save & Next until the Arduino is reset.
- This is acceptable for now, but should be investigated later.
- On PWB, FRAM currently does not ACK, so saved configuration cannot persist until hardware rework is repaired.

## Build Verification

- Arduino main sketch compiles:
  - `arduino-cli compile --fqbn arduino:mbed_nano:nano33ble /Users/bobh/Desktop/Projects/HolidayDisplay`
- FRAM diagnostic sketch compiles:
  - `arduino-cli compile --fqbn arduino:mbed_nano:nano33ble /Users/bobh/Desktop/Projects/HolidayDisplay/FRAM_diag`
- iPhone app builds successfully with Xcode.
- Updated app was installed on attached iPhone 13 mini.

## Next Work

1. Repair/check PWB FRAM rework.
2. Re-run `FRAM_diag` on the PWB.
3. Confirm PWB gets ACK at `0x50` through `0x57`.
4. Upload main firmware to PWB.
5. Use iPhone app to select effect and `Save & Next`.
6. Reset Arduino and confirm board boots configured with saved effect.
7. Later: add a clearly visible "breathing green" diagnostic/effect so the current active effect is obvious.
8. Later: investigate BLE state after Save & Next so reset is not required.
