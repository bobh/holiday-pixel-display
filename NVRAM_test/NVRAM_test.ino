//NVRAM_test

#include <Wire.h>

static const uint8_t candidateAddresses[] = { 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57 };

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println(F("FRAM test starting..."));
  Wire.begin();

  uint8_t foundAddr = 0x00;
  for (uint8_t addr : candidateAddresses) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Found I2C device at 0x"));
      Serial.println(addr, HEX);
      foundAddr = addr;
      break;
    }
  }

  if (!foundAddr) {
    Serial.println(F("No device found in 0x50-0x57 range."));
    return;
  }

  const uint16_t testAddr = 0x0000;
  const uint8_t writeData[] = { 'K', 'I', 'C', 'A', 'D', '!' };
  const size_t len = sizeof(writeData);

  // Write test pattern
  Wire.beginTransmission(foundAddr);
  Wire.write((testAddr >> 8) & 0xFF);
  Wire.write(testAddr & 0xFF);
  Wire.write(writeData, len);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("Write failed"));
    return;
  }

  delay(10);

  // Read it back
  Wire.beginTransmission(foundAddr);
  Wire.write((testAddr >> 8) & 0xFF);
  Wire.write(testAddr & 0xFF);
  if (Wire.endTransmission(false) != 0) {
    Serial.println(F("Random read setup failed"));
    return;
  }

  Wire.requestFrom(foundAddr, len);
  uint8_t readData[len];
  for (size_t i = 0; i < len && Wire.available(); ++i) {
    readData[i] = Wire.read();
  }

  Serial.print(F("Read: "));
  for (size_t i = 0; i < len; ++i) Serial.write(readData[i]);
  Serial.println();

  Serial.println(memcmp(writeData, readData, len) == 0
                 ? F("FRAM read/write OK")
                 : F("FRAM mismatch"));
}

void loop() {
  // no-op
}
