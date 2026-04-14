# LiPo Battery Protection — Design Plan
**Project:** Holiday Pixel Display  
**Date:** 2026-04-13  
**Status:** Draft — awaiting review before implementation

---

## 1. Problem Statement

The Holiday Pixel Display can run on a LiPo battery via the TPS61023 boost converter. LiPo cells are permanently damaged by over-discharge below ~3.0V. The system needs to:

1. Monitor battery voltage continuously
2. Warn the user before the battery reaches a dangerous level
3. Shut down the boost converter automatically at the hard cutoff voltage
4. Never re-enable the converter after cutoff without a deliberate power cycle

---

## 2. Hardware

### Voltage Measurement
- **Pin:** A0
- **Circuit:** Two 100 kΩ 1% resistors in series between Vbattery and GND. A0 connects to the midpoint.
- **Scale factor:** Vadc = Vbattery ÷ 2
- **ADC:** 12-bit (0–4095), 3.3V reference

**Voltage conversion formula:**
```
Vbattery (V) = ADC_count × (3.3 / 4095) × 2
Vbattery (V) = ADC_count × 0.001611
```

**Key ADC threshold values:**

| Condition | Vbattery | Vadc | ADC Count |
|---|---|---|---|
| Full charge | 4.20 V | 2.10 V | 2607 |
| Nominal | 3.70 V | 1.85 V | 2298 |
| Warning threshold | 3.20 V | 1.60 V | 1985 |
| Hard cutoff | 3.00 V | 1.50 V | 1862 |
| Absolute minimum (do not reach) | 2.50 V | 1.25 V | 1553 |

### Boost Converter Enable
- **Pin:** D5
- **Device:** TPS61023 (Adafruit breakout)
- **Polarity:** EN is active-HIGH — HIGH = converter ON, LOW = converter OFF

> **Verify before flashing:** Confirm EN polarity on the specific Adafruit TPS61023 board. Some breakouts invert the logic with an external pull-up. Test with a multimeter: with D5 HIGH, verify 5V output present.

### AC Rail Monitoring
- **Pin:** A1
- **Circuit:** 100 kΩ voltage divider on the common 5V rail (AC adapter output). A1 = Vrail ÷ 2.
- **ADC:** 12-bit, 3.3V reference — same math as A0 battery measurement
- **Sampled every 15 seconds** inside `updatePower()` alongside battery sampling

**Voltage conversion:**
```
Vrail (V) = ADC_count × (3.3 / 4095) × 2 = ADC_count × 0.001611
```

**A1 rail voltage reference points:**

| Condition | Vrail | VA1 | ADC Count |
|---|---|---|---|
| Healthy AC (5V) | 5.00 V | 2.50 V | 3100 |
| AC present but degraded | 4.20 V | 2.10 V | 2607 |
| AC absent / rail dead | 0 V | 0 V | 0 |

> **Current hardware fault context:** The original PCB common rail is reading 4.2V instead of ~5V — a degraded AC condition. Continuous A1 monitoring with real voltage reporting over BLE makes this kind of fault visible remotely without needing a multimeter at the board. The ADC value at 4.2V (~2607) is clearly distinguishable from a healthy 5V rail (~3100).

**Thresholds:**
- `ADC > 1000` (~1.6V, Vrail > 3.2V) = AC present
- `ADC > 2800` (~4.5V, Vrail > 4.5V) = AC present and healthy
- `1000 < ADC ≤ 2800` = AC present but **rail voltage anomaly** — present but below expected

**AC present (healthy) behavior:**
- D5 → LOW (boost converter disabled, LiPo load removed)
- Battery monitoring suspended (no sampling, no WARNING/CUTOFF transitions)
- LED returns to normal config state (green/blue/yellow)
- BLE `charBattery` reports `0xFFFF` ("on AC power, battery not monitored")
- BLE `charBattery` reports actual Vrail mV on each 15s sample for diagnostic visibility

**AC rail anomaly behavior:**
- D5 stays LOW (AC is present, even if degraded — avoid unnecessary battery draw)
- BLE reports actual Vrail mV — iOS app can display or alert on degraded rail
- Does not trigger battery WARNING/CUTOFF state machine

**AC removed behavior:**
- D5 → HIGH (boost converter re-enabled), unless CUTOFF latch is set
- Battery monitoring resumes; regression window resets (60s warmup)
- If CUTOFF latch was set before AC was connected, D5 stays LOW after AC removal — requires reset

