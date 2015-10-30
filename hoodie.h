/**
 * Copyright (c) 2015 FirstBuild
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "font.h"

#ifndef HOODIE_H
#define HOODIE_H

#define HOODIE_VERSION_MAJOR 1
#define HOODIE_VERSION_MINOR 0
#define HOODIE_VERSION_PATCH 1

#define PIN_ROW_A 9
#define PIN_ROW_B 10
#define PIN_ROW_C 11
#define PIN_ROW_D 12
#define PIN_G1    13
#define PIN_STB   A1
#define PIN_CLK   A2
#define PIN_R1    A3
#define PIN_OE    A4

#define HOODIE_SCREEN_WIDTH  32
#define HOODIE_SCREEN_HEIGHT 16
#define HOODIE_SCROLL_RATE   20

#define HOODIE_SCREEN_WIDTH_IN_BYTES (HOODIE_SCREEN_WIDTH / 8)

#define BLE_NAME "Hoodie"
#define BLE_PACKET_SIZE 200
#define BLE_PACKET_DELAY 5000

class CHoodie {
  private:
    int _unlocked;
    char _text[BLE_PACKET_SIZE + 2 * HOODIE_SCREEN_WIDTH];
    unsigned long _textWidth;
    unsigned long _columnOffset;
    int _rowToDisplay;
    char _displayBuffer[HOODIE_SCREEN_HEIGHT][HOODIE_SCREEN_WIDTH_IN_BYTES];
    char _packetBuffer[BLE_PACKET_SIZE];
    int _packetSize;
    unsigned long _previousMilliseconds;

  public:
    CHoodie(void) :
      _unlocked(true),
      _text(),
      _textWidth(0),
      _columnOffset(0),
      _rowToDisplay(0),
      _displayBuffer(),
      _packetBuffer(),
      _packetSize(0),
      _previousMilliseconds(0) { }

    void begin(void) {
      pinMode(PIN_R1, OUTPUT);
      pinMode(PIN_OE, OUTPUT);
      pinMode(PIN_ROW_A, OUTPUT);
      pinMode(PIN_ROW_B, OUTPUT);
      pinMode(PIN_ROW_C, OUTPUT);
      pinMode(PIN_ROW_D, OUTPUT);
      pinMode(PIN_G1, INPUT);
      pinMode(PIN_STB, OUTPUT);
      pinMode(PIN_CLK, OUTPUT);

      clear();

      ble_set_name(BLE_NAME);
      ble_begin();
      delay(1000);
    }

    void loop(void) {
      unsigned long current = millis();

      if ((current - _previousMilliseconds) > HOODIE_SCROLL_RATE) {
        _previousMilliseconds = current;

        if (_textWidth > HOODIE_SCREEN_WIDTH) {
          _columnOffset = (_columnOffset + 1) % _textWidth;
          updateDisplay();
        }
      }
      
      if (ble_available()) {
        while (ble_available()) {
          char c = ble_read();

          if (c == 0) {
            _packetBuffer[_packetSize] = 0;
            _packetSize = 0;

            setText(_packetBuffer);
          }
          else if (_packetSize < sizeof(_packetBuffer)) {
            _packetBuffer[_packetSize++] = c;
          }
        }
      }

      ble_do_events();
    }

    void tick(void) {
      for (int i = 0; i < HOODIE_SCREEN_WIDTH_IN_BYTES; i++) {
        shiftOut(PIN_R1, PIN_CLK, MSBFIRST, _displayBuffer[_rowToDisplay][i]);
      }

      digitalWrite(PIN_OE, 1);
      digitalWrite(PIN_ROW_A, _rowToDisplay & 1);
      digitalWrite(PIN_ROW_B, _rowToDisplay & 2);
      digitalWrite(PIN_ROW_C, _rowToDisplay & 4);
      digitalWrite(PIN_ROW_D, _rowToDisplay & 8);
      digitalWrite(PIN_STB, 0);
      digitalWrite(PIN_STB, 1);
      digitalWrite(PIN_OE, 0);

      _rowToDisplay = (_rowToDisplay + 1) % HOODIE_SCREEN_HEIGHT;
    }

    void clear(void) {
      memset(_displayBuffer, 0xff, sizeof(_displayBuffer));
    }

    void setPixel(int row, int column, int on) {
      int index = HOODIE_SCREEN_WIDTH_IN_BYTES - (column / 8) - 1;
      int offset = column % 8;
      int mask = 1 << offset;

      if (row < HOODIE_SCREEN_HEIGHT) {
        if (column < HOODIE_SCREEN_WIDTH) {
          if (on) {
            _displayBuffer[row][index] &= ~mask;
          }
          else {
            _displayBuffer[row][index] |= mask;
          }
        }
      }
    }

    void setColumn(int column, int pixels) {
      for (int i = 0; i < HOODIE_SCREEN_HEIGHT; i++) {
        setPixel(i, column, pixels & (1 << i));
      }
    }

    void addLetter(unsigned long& column, const int *buffer, int width) {
      for (int i = 0; i < width; i++, column++) {
        int offset = column - _columnOffset;
        
        if (column < _columnOffset) {
          /* skip */
        }
        else if (offset > HOODIE_SCREEN_WIDTH) {
          break;
        }
        else {
          setColumn(offset, buffer[i]);
        }
      }
    }

    void updateDisplay(void) {
      unsigned long c = 0;
      int space = 0;

      for (char *text = _text; *text; text++) {
        int width = Font::getWidth(*text);
        const int *buffer = Font::getBuffer(*text);

        addLetter(c, buffer, width);
        addLetter(c, &space, 1);
      }
    }

    static int getWidth(const char *text) {
      unsigned long w;
      
      for (w = 0; *text; text++) {
        w += Font::getWidth(*text) + 1;
      }

      return w;
    }

    void setText(const char *text) {
      _columnOffset = 0;
      _textWidth = getWidth(text);

      if (_textWidth > HOODIE_SCREEN_WIDTH) {
        int textLength = strlen(text);

        _textWidth += 2 * HOODIE_SCREEN_WIDTH;
        
        memset(_text, ' ', HOODIE_SCREEN_WIDTH);
        memcpy(_text + HOODIE_SCREEN_WIDTH, text, textLength);
        memset(_text + HOODIE_SCREEN_WIDTH + textLength, ' ', HOODIE_SCREEN_WIDTH);
        _text[2 * HOODIE_SCREEN_WIDTH + textLength] = 0;

      }
      else {
        strncpy(_text, text, sizeof(_text));
      }

      clear();
      updateDisplay();
    }
};

CHoodie Hoodie;

#endif /* HOODIE_H */
