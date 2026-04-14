# Holiday Pixel Display — Project Status for Claude Code
**Date:** 2026-04-14  
**Repo:** https://github.com/bobh/holiday-pixel-display  
**Working directory:** `/Users/bobh/Desktop/Projects/HolidayDisplay/`  
**Next session:** On or around 2026-09-01 (near Holiday Season)

---

## Development Environment

| Machine | Role | IDE |
|---|---|---|
| MacBook Pro (MBP) | iPhone Central iOS app (`BLECentralIOS/BLECentralIOS.xcodeproj`) | Xcode + Claude Code CLI |
| MacBook Air (MBA) | Arduino Nano 33 BLE firmware (`HolidayDisplay.ino`, `BLEPeripheral.ino`) | VSCode + Claude Code Extension |

Work done on one machine must be pushed to GitHub before it is visible on the other.

---

## All Hardware Blockers Resolved

| Blocker | Resolution |
|---|---|
| FRAM I2C pins swapped on PCB | PCB reworked 2026-04-13. `Wire.begin()` required before FRAM access — NRF52840 BLE stack breaks I2C after `BLE.begin()`. |
| iPhone Central app could not connect | Local name omitted from BLE advertisement — 31-byte packet limit. Fixed 2026-04-12. |

---

## Implemented and Committed — Not Yet Hardware Tested

All items below compiled clean and are committed to GitHub (`main`). Hardware verification deferred to the 2026 Holiday Season.

### 1. WarmWhite breathing effect (`HolidayDisplay.ino`)
Slow 4-second sine-wave pulse, W channel dominant (W=220 peak, R=35 warm tint).  
First effect to exercise the SK6812 white LED channel. Effect ID 4, already in iOS enum — no MBP changes needed.

### 2. State machine: revert to saved config on disconnect without save (`BLEPeripheral.ino`)
On disconnect during CONFIGURING state (connected but Save & Next not pressed):
- If `framHasValidConfig()` → reload FRAM config, stay CONFIGURED (green LED)
- If no FRAM config → go UNCONFIGURED (yellow LED) as before

Previously: always went UNCONFIGURED, pixels froze on last rendered frame.

### 3. LiPo battery protection (`HolidayDisplay.ino` + `BLEPeripheral.ino`)
Full design document: `Battery_Protection_Design.md`

**Four power states:**

| State | Trigger | LED | D5 (Boost EN) | BLE notify |
|---|---|---|---|---|
| OK | Normal operation | Normal BLE state | HIGH (on) | Silent |
| BATTERY_WARNING | TTE ≤ 5 min OR Vbatt ≤ 3.2V | Blinks RED (100ms/900ms) | HIGH | Code `0x01` |
| BATTERY_CUTOFF | Vbatt ≤ 3.0V (latched) | Solid RED | LOW (off) | Code `0x02` |
| HARDWARE_FAULT | AC rail present but < 4.5V | Normal BLE state | LOW | Code `0x03` |
| ON_AC | AC rail ≥ 4.5V | Normal BLE state | LOW | Code `0x04` |

**Key design decisions:**
- TTE via 5-sample linear regression on A0 (15s sample interval, 60s warmup)
- A1 monitors actual AC rail voltage continuously — useful for diagnosing PCB hardware fault (original board shows 4.2V instead of 5V on common rail)
- iPhone receives status codes only — no voltage values cross BLE. iPhone does not implement threshold logic.
- `ledLocked` flag prevents BLE state LED changes from overriding battery WARNING/CUTOFF LED
- Cutoff latch is RAM-only — clears on hardware reset/power cycle only
- TPS61023 EN polarity confirmed: HIGH = boost ON

**BLE notification:** `charBattery` sends 1-byte status code on state transitions only (one-shot). `notifyBattery()` checks `powerStatusChanged` flag set by `updatePower()`.

---

## Open Items — iOS / MBP Side

| Issue | Description |
|---|---|
| Effect out of sync after Arduino reset | iPhone caches last selected effect. On reconnect, app should read `charEffectId` characteristic and update UI to match current Arduino state. |
| Battery status notifications | Subscribe to `charBattery` notifications. On code `0x01`: alert "Holiday Lights battery is low. Switch to AC power." On code `0x02`: alert "Holiday Lights are shutting down. Switch to AC power." On code `0x03`: alert "Hardware fault detected on Holiday Lights." |

---

## Other Open Items

| Item | Notes |
|---|---|
| Yellow status LED | Reads as yellow/green. Fix: `analogWrite()` to reduce green channel. Low priority. |
| TPS61023 + LiPo circuit verification | Battery protection code is implemented; hardware path (A0 divider, A1 divider, D5→EN) not yet exercised. Verify A1 divider ratio with multimeter before trusting rail voltage readings. |
| More effects | Candle, Ember, Sparkle — see `ORTHANC_Halloween_PRD.md` |
| Original PCB hardware fault | Common rail reading 4.2V (expected ~5V). Cause unknown — possible voltage transient damage. A1 monitoring will surface this via BLE `HARDWARE_FAULT` notification once battery code is exercised. |

---

## Key Files

| File | Description |
|---|---|
| `HolidayDisplay.ino` | Main sketch — effects, power management, setup/loop |
| `BLEPeripheral.ino` | BLE service, FRAM persistence, battery notification |
| `Battery_Protection_Design.md` | Full design document for battery protection feature |
| `BLECentralIOS/` | iPhone Central app (Xcode/Swift) — MBP only |
| `BLE_Message_Protocol.md` | BLE service/characteristic spec |
| `FRAM_erase/FRAM_erase.ino` | FRAM diagnostic + erase utility (bench tool) |
| `NVRAM_test/NVRAM_test.ino` | Basic FRAM read/write test |
| `CLAUDE.md` | Architecture reference — pin assignments, class structure |

---

## FRAM Layout

| Address | Content |
|---|---|
| `0x0000` | Magic byte — `0xA5` = valid config present |
| `0x0001–0x0010` | `DisplayConfig` struct (16 bytes incl. alignment padding) |

---

## BLE Quick Reference

- **Service UUID:** `19B10000-E8F2-537E-4F6C-D104768A1214`
- **Manufacturer data (8 bytes):** `[0xFF, 0xFF, boardId, deviceType, fwVersion, pixelCount, capFlags, configState]`
- **Local name:** omitted — 31-byte advertisement packet limit
- **Save trigger:** write `0x01` to `charSaveConfig` (`19B10008-...`) = "Save & Next" in iOS app
- **Battery status:** `charBattery` (`19B10011-...`) — 1-byte status code, notified on transitions
