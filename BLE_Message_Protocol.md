# BLE Message Protocol (Holiday / Halloween Display Control)
**Version 0.1**

This document defines the **BLE GATT protocol** for controlling the holiday lighting display (Orthanc tower) from a BLE Central (app/device).

It is designed to be:
- **Simple and extensible** (effects + parameters)
- **Platform-agnostic** (works with Nano 33 BLE, ESP32-S3, etc.)
- **Consistent across devices** via a shared UUID scheme and payload format

---
## 1. BLE Service & Characteristics

### 1.1 Service: `HOLIDAY / HALLOWEEN DISPLAY CONTROL`
- **UUID**: `19B10000-E8F2-537E-4F6C-D104768A1214`
- **Properties**: Advertised service, support for Notify + Write on characteristics (see below)

### 1.2 Characteristic: `device_info` (READ ONLY)
- **UUID**: `19B10010-E8F2-537E-4F6C-D104768A1214`
- **Type**: UTF-8 string or small binary struct
- **Purpose**: Allows the app to adapt its UI based on device type/capabilities.

**Recommended format (CSV-like):**
```
<model>,<fw>,<capabilities>,<pixel_count>
```

**Nano example:**
```
NANO33BLE,fw1.0,RGBW,FRAM,8px
```
**S3 example:**
```
ESP32S3,fw1.0,RGBW,AUDIO,FLASH,32px
```

---
### 1.3 Characteristic: `battery_status` (READ + NOTIFY)
- **UUID**: `19B10011-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t` (percentage) or `uint16_t` (millivolts)
- **Meaning**: Reports battery level or raw voltage.

**Nano 33 BLE Rev 2**: report either percentage or raw mV from the divider.

---
### 1.4 Characteristic: `effect_id`
- **UUID**: `19B10001-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t`
- **Meaning**: Select the active lighting effect.

### 1.5 Characteristic: `brightness`
- **UUID**: `19B10002-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t` (0–255)

### 1.6 Characteristic: `speed`
- **UUID**: `19B10003-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t` (controls pulse/transition speed)

### 1.7 Characteristic: `color_primary`
- **UUID**: `19B10004-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint32_t` (0xRRGGBB)

### 1.8 Characteristic: `color_secondary`
- **UUID**: `19B10005-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint32_t` (0xRRGGBB)

### 1.9 Characteristic: `flags`
- **UUID**: `19B10006-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t` bitfield (expandable for future toggles)

### 1.10 Characteristic: `audio_track_id`
- **UUID**: `19B10007-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t`
- **Meaning**: Selects a sound track (ignored on Nano, used on ESP32-S3).

### 1.11 Characteristic: `save_config` (WRITE + NOTIFY)
- **UUID**: `19B10008-E8F2-537E-4F6C-D104768A1214`
- **Type**: `uint8_t` (write `1` to save)

**Behavior**:
- On write(1): device saves current `DisplayConfig` to non-volatile storage.
- Then sends Notify(1) to confirm success.

---
## 2. Shared Config Structure

```cpp
struct DisplayConfig {
    uint8_t  effect_id;
    uint8_t  brightness;
    uint8_t  speed;
    uint32_t color_primary;     // RGB only
    uint32_t color_secondary;   // RGB only
    uint8_t  audio_track_id;    // ignored on Nano
    uint8_t  flags;
};
```

This struct is the canonical configuration payload that can be stored in FRAM or flash and restored at boot.

---
## 3. Effect IDs (Suggested Mapping)

| ID | Effect |
|---:|:-------|
| 0 | Fire (default) |
| 1 | Candle |
| 2 | Ember |
| 3 | Sparkle |
| 4 | WarmWhite |
|10 | LOTR Cold White / Elvish |
|11 | LOTR Palantír Heartbeat |
|12 | LOTR Many-Color Shimmer |

---
## 4. RGBW Handling (W Dominance)

### Guiding Principle
**The app never sends a direct W channel value.** The device computes W internally.

**Why:**
- SK6812 RGBW strips have wildly varying W intensity.
- Raw W values often wash out saturation or blow out brightness.
- Deriving W from RGB keeps the BLE interface simple and consistent.

