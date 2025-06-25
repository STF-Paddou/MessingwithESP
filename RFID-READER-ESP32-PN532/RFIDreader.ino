#include <Wire.h>
#include <Adafruit_PN532.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define PN532_IRQ   (2)
#define PN532_RESET (3)

TwoWire myWire = TwoWire(0);  // Use I2C bus 0 for ESP32-C3

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &myWire);

// Custom UID you want to write (4 bytes)
uint8_t newUid[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

bool magicCardBackdoorUnlock() {
  // Send HALT command
  uint8_t halt[] = { 0x50, 0x00, 0x00, 0x00 };
  nfc.inDataExchange(halt, 2, halt); // Halt card

  // Magic backdoor commands
  uint8_t cmd1[] = { 0x40 };
  uint8_t response[8];
  if (!nfc.inDataExchange(cmd1, 1, response)) return false;
  if (response[0] != 0x0A) return false;

  uint8_t cmd2[] = { 0x43 };
  if (!nfc.inDataExchange(cmd2, 1, response)) return false;
  if (response[0] != 0x0A) return false;

  return true;
}

bool writeUid(uint8_t *newUid) {
  // Prepare block 0 data
  uint8_t block0[16] = {0};

  // Copy UID
  memcpy(block0, newUid, 4);

  // Calculate BCC
  block0[4] = newUid[0] ^ newUid[1] ^ newUid[2] ^ newUid[3];

  // Add default manufacturer bytes and fill the rest with zeros
  block0[5] = 0xFF; // Manufacturer byte, arbitrary
  memset(block0 + 6, 0x00, 10);

  return nfc.mifareclassic_WriteDataBlock(0, block0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  myWire.begin(SDA_PIN, SCL_PIN);
  Serial.println("Looking for PN532 via I2C...");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board :(");
    while (1) delay(10);
  }

  Serial.print("Found PN532! Firmware version: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print('.');
  Serial.println((versiondata >> 16) & 0xFF, HEX);

  nfc.SAMConfig();
  Serial.println("Waiting for an ISO14443A card...");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.print("Found card with UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    if (magicCardBackdoorUnlock()) {
      Serial.println("Magic card unlocked! Writing UID...");
      if (writeUid(newUid)) {
        Serial.println("UID successfully written!");
      } else {
        Serial.println("Failed to write UID.");
      }
    } else {
      Serial.println("Failed to unlock magic card. Not a writable UID card?");
    }

    delay(3000);  // Wait before next read attempt
  }
}
