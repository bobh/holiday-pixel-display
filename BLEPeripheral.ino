#include <ArduinoBLE.h>
#include <Wire.h>

// ------------------------------------------------------------
// BLE Service / Characteristic UUIDs (Holiday Display Control)
// ------------------------------------------------------------
static const char* SERVICE_UUID        = "19B10000-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_DEVICE_INFO    = "19B10010-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_BATTERY        = "19B10011-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_EFFECT_ID      = "19B10001-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_BRIGHTNESS     = "19B10002-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_SPEED          = "19B10003-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_COLOR_PRIMARY  = "19B10004-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_COLOR_SECONDARY= "19B10005-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_FLAGS          = "19B10006-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_AUDIO_TRACK    = "19B10007-E8F2-537E-4F6C-D104768A1214";
static const char* CHAR_SAVE_CONFIG    = "19B10008-E8F2-537E-4F6C-D104768A1214";

// ------------------------------------------------------------
// Board identity & advertising configuration (Protocol §5)
// ------------------------------------------------------------
static const uint8_t BOARD_ID       = 1;     // 0 = MASTER, 1-99 for additional boards
static const uint8_t DEVICE_TYPE    = 0x01;  // 0x01 = Nano 33 BLE
static const uint8_t FW_VERSION     = 0x10;  // v1.0
static const uint8_t PIXEL_COUNT    = 8;
static const uint8_t CAP_FLAGS      = 0x03;  // Bit 0: RGBW, Bit 1: FRAM

// ------------------------------------------------------------
// Config structure used by both BLE and the display engine
// ------------------------------------------------------------
struct DisplayConfig {
  uint8_t  effect_id;
  uint8_t  brightness;
  uint8_t  speed;
  uint32_t color_primary;     // RGB only
  uint32_t color_secondary;   // RGB only
  uint8_t  audio_track_id;    // ignored on Nano
  uint8_t  flags;
};

// ------------------------------------------------------------
// External display instance (defined in HolidayDisplay.ino)
// ------------------------------------------------------------
extern class PixelDisplay display;

static DisplayConfig gConfig = {
  .effect_id = 0,
  .brightness = 128,
  .speed = 128,
  .color_primary = 0xFFFFFF,
  .color_secondary = 0x000000,
  .audio_track_id = 0,
  .flags = 0,
};

static BLEService holidayService(SERVICE_UUID);
static BLECharacteristic charDeviceInfo(CHAR_DEVICE_INFO, BLERead, 32);
static BLECharacteristic charBattery(CHAR_BATTERY, BLERead | BLENotify, 2);
static BLECharacteristic charEffectId(CHAR_EFFECT_ID, BLERead | BLEWrite, 1);
static BLECharacteristic charBrightness(CHAR_BRIGHTNESS, BLERead | BLEWrite, 1);
static BLECharacteristic charSpeed(CHAR_SPEED, BLERead | BLEWrite, 1);
static BLECharacteristic charColorPrimary(CHAR_COLOR_PRIMARY, BLERead | BLEWrite, 4);
static BLECharacteristic charColorSecondary(CHAR_COLOR_SECONDARY, BLERead | BLEWrite, 4);
static BLECharacteristic charFlags(CHAR_FLAGS, BLERead | BLEWrite, 1);
static BLECharacteristic charAudioTrack(CHAR_AUDIO_TRACK, BLERead | BLEWrite, 1);
static BLECharacteristic charSaveConfig(CHAR_SAVE_CONFIG, BLERead | BLEWrite | BLENotify, 1);

// Forward declarations
static void applyConfig();
static void notifyBattery();
static void handleWrites();
static bool saveConfig();

