# Holiday Pixel Display — Project Status for Claude Code
**Date:** 2026-04-13  
**Repo:** https://github.com/bobh/holiday-pixel-display  
**Working directory:** `/Users/bobh/Desktop/Projects/HolidayDisplay/`

---

## Development Environment

| Machine | Role | IDE |
|---|---|---|
| MacBook Pro (MBP) | iPhone Central iOS app (`BLECentralIOS/BLECentralIOS.xcodeproj`) | Xcode + Claude Code CLI |
| MacBook Air (MBA) | Arduino Nano 33 BLE firmware (`HolidayDisplay.ino`, `BLEPeripheral.ino`) | VSCode + Claude Code Extension |

Work done on one machine must be pushed to GitHub before it is visible on the other.

---

## All Hardware Blockers Resolved

### ~~BLOCKER 1 — FRAM I2C pins swapped on PCB~~ — RESOLVED 2026-04-13
PCB reworked. FRAM confirmed working. `Wire.begin()` must be called before each FRAM access — NRF52840 BLE stack leaves I2C in a broken state after `BLE.begin()`. Fix applied in `BLEPeripheral.ino`.

### ~~BLOCKER 2 — iPhone Central app could not connect~~ — RESOLVED 2026-04-12
Local name omitted from BLE advertisement to stay within 31-byte packet limit.

---

## Verified Working

- Power on → **yellow** (UNCONFIGURED, no FRAM config)
- Connect iPhone → **blue**
- Select effect, adjust controls → press **"Save & Next"** → **green** (written to FRAM)
- Disconnect → power cycle → **green** (config loaded from FRAM)
- WarmWhite slow breathing effect verified on hardware (W channel dominant, ~4s cycle)

---

## Arduino Changes — Implemented, Not Yet Integrated (needs flash + test)

### 1. WarmWhite breathing effect (`HolidayDisplay.ino`)
Slow sine-wave pulse, 4-second period, W channel dominant (W=220, R=35 warm tint).  
All 8 pixels move together. First effect to use the white LED channel.  
**Status:** Compiled clean. Visually verified on hardware. ✅

### 2. FRAM persistence (`BLEPeripheral.ino`)
Real `saveConfig()` / `loadConfigFromFRAM()` / `framHasValidConfig()` replacing stubs.  
`Wire.begin()` called inside `saveConfig()` and `loadConfigFromFRAM()` to work around NRF52840 I2C/BLE conflict.  
FRAM layout: `0x0000` = magic byte `0xA5`, `0x0001–0x0010` = `DisplayConfig` (16 bytes with padding).  
**Status:** Compiled clean. Verified working on hardware via Save & Next → power cycle → green boot. ✅

### 3. State machine: revert to saved config on disconnect without save (`BLEPeripheral.ino`)
**Problem:** Booting with saved FRAM config (green/CONFIGURED), connecting to preview a new effect, then disconnecting without pressing Save & Next incorrectly set `state = UNCONFIGURED`. Pixels froze on last rendered frame (solid white at breath peak). LED went yellow.  
**Fix:** On disconnect during CONFIGURING state, check `framHasValidConfig()`. If true, reload FRAM config and stay CONFIGURED (green). Only go UNCONFIGURED if no FRAM config exists at all.  
**Status:** Compiled clean. **Not yet flashed or tested on hardware.** ⚠️

---

## Open Items — iOS / MBP Side

| Issue | Description |
|---|---|
| Effect out of sync after Arduino reset | iPhone shows last selected effect; Arduino boots with FRAM-saved effect. iOS app needs to read `charEffectId` characteristic on reconnect and update UI to match. |

---

## Other Open Items

| Item | Priority | Notes |
|---|---|---|
| Yellow status LED | Low | Reads as yellow/green. Fix: `analogWrite()` to dim green channel relative to red. |
| TPS61023 + LiPo monitoring | Low | A0 (voltage divider), A1 (AC detect), D5 (cutoff) all stubbed. Hardware unverified. |
| More effects | Future | Candle, Ember, Sparkle per `ORTHANC_Halloween_PRD.md` |

---

## Key Files

| File | Description |
|---|---|
| `HolidayDisplay.ino` | Main sketch — PixelDisplay class, Effect enum, setup/loop, effect rendering |
| `BLEPeripheral.ino` | BLE service + FRAM persistence |
| `BLECentralIOS/` | iPhone Central app (Xcode/Swift) — **MBP only** |
| `BLE_Message_Protocol.md` | BLE service/characteristic spec — source of truth |
| `FRAM_erase/FRAM_erase.ino` | FRAM diagnostic + erase utility (bench tool) |
| `NVRAM_test/NVRAM_test.ino` | Basic FRAM read/write test |
| `RGBW_Calibration.md` | W-channel brightness and color mixing notes |
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
- **Local name:** intentionally omitted (31-byte advertisement packet limit)
- **Save trigger:** write `0x01` to `charSaveConfig` (`19B10008-...`) — this is "Save & Next" in the iOS app
