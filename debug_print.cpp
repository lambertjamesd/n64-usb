#include "debug_print.h"

#include <Arduino.h>

char gHexCharacter[] = {
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'A', 'B',
  'C', 'D', 'E', 'F',
};

void printHex(uint8_t value) {
  Serial.write(gHexCharacter[value >> 4]);
  Serial.write(gHexCharacter[value & 0xF]);
}

void printBinary(uint8_t value) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (value & 0x80) {
      Serial.write('1');
    } else {
      Serial.write('0');
    }

    value <<= 1;
  }
}

void debugPrintBuffer(uint8_t* data, uint8_t bytes) {
  for (uint8_t i = 0; i < bytes; ++i) {
    printHex(data[i]);

    if ((i & 0x7) == 0x7) {
      Serial.write('\n');
    } else {
      Serial.write(' ');
    }
  }

  if ((bytes & 0x7) != 0) {
    Serial.write('\n');
  }
}