void setupBLE() {
  if (!BLE.begin()) {
    // Fail silently; the board should still drive LEDs even without BLE.
    return;
  }

  // NOTE: Local name omitted intentionally — with a 128-bit service UUID +
  // 8-byte manufacturer data, the advertisement is already at the 31-byte BLE
  // limit. Adding a local name overflows the packet and causes ArduinoBLE to
  // silently drop the service UUID, making the peripheral invisible to iOS
  // service-filtered scanning.  Board identity is available via manufacturer
  // data bytes and the device_info characteristic after connection.
  BLE.setAdvertisedService(holidayService);

  // Manufacturer Specific Data for pre-connection identification (Protocol §5)
  // Byte 7: Config state — 0x00 = Unconfigured, 0x01 = Configured (Option A)
  // Initialized from current state so scanners see correct status immediately after boot.
  uint8_t mfgData[8] = {
    0xFF, 0xFF,       // Company ID (0xFFFF = test/development per BLE spec)
    BOARD_ID,         // Board ID (0-99, 0 = MASTER)
    DEVICE_TYPE,      // Device Type (0x01 = Nano 33 BLE)
    FW_VERSION,       // FW Version (0x10 = v1.0)
    PIXEL_COUNT,      // Pixel count
    CAP_FLAGS,        // Capability flags (RGBW + FRAM)
    (state == CONFIGURED) ? (uint8_t)0x01 : (uint8_t)0x00,  // Config state
  };
  BLE.setManufacturerData(mfgData, sizeof(mfgData));

  // Device info (read-only)
  char deviceInfo[32];
  snprintf(deviceInfo, sizeof(deviceInfo), "NANO33BLE,fw1.0,RGBW,FRAM,%dpx", PIXEL_COUNT);
  charDeviceInfo.setValue(deviceInfo);

  // Initial values
  charEffectId.setValue(&gConfig.effect_id, 1);
  charBrightness.setValue(&gConfig.brightness, 1);
  charSpeed.setValue(&gConfig.speed, 1);
  charColorPrimary.setValue((const uint8_t*)&gConfig.color_primary, 4);
  charColorSecondary.setValue((const uint8_t*)&gConfig.color_secondary, 4);
  charFlags.setValue(&gConfig.flags, 1);
  charAudioTrack.setValue(&gConfig.audio_track_id, 1);
  uint8_t noSave = 0;
  charSaveConfig.setValue(&noSave, 1);

  // Add all characteristics
  holidayService.addCharacteristic(charDeviceInfo);
  holidayService.addCharacteristic(charBattery);
  holidayService.addCharacteristic(charEffectId);
  holidayService.addCharacteristic(charBrightness);
  holidayService.addCharacteristic(charSpeed);
  holidayService.addCharacteristic(charColorPrimary);
  holidayService.addCharacteristic(charColorSecondary);
  holidayService.addCharacteristic(charFlags);
  holidayService.addCharacteristic(charAudioTrack);
  holidayService.addCharacteristic(charSaveConfig);

  BLE.addService(holidayService);
  BLE.advertise();

  applyConfig();
}

void updateBLE() {
  BLEDevice central = BLE.central();
  static bool wasConnected = false;

  // Detect new connection
  if (central && !wasConnected) {
    wasConnected = true;
    state = CONFIGURING;
    setStatusLED(0, 0, 255);   // BLUE — central connected, configuring
  }

  // Detect disconnection
  if (!central && wasConnected) {
    wasConnected = false;
    if (state == CONFIGURED) {
      // Config was saved this session — keep running it.
      setStatusLED(0, 255, 0); // GREEN
      updateAdvertisingState(0x01);
    } else if (framHasValidConfig()) {
      // Disconnected without saving but a prior config exists in FRAM.
      // Revert to it so pixels keep running and LED stays green.
      Wire.begin();
      loadConfigFromFRAM();
      state = CONFIGURED;
      setStatusLED(0, 255, 0); // GREEN — reverted to saved config
      updateAdvertisingState(0x01);
    } else {
      // No saved config at all — go dark.
      state = UNCONFIGURED;
      setStatusLED(255, 255, 0); // YELLOW
      updateAdvertisingState(0x00);
    }
    BLE.setAdvertisedService(holidayService);
    BLE.advertise();
    return;
  }

  if (!central) {
    return;
  }

  // Handle writes from the central
  handleWrites();

  // Update battery status occasionally
  static unsigned long lastBatteryUpdate = 0;
  if (millis() - lastBatteryUpdate > 2000) {
    lastBatteryUpdate = millis();
    notifyBattery();
  }
}

