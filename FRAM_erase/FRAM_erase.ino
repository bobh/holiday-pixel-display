// FRAM_erase.ino
// Diagnostic and erase utility for FM24CL16B on HolidayDisplay PCB.
// Reads the first 16 bytes of FRAM, reports whether a valid config
// is present, then erases by zeroing the magic byte at 0x0000.

#include <Wire.h>

static const uint8_t  FRAM_ADDR  = 0x50;
static const uint8_t  MAGIC_BYTE = 0xA5;

static void framWriteByte(uint16_t addr, uint8_t val) {
  Wire.beginTransmission(FRAM_ADDR);
  Wire.write((addr >> 8) & 0xFF);
  Wire.write(addr & 0xFF);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t framReadByte(uint16_t addr) {
  Wire.beginTransmission(FRAM_ADDR);
  Wire.write((addr >> 8) & 0xFF);
  Wire.write(addr & 0xFF);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom(FRAM_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Wire.begin();

  Serial.println(F("--- FRAM Diagnostic ---"));

  // Dump first 16 bytes (magic + DisplayConfig)
  Serial.println(F("Addr  : Value"));
  for (uint16_t i = 0; i < 16; i++) {
    uint8_t b = framReadByte(i);
    Serial.print(F("0x"));
    if (i < 0x10) Serial.print(F("000"));
    Serial.print(i, HEX);
    Serial.print(F(" : 0x"));
    if (b < 0x10) Serial.print(F("0"));
    Serial.println(b, HEX);
  }

  // Report magic byte status
  uint8_t magic = framReadByte(0x0000);
  Serial.println();
  Serial.print(F("Magic byte 0x0000 = 0x"));
  Serial.println(magic, HEX);
  if (magic == MAGIC_BYTE) {
    Serial.println(F("=> Valid config present"));
    Serial.print(F("   effect_id  : "));  Serial.println(framReadByte(0x0001));
    Serial.print(F("   brightness : "));  Serial.println(framReadByte(0x0002));
    Serial.print(F("   speed      : "));  Serial.println(framReadByte(0x0003));
  } else {
    Serial.println(F("=> No valid config (magic byte not 0xA5)"));
  }

  // Erase: zero the magic byte so firmware boots UNCONFIGURED
  Serial.println();
  Serial.println(F("Erasing magic byte..."));
  framWriteByte(0x0000, 0x00);
  uint8_t verify = framReadByte(0x0000);
  if (verify == 0x00) {
    Serial.println(F("Erase OK — board will boot UNCONFIGURED on next power cycle."));
  } else {
    Serial.print(F("Erase FAILED — 0x0000 still reads 0x"));
    Serial.println(verify, HEX);
  }
}

void loop() {}
