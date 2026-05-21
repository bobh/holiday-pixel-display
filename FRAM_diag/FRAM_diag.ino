// FRAM_diag.ino
// Non-destructive diagnostic for FM24CL16B on HolidayDisplay PCB.

#include <Wire.h>

static const uint8_t  FRAM_BASE_ADDR   = 0x50;
static const uint16_t FRAM_MAGIC_ADDR  = 0x0000;
static const uint16_t FRAM_CONFIG_ADDR = 0x0001;
static const uint16_t FRAM_TEST_ADDR   = 0x07F0;
static const uint8_t  FRAM_MAGIC_BYTE  = 0xA5;

static uint8_t framDeviceAddress(uint16_t addr) {
  return FRAM_BASE_ADDR | ((addr >> 8) & 0x07);
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

static void printHexByte(uint8_t b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

static void dumpBytes(uint16_t start, uint8_t len) {
  uint8_t buf[16];
  while (len > 0) {
    uint8_t chunk = min<uint8_t>(len, sizeof(buf));
    Serial.print(F("0x"));
    if (start < 0x1000) Serial.print('0');
    if (start < 0x0100) Serial.print('0');
    if (start < 0x0010) Serial.print('0');
    Serial.print(start, HEX);
    Serial.print(F(": "));

    if (!framRead(start, buf, chunk)) {
      Serial.println(F("READ FAILED"));
      return;
    }

    for (uint8_t i = 0; i < chunk; i++) {
      printHexByte(buf[i]);
      Serial.print(' ');
    }
    Serial.println();

    start += chunk;
    len -= chunk;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin();
  Serial.println(F("--- FM24CL16B FRAM Diagnostic ---"));

  Serial.println(F("I2C scan 0x50-0x57:"));
  bool found = false;
  for (uint8_t addr = 0x50; addr <= 0x57; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    Serial.print(F("  0x"));
    Serial.print(addr, HEX);
    Serial.print(F(": "));
    Serial.println(err == 0 ? F("ACK") : F("no ACK"));
    found = found || (err == 0);
  }

  if (!found) {
    Serial.println(F("No FM24CL16B ACKs found. Check SDA/SCL rework, power, and pullups."));
    return;
  }

  Serial.println();
  Serial.println(F("Current config area:"));
  dumpBytes(FRAM_MAGIC_ADDR, 16);

  uint8_t magic = 0;
  if (framRead(FRAM_MAGIC_ADDR, &magic, 1)) {
    Serial.print(F("Magic byte: 0x"));
    printHexByte(magic);
    Serial.println(magic == FRAM_MAGIC_BYTE ? F(" valid") : F(" invalid"));
  } else {
    Serial.println(F("Magic byte read failed."));
  }

  Serial.println();
  Serial.println(F("Non-destructive write/read test at 0x07F0:"));
  uint8_t saved[4] = {};
  uint8_t pattern[4] = { 0x48, 0x44, 0x31, 0x36 }; // "HD16"
  uint8_t verify[4] = {};

  bool savedOk = framRead(FRAM_TEST_ADDR, saved, sizeof(saved));
  bool writeOk = framWrite(FRAM_TEST_ADDR, pattern, sizeof(pattern));
  bool verifyOk = framRead(FRAM_TEST_ADDR, verify, sizeof(verify));
  bool match = verifyOk && memcmp(pattern, verify, sizeof(pattern)) == 0;
  bool restoreOk = savedOk && framWrite(FRAM_TEST_ADDR, saved, sizeof(saved));

  Serial.print(F("  save old bytes : ")); Serial.println(savedOk ? F("OK") : F("FAIL"));
  Serial.print(F("  write pattern  : ")); Serial.println(writeOk ? F("OK") : F("FAIL"));
  Serial.print(F("  read verify    : ")); Serial.println(verifyOk ? F("OK") : F("FAIL"));
  Serial.print(F("  verify match   : ")); Serial.println(match ? F("OK") : F("FAIL"));
  Serial.print(F("  restore old    : ")); Serial.println(restoreOk ? F("OK") : F("FAIL"));

  Serial.println(F("--- Done ---"));
}

void loop() {}