// Update byte 7 of manufacturer advertising data with current config state.
// Called after disconnect so the scanner sees the updated status on next scan.
static void updateAdvertisingState(uint8_t configByte) {
  uint8_t mfgData[8] = {
    0xFF, 0xFF,
    BOARD_ID,
    DEVICE_TYPE,
    FW_VERSION,
    PIXEL_COUNT,
    CAP_FLAGS,
    configByte,
  };
  BLE.setManufacturerData(mfgData, sizeof(mfgData));
}

static void handleWrites() {
  if (charEffectId.written()) {
    uint8_t value;
    charEffectId.readValue(&value, 1);
    gConfig.effect_id = value;
    applyConfig();
  }

  if (charBrightness.written()) {
    uint8_t value;
    charBrightness.readValue(&value, 1);
    gConfig.brightness = value;
    applyConfig();
  }

  if (charSpeed.written()) {
    uint8_t value;
    charSpeed.readValue(&value, 1);
    gConfig.speed = value;
    applyConfig();
  }

  if (charColorPrimary.written()) {
    uint32_t value;
    charColorPrimary.readValue((uint8_t*)&value, 4);
    gConfig.color_primary = value;
    applyConfig();
  }

  if (charColorSecondary.written()) {
    uint32_t value;
    charColorSecondary.readValue((uint8_t*)&value, 4);
    gConfig.color_secondary = value;
    applyConfig();
  }

  if (charFlags.written()) {
    uint8_t value;
    charFlags.readValue(&value, 1);
    gConfig.flags = value;
  }

  if (charAudioTrack.written()) {
    uint8_t value;
    charAudioTrack.readValue(&value, 1);
    gConfig.audio_track_id = value;
  }

  if (charSaveConfig.written()) {
    uint8_t value;
    charSaveConfig.readValue(&value, 1);
    if (value == 1) {
      bool saved = saveConfig(); // Persist to FRAM and verify
      uint8_t ack = saved ? 1 : 0;
      if (saved) {
        state = CONFIGURED;
        setStatusLED(0, 255, 0); // GREEN — config saved
      }
      charSaveConfig.writeValue(&ack, 1);
      // Advertising state byte updated on disconnect (central is still connected here)
    }
  }
}

static bool isValidEffectId(uint8_t id) {
  switch (id) {
    case 0: case 1: case 2: case 3: case 4:   // Fire..WarmWhite
    case 10: case 11: case 12:                  // LOTR effects
      return true;
    default:
      return false;
  }
}

static void applyConfig() {
  // Validate against defined effect IDs (non-contiguous per Protocol §3)
  Effect effect = Effect::Fire;
  if (isValidEffectId(gConfig.effect_id)) {
    effect = (Effect)gConfig.effect_id;
  }

  display.setEffect(effect);
  display.setBrightness(gConfig.brightness);
  display.setSpeed(gConfig.speed);
  display.setPrimaryColor(gConfig.color_primary);
  display.setSecondaryColor(gConfig.color_secondary);
}

static void notifyBattery() {
  // Send one-shot notification only on power status transitions.
  // currentPowerStatus and powerStatusChanged are owned by HolidayDisplay.ino.
  // Voltage values never leave the Arduino — iPhone receives the status code only.
  if (!powerStatusChanged) return;
  powerStatusChanged = false;
  uint8_t data[2] = { static_cast<uint8_t>(currentPowerStatus), 0x00 };
  charBattery.writeValue(data, 2);
}

