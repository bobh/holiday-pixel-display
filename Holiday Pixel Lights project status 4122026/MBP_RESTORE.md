# MBP Restore Guide — Holiday Pixel Lights
**Restore phrase:** "Restore Holiday Lights context"

When you return in September, the restore phrase is: **"Restore Holiday Lights context"** — memory and CLAUDE_STATUS.md will have the full picture. The three unverified items to work through:

---

## How to Restore Context

1. Open this file and `CLAUDE_STATUS.md` in the same directory
2. Claude Code (MBP) will load memory automatically — it knows the project, machine roles, and git workflow
3. Pull latest from GitHub before starting: `cd ~/Desktop/Projects/HolidayDisplay && git pull`

---

## MBP Environment

- **Role:** iOS iPhone Central app only — Xcode development
- **Xcode project:** `~/Desktop/Projects/HolidayDisplay/BLECentralIOS/BLECentralIOS.xcodeproj`
- **Git repo:** `~/Desktop/Projects/HolidayDisplay/`
- **Test device:** iPhone 13 mini
- **Do not modify:** Any `.ino` Arduino files — reference only; Arduino work belongs to MBA

---

## Three Open Items to Work Through

### 1. Effect out of sync after Arduino reset (iOS fix needed)
**Problem:** iPhone shows the last effect selected during the session. Arduino boots with the FRAM-saved effect. After reconnecting, the UI doesn't reflect what the Arduino is actually running.  
**Fix needed:** In `BLEManager.swift`, `didUpdateValueFor` already handles `charEffectId` reads — confirm `ControlView` is binding to `currentEffect` at connection time and displaying the value read back from the peripheral, not the last locally-selected value.  
**File:** `BLECentralIOS/BLECentralIOS/BLEManager.swift`

### 2. State machine fix — coordinate with MBA after flash
**Problem:** Booting with saved FRAM config (green/CONFIGURED), connecting to preview a new effect, then disconnecting without Save & Next incorrectly sets the board back to UNCONFIGURED.  
**Arduino fix:** On disconnect during CONFIGURING, check `framHasValidConfig()` — if true, reload FRAM config and stay CONFIGURED. MBA owns this fix; MBP only needs to verify the iOS app responds correctly (board stays green, config status shows Configured in scanner list).

### 3. Power subsystem — no iOS work needed, but verify display
**TPS61023 boost converter + LiPo battery monitoring** (A0 voltage divider, A1 AC detect, D5 cutoff) are all stubbed on the Arduino. Once MBA implements battery reporting, `BLEManager.swift` already reads `charBattery` (2-byte mV value) and exposes `batteryMv` — verify it surfaces correctly in the UI.

---

## Key Reference
- **Repo:** https://github.com/bobh/holiday-pixel-display
- **Protocol spec:** `BLE_Message_Protocol.md` — source of truth for UUIDs and message format
- **Full project status:** `CLAUDE_STATUS.md` in this directory