---

## 3. Power State Machine

```
              Power-on
                  │
          ┌───────┴────────┐
          │ AC present?    │
         YES               NO
          │                │
          ▼                ▼
    ┌───────────┐    ┌─────────────────────────┐
    │  POWER_AC │    │        POWER_OK         │
    │  D5=LOW   │    │  D5=HIGH, LED=normal    │
    │  LED=norm │    │  Report voltage via BLE │
    └─────┬─────┘    └────────────┬────────────┘
          │  AC                   │  TTE ≤ 300s  OR  Vbatt ≤ 3.2V
          │  removed              ▼
          └──────────►  ┌─────────────────────────┐
                        │      POWER_WARNING      │
                        │  D5=HIGH, LED blinks RED│
                        │  BLE notification sent  │◄─── one-shot alert
                        └────────────┬────────────┘
                                     │  Vbatt ≤ 3.0V  (latched)
                                     ▼
                        ┌─────────────────────────┐
                        │      POWER_CUTOFF       │
                        │  D5=LOW  (boost off)    │
                        │  LED solid RED          │
                        │  BLE shutdown notify    │
                        └─────────────────────────┘
                                     │  Reset/power cycle only
```

**AC state:** Checked every 15 seconds. If AC is detected from any state, immediately transitions to POWER_AC (D5 LOW, monitoring suspended). If AC is removed, transitions to POWER_OK and restarts the regression warmup — unless CUTOFF latch is set, in which case D5 stays LOW.

**Latch rule:** Once POWER_CUTOFF is entered, it is permanent until reset. The LiPo recovers voltage when load is removed — without a latch, the system oscillates ON/OFF near the cutoff threshold.

---

## 4. Battery Sampling

- **Interval:** 15 seconds (4 samples per minute, as specified)
- **Timing:** Non-blocking via `millis()` — same pattern as the LED effect frame timer
- **ADC setup:** `analogReadResolution(12)` called once in `setupPower()`
- **Warmup:** The regression requires 5 samples (60 seconds). During warmup, state stays POWER_OK and TTE is reported as "unknown." Voltage hard cutoff still applies during warmup.

---

## 5. Time to Empty — Linear Regression

### Method
5-sample sliding window, least-squares linear regression.  
Samples are stored oldest-to-newest. Each new sample shifts the window.

**Fixed x-axis values (seconds):** `{0, 15, 30, 45, 60}`

Pre-computed constants (x is always the same):
```
n       = 5
sum_x   = 150
sum_x2  = 6750
denom   = n×sum_x2 − sum_x² = 33750 − 22500 = 11250
```

**Slope calculation (V/s):**
```
sum_y  = V[0] + V[1] + V[2] + V[3] + V[4]
sum_xy = 0×V[0] + 15×V[1] + 30×V[2] + 45×V[3] + 60×V[4]
m      = (5×sum_xy − 150×sum_y) / 11250
```

**Time to Empty (seconds):**
```
TTE = (V_cutoff − V_current) / m
    = (3.0 − V[4]) / m
```
`m` is negative (voltage is falling), so TTE is positive.  
If `m ≥ 0` (voltage flat or rising), TTE is treated as infinite — no warning.

### Warning Trigger
`TTE ≤ 300 seconds` (5 minutes) **OR** `Vbatt ≤ 3.2V` — whichever comes first.

The voltage threshold catches cases where the battery drops rapidly (high load) before the regression window can predict it. The regression catches a gradual approach to cutoff.

---

## 6. LED Behavior

The onboard RGB LED currently signals BLE config state (yellow/blue/green). Battery warning **overrides** config state signaling:

| Power State | LED |
|---|---|
| POWER_OK | Normal config state (yellow / blue / green) |
| POWER_WARNING | Blinks RED — 100 ms ON / 900 ms OFF (10% duty cycle, ~1 Hz) |
| POWER_CUTOFF | Solid RED (permanent) |

**Rationale for 10% duty cycle:** The system is already low on power. A short pulse is visually urgent and distinctive while minimizing LED current draw.

**Implementation:** Non-blocking blink using `millis()`. Battery state LED logic runs after config state logic so it can override.

---

## 7. BLE Notification

### Existing characteristic reused: `charBattery` (`19B10011-...`)
- Already declared `BLERead | BLENotify`
- Currently always reports `0x0000` (stub)
- **No new characteristic needed**