// ------------------------------------------------------------
// FRAM persistence — FM24CL16B at I2C addresses 0x50-0x57
// Layout:
//   0x0000        : magic byte (0xA5 = valid config present)
//   0x0001–0x000D : DisplayConfig struct (13 bytes)
// Magic is written AFTER config so a power failure mid-write
// leaves framHasValidConfig() returning false rather than
// loading a partially-written struct.
// ------------------------------------------------------------
static const uint8_t  FRAM_I2C_ADDR   = 0x50;
static const uint16_t FRAM_MAGIC_ADDR  = 0x0000;
static const uint16_t FRAM_CONFIG_ADDR = 0x0001;
static const uint8_t  FRAM_MAGIC_BYTE  = 0xA5;

static uint8_t framDeviceAddress(uint16_t addr) {
  return FRAM_I2C_ADDR | ((addr >> 8) & 0x07);
}

static bool framWrite(uint16_t addr, const uint8_t* data, uint8_t len) {
  while (len > 0) {
    uint8_t deviceAddr = framDeviceAddress(addr);
    uint8_t wordAddr = addr & 0xFF;
    uint8_t chunkLen = min<uint8_t>(len, 256 - wordAddr);

    Wire.beginTransmission(deviceAddr);
    Wire.write(wordAddr);
    Wire.write(data, chunkLen);
    if (Wire.endTransmission() != 0) return false;

    addr += chunkLen;
    data += chunkLen;
    len -= chunkLen;
  }
  return true;
}

static bool framRead(uint16_t addr, uint8_t* data, uint8_t len) {
  while (len > 0) {
    uint8_t deviceAddr = framDeviceAddress(addr);
    uint8_t wordAddr = addr & 0xFF;
    uint8_t chunkLen = min<uint8_t>(len, 256 - wordAddr);

    Wire.beginTransmission(deviceAddr);
    Wire.write(wordAddr);
    if (Wire.endTransmission(false) != 0) return false;

    uint8_t received = Wire.requestFrom(deviceAddr, chunkLen);
    if (received != chunkLen) return false;
    for (uint8_t i = 0; i < chunkLen; i++) {
      if (!Wire.available()) return false;
      data[i] = Wire.read();
    }

    addr += chunkLen;
    data += chunkLen;
    len -= chunkLen;
  }
  return true;
}

bool framHasValidConfig() {
  uint8_t magic = 0;
  if (!framRead(FRAM_MAGIC_ADDR, &magic, 1)) return false;
  return magic == FRAM_MAGIC_BYTE;
}

void loadConfigFromFRAM() {
  Wire.begin();
  if (!framRead(FRAM_CONFIG_ADDR, (uint8_t*)&gConfig, (uint8_t)sizeof(gConfig))) return;
  applyConfig();
}

static bool saveConfig() {
  // Re-initialize Wire before FRAM access. On NRF52840, BLE.begin() can
  // leave the I2C peripheral in a state that silently drops transactions.
  // Calling Wire.begin() here is safe — it reinitializes without side effects.
  Wire.begin();

  if (!framWrite(FRAM_CONFIG_ADDR, (uint8_t*)&gConfig, (uint8_t)sizeof(gConfig))) {
    return false;
  }
  uint8_t magic = FRAM_MAGIC_BYTE;
  if (!framWrite(FRAM_MAGIC_ADDR, &magic, 1)) {
    return false;
  }

  uint8_t verifyMagic = 0;
  DisplayConfig verifyConfig = {};
  if (!framRead(FRAM_MAGIC_ADDR, &verifyMagic, 1)) return false;
  if (verifyMagic != FRAM_MAGIC_BYTE) return false;
  if (!framRead(FRAM_CONFIG_ADDR, (uint8_t*)&verifyConfig, (uint8_t)sizeof(verifyConfig))) return false;
  return memcmp(&verifyConfig, &gConfig, sizeof(gConfig)) == 0;
}
