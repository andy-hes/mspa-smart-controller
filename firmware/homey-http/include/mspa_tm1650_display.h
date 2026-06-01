#pragma once

#include <Arduino.h>
#include <Wire.h>

class MspaTm1650Display {
 public:
  void begin(int sda_pin, int scl_pin) {
    if (initialized_ && sda_pin == sda_pin_ && scl_pin == scl_pin_) return;
    sda_pin_ = sda_pin;
    scl_pin_ = scl_pin;
    Wire.end();
    Wire.begin(sda_pin_, scl_pin_);
    delay(5);
    initialized_ = true;

    setBrightness(7);
    clear();
  }

  void setBrightness(uint8_t brightness) {
    if (brightness > 7) brightness = 7;
    brightness_ = brightness;
    const uint8_t ctrl = static_cast<uint8_t>(0x01 | (brightness_ << 4));
    for (int i = 0; i < 4; i++) writeReg(kCtrlAddr[i], ctrl);
  }

  uint8_t getBrightness() const { return brightness_; }

  void clear() { displayRaw(0x00, 0x00, 0x00, 0x00); }

  void displayRaw(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3 = 0x00) {
    writeReg(kDigitAddr[0], d0);
    writeReg(kDigitAddr[1], d1);
    writeReg(kDigitAddr[2], d2);
    writeReg(kDigitAddr[3], d3);
  }

  void displayNumber3(int value, bool decimal_middle = false) {
    if (value < 0) value = 0;
    if (value > 999) value = 999;

    const int hundreds = (value / 100) % 10;
    const int tens = (value / 10) % 10;
    const int ones = value % 10;

    uint8_t d0 = (value >= 100) ? digitToSeg(hundreds) : 0x00;
    uint8_t d1 = (value >= 10) ? digitToSeg(tens) : 0x00;
    if (decimal_middle) d1 |= 0x80;
    uint8_t d2 = digitToSeg(ones);
    displayRaw(d0, d1, d2, 0x00);
  }

  void displayText3(const char* text) {
    uint8_t out[4] = {0, 0, 0, 0};
    for (int i = 0; i < 3; i++) {
      const char c = text[i];
      if (c == '\0') break;
      out[i] = encodeChar(c);
    }
    displayRaw(out[0], out[1], out[2], out[3]);
  }

 private:
  static constexpr uint8_t kDigitAddr[4] = {0x68, 0x6A, 0x6C, 0x6E};
  static constexpr uint8_t kCtrlAddr[4] = {0x48, 0x4A, 0x4C, 0x4E};

  int sda_pin_ = -1;
  int scl_pin_ = -1;
  bool initialized_ = false;
  uint8_t brightness_ = 7;

  void writeReg(uint8_t addr, uint8_t value) {
    Wire.beginTransmission(addr >> 1);
    Wire.write(value);
    Wire.endTransmission();
  }

  uint8_t digitToSeg(int digit) {
    static constexpr uint8_t kSegMap[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    if (digit < 0 || digit > 9) return 0x00;
    return kSegMap[digit];
  }

  uint8_t encodeChar(char c) {
    switch (c) {
      case '0': return digitToSeg(0);
      case '1': return digitToSeg(1);
      case '2': return digitToSeg(2);
      case '3': return digitToSeg(3);
      case '4': return digitToSeg(4);
      case '5': return digitToSeg(5);
      case '6': return digitToSeg(6);
      case '7': return digitToSeg(7);
      case '8': return digitToSeg(8);
      case '9': return digitToSeg(9);
      case 'A':
      case 'a': return 0x77;
      case 'B':
      case 'b': return 0x7C;
      case 'C':
      case 'c': return 0x39;
      case 'D':
      case 'd': return 0x5E;
      case 'E':
      case 'e': return 0x79;
      case 'F':
      case 'f': return 0x71;
      case 'G':
      case 'g': return 0x6F;
      case 'H':
      case 'h': return 0x76;
      case 'I':
      case 'i': return 0x06;
      case 'J':
      case 'j': return 0x1E;
      case 'L':
      case 'l': return 0x38;
      case 'N':
      case 'n': return 0x54;
      case 'O':
      case 'o': return 0x5C;
      case 'P':
      case 'p': return 0x73;
      case 'R':
      case 'r': return 0x50;
      case 'S':
      case 's': return 0x6D;
      case 'T':
      case 't': return 0x78;
      case 'U':
      case 'u': return 0x3E;
      case '-': return 0x40;
      case '_': return 0x08;
      default: return 0x00;
    }
  }
};