### Design principle
The Arduino owns all voltage measurements, thresholds, and state decisions. The iPhone receives only a high-level status code and displays a fixed message. No voltage values are sent to iOS — thresholds live exclusively on the Arduino.

### Reporting protocol

The `charBattery` characteristic sends a **1-byte status code** (using the low byte of the existing 2-byte characteristic). The Arduino decides the state; the iPhone reacts to the code.

| Code | Name | Arduino condition | iOS action |
|---|---|---|---|
| `0x00` | OK | Battery healthy, monitoring active | No alert — silent |
| `0x01` | BATTERY_WARNING | TTE ≤ 300s or Vbatt ≤ 3.2V | Alert: "Holiday Lights battery is low. Switch to AC power." |
| `0x02` | BATTERY_CUTOFF | Vbatt ≤ 3.0V (latched) | Alert: "Holiday Lights are shutting down. Switch to AC power." |
| `0x03` | HARDWARE_FAULT | AC rail present but degraded | Alert: "Hardware fault detected on Holiday Lights." |
| `0x04` | ON_AC | AC rail healthy | Informational — no alert |

**Notifications are one-shot:** the Arduino sends a BLE notify only on state *transitions*, not repeatedly. The iPhone does not need to poll or debounce.

**Voltage data stays on the Arduino.** Raw millivolt readings from A0 and A1 are used internally for state decisions and are never transmitted over BLE.

`notifyBattery()` in `BLEPeripheral.ino` is updated to:
1. Report real voltage mV every 15 seconds during normal operation
2. Send an immediate notification on WARNING entry
3. Send a final `0x0000` notification on CUTOFF entry

### iPhone Alert Message (MBP implementation)
On receiving `0x0000` or voltage < 3200 mV:
> **"Holiday Lights are shutting down. Switch to AC power."**

---

## 8. Files Changed

| File | Changes |
|---|---|
| `HolidayDisplay.ino` | Add `setupPower()`, `updatePower()`, `sampleBattery()`, `computeTTE()`. Update `setStatusLED()` logic for battery override. Call `setupPower()` from `setup()`, `updatePower()` from `loop()`. |
| `BLEPeripheral.ino` | Update `notifyBattery()` to accept real mV value. Add `triggerBatteryWarningNotify()` and `triggerBatteryShutdownNotify()` called from `updatePower()`. |
| `BLECentralIOS/` (MBP) | Subscribe to `charBattery` notifications. Trigger iOS local notification on warning/shutdown values. |

---

## 9. New Symbols (Arduino)

```cpp
// setupPower() — call once from setup(), after setupBLE()
void setupPower();

// updatePower() — call every loop(), non-blocking
void updatePower();

// Shared with BLEPeripheral.ino for charBattery notification
enum class PowerStatus : uint8_t {
    OK               = 0x00,
    BATTERY_WARNING  = 0x01,
    BATTERY_CUTOFF   = 0x02,
    HARDWARE_FAULT   = 0x03,
    ON_AC            = 0x04,
};

extern PowerStatus currentPowerStatus;  // set by updatePower(), read by notifyBattery()
```

---

## 10. Risks and Open Questions

| # | Item | Notes |
|---|---|---|
| 1 | TPS61023 EN polarity | Verify HIGH=ON before flashing. Inverting this shuts down LEDs at full battery. |
| 2 | A1 voltage divider values | Assumed 100K/100K (Vrail ÷ 2). Verify on PCB — different values shift the ADC thresholds. Measure VA1 with multimeter while AC is connected and back-calculate Vrail to confirm. |
| 6 | Original PCB hardware fault | Common rail reading 4.2V instead of ~5V — cause unknown (possible voltage transient damage). Continuous A1 monitoring will report this over BLE. Suspected components: Schottky diode on AC path, TPS61023, or AC adapter itself. |
| 3 | Regression during rapid discharge | 5 samples × 15s = 60s window. If load spikes cause rapid drop, hard voltage threshold catches it. |
| 4 | BLE not connected at cutoff | `notifyBattery()` only sends when a central is connected. If iPhone is not connected at cutoff, no alert is sent. Mitigated by LED indication. |
| 5 | charBattery value 0x0000 ambiguity | `0x0000` used as shutdown sentinel. If genuine battery read fails (ADC error), could false-trigger shutdown notify. Consider `0xFFFF` as "error/unknown" sentinel instead. |

---

## 11. Out of Scope

- Multi-cell battery packs
- Fuel gauge IC (e.g., MAX17043) integration
- Persistent low-battery log in FRAM