### Recommended Device Behavior
1. Convert RGB → RGBW algorithmically.
2. Clamp or scale W to avoid overpowering colors.
3. Allow effects to intentionally use W (e.g., candle, ember, warm white).
4. Keep BLE interface RGB-only.

### Simple Algorithm (Min-subtraction, common & fast)
```cpp
uint8_t w = min({r, g, b});
uint8_t wScaled = (uint16_t)w * whiteScale / 255;

r = max(0, r - wScaled);
g = max(0, g - wScaled);
b = max(0, b - wScaled);

// w = wScaled;
```

### Better (Luminance-based, like WLED Accurate)
- Estimates “how white” the color is from the distribution between max/min.
- Produces more natural-looking pastels and mixed colors.

### Calibration (Recommended)
Since W intensity varies per strip, measure:
- Brightness of RGB white (255,255,255)
- Brightness of pure W (0,0,0,255)

Then compute a `whiteScaleFactor` and apply it to the computed W.

---
## 5. UUID Pattern & Board ID Advertising

### Design Goals
- **Identify Board ID without connecting** (from advertising data)
- **Identify device type and capabilities** before connection
- **Stay within 31-byte legacy advertising limit**
- **Easy to parse in Swift** (CoreBluetooth)
- **Forward-compatible** for future enhancements
- **BLE-compliant** (UUIDs represent services, not device instances)
- **Scalable** to many boards (0..99+)

### UUID Pattern
All UUIDs share the base suffix: `E8F2-537E-4F6C-D104768A1214`.
The first 32-bit group is `19B1xxxx`, where `xxxx` is a characteristic index.

**All boards advertise the same service UUID:**
```
19B10000-E8F2-537E-4F6C-D104768A1214
```

| Purpose | UUID | Index |
|---------|------|-------|
| Service | 19B10000-E8F2-537E-4F6C-D104768A1214 | 0000 |
| effect_id | 19B10001-E8F2-537E-4F6C-D104768A1214 | 0001 |
| brightness | 19B10002-E8F2-537E-4F6C-D104768A1214 | 0002 |
| speed | 19B10003-E8F2-537E-4F6C-D104768A1214 | 0003 |
| color_primary | 19B10004-E8F2-537E-4F6C-D104768A1214 | 0004 |
| color_secondary | 19B10005-E8F2-537E-4F6C-D104768A1214 | 0005 |
| flags | 19B10006-E8F2-537E-4F6C-D104768A1214 | 0006 |
| audio_track_id | 19B10007-E8F2-537E-4F6C-D104768A1214 | 0007 |
| save_config | 19B10008-E8F2-537E-4F6C-D104768A1214 | 0008 |
| device_info | 19B10010-E8F2-537E-4F6C-D104768A1214 | 0010 |
| battery_status | 19B10011-E8F2-537E-4F6C-D104768A1214 | 0011 |

---

### Board ID Encoding (Advertising Data)

**Principle:** Board identity lives in **advertising data**, not UUIDs.

#### Advertising Packet Structure
Use **Manufacturer Specific Data** (AD Type 0xFF) to encode Board ID and capabilities:

```
Byte Layout (7 bytes total):
┌─────────────┬──────────┬──────────┬────────────┬───────────┬────────────┬─────────────┐
│ Company ID  │ Board ID │ Device   │ FW Version │ Pixel     │ Capability │ Reserved    │
│ (2 bytes)   │ (1 byte) │ Type     │ (1 byte)   │ Count     │ Flags      │ (1 byte)    │
│             │          │ (1 byte) │            │ (1 byte)  │ (1 byte)   │             │
└─────────────┴──────────┴──────────┴────────────┴───────────┴────────────┴─────────────┘
  0xFFFF        0-99       0-15       0-255        0-255       bitfield      0x00
```

**Field Definitions:**
- **Company ID** (2 bytes): `0xFFFF` (test/development use per BLE spec)
- **Board ID** (1 byte): `0-99` (unique within installation, 0 = MASTER)
- **Device Type** (1 byte):
  - `0x01` = Arduino Nano 33 BLE
  - `0x02` = ESP32-S3
  - `0x03-0x0F` = Reserved for future hardware
