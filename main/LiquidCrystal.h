#ifndef LiquidCrystal_h
#define LiquidCrystal_h

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "GPIO.h"


#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00


class LiquidCrystal {
public:
    LiquidCrystal(gpio_num_t rs, gpio_num_t enable,
                  gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3);
    void begin(uint8_t cols, uint8_t rows);
    void clear();
    void home();
    void setCursor(uint8_t row, uint8_t col);
    void command(uint8_t);
    void setRowOffsets(int row1, int row2, int row3, int row4);
    void display();
    virtual size_t write(uint8_t);
    virtual size_t write_word(char *s);
    // using Print::write;
private:
    void send(uint8_t, uint8_t);
    void write4bits(uint8_t);
    void write8bits(uint8_t);
    void pulseEnable();

    gpio_num_t _rs_pin; // LOW: command.  HIGH: character.
    gpio_num_t _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
    gpio_num_t _enable_pin; // activated by a HIGH pulse.
    gpio_num_t _data_pins[8];

    uint8_t _displayfunction;
    uint8_t _displaycontrol;
    uint8_t _displaymode;

    uint8_t _initialized;

    uint8_t _numlines;
    uint8_t _row_offsets[4];
};

#endif