- **FW Version** (1 byte): Firmware version (e.g., `0x10` = v1.0, `0x11` = v1.1)
- **Pixel Count** (1 byte): Number of RGBW pixels (typically 4-32)
- **Capability Flags** (1 byte bitfield):
  - Bit 0: RGBW support
  - Bit 1: FRAM available
  - Bit 2: Audio/DFPlayer support
  - Bit 3: Flash storage
  - Bits 4-7: Reserved
- **Reserved** (1 byte): `0x00` for future use

**Total size:** 7 bytes (well under 31-byte limit, leaves room for device name)

---

#### Arduino Implementation Example

```cpp
void setupAdvertising(uint8_t boardID, uint8_t pixelCount) {
  // Set local name (max 8 chars to stay under 31-byte limit)
  char name[16];
  snprintf(name, sizeof(name), "Orth-%02d", boardID);
  BLE.setLocalName(name);

  // Advertise the Holiday Display service UUID
  BLE.setAdvertisedService(holidayService);

  // Build manufacturer data payload
  uint8_t mfgData[9] = {
    0xFF, 0xFF,           // Company ID (0xFFFF = test/development)
    boardID,              // Board ID (0-99)
    0x01,                 // Device Type (0x01 = Nano 33 BLE)
    0x10,                 // FW Version (1.0)
    pixelCount,           // Pixel count
    0b00000011,           // Capability flags (RGBW + FRAM)
    0x00                  // Reserved
  };

  BLE.setManufacturerData(mfgData, sizeof(mfgData));
  BLE.advertise();
}
```

---

#### Swift Parsing Example (iOS CoreBluetooth)

```swift
func centralManager(_ central: CBCentralManager,
                   didDiscover peripheral: CBPeripheral,
                   advertisementData: [String : Any],
                   rssi RSSI: NSNumber) {

    // Check for manufacturer data
    guard let mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
          mfgData.count >= 9 else { return }

    // Parse manufacturer data
    let companyID = UInt16(mfgData[0]) | (UInt16(mfgData[1]) << 8)
    guard companyID == 0xFFFF else { return }  // Verify it's our format

    let boardID = mfgData[2]
    let deviceType = mfgData[3]
    let fwVersion = mfgData[4]
    let pixelCount = mfgData[5]
    let capabilityFlags = mfgData[6]

    let hasRGBW = (capabilityFlags & 0x01) != 0
    let hasFRAM = (capabilityFlags & 0x02) != 0
    let hasAudio = (capabilityFlags & 0x04) != 0
    let hasFlash = (capabilityFlags & 0x08) != 0

    print("Found Board \(boardID): \(deviceTypeName(deviceType))")
    print("  FW: v\(fwVersion >> 4).\(fwVersion & 0x0F)")
    print("  Pixels: \(pixelCount)")
    print("  Capabilities: RGBW=\(hasRGBW), FRAM=\(hasFRAM), Audio=\(hasAudio)")

    // Can now filter/connect based on Board ID without connecting first
    if boardID == 0 {
        // This is the MASTER board - connect to it first
        central.connect(peripheral, options: nil)
    }
}
```

---

### Board ID Assignment

The assignment of Board IDs is the responsibility of the **Peripheral software component** working in conjunction with **hardware configuration management**.

**Implementation options:**
1. **Compile-time:** Hardcoded in firmware (`#define BOARD_ID 0`)
2. **FRAM/Flash:** Stored in non-volatile memory and read at boot
3. **Hardware switches:** DIP switches or jumpers read via GPIO
4. **First-boot configuration:** Set via BLE on first power-up and saved

**Guidelines:**
- Board ID `0` is reserved for the **MASTER** board in multi-board installations
- Board IDs `1-99` are available for additional boards
- Board IDs should be unique within a single installation/room
- The physical Board ID should be labeled on the device (e.g., on the ceramic base)

---
## 6. Notes & Best Practices
- Keep BLE characteristics lightweight and structured.
- Persist `DisplayConfig` to FRAM/flash when `save_config` is written.
- If BLE disconnects, continue running the last known effect.
- Keep the BLE interface RGB-only and compute W on-device for consistent results.

---
*This protocol document is designed to be the authoritative reference for BLE control of the Orthanc Holiday display, and to guide both firmware and app development.*
