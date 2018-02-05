#include "LiquidCrystal.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define delayMicroseconds(x) vTaskDelay(x/portTICK_PERIOD_MS)

LiquidCrystal::LiquidCrystal(gpio_num_t rs,  gpio_num_t enable,
			     gpio_num_t d0, gpio_num_t d1, gpio_num_t d2, gpio_num_t d3)
{
  _rs_pin = rs;
  _enable_pin = enable;

  _data_pins[0] = d0;
  _data_pins[1] = d1;
  _data_pins[2] = d2;
  _data_pins[3] = d3;

  _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;

  begin(16, 1);
}


void LiquidCrystal::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
	_displayfunction |= LCD_2LINE;
	_numlines = lines;
	setRowOffsets(0x00, 0x40, 0x00 + cols, 0x40 + cols);


	ESP32CPP::GPIO::setOutput(_rs_pin);
	ESP32CPP::GPIO::setOutput(_enable_pin);
	// Do these once, instead of every time a character is drawn for speed reasons.
	for (int i=0; i<((_displayfunction & LCD_8BITMODE) ? 8 : 4); ++i)
	{
		ESP32CPP::GPIO::setOutput(_data_pins[i]);
	 }

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way before 4.5V so we'll wait 50
	delayMicroseconds(50000);
	// Now we pull both RS and R/W low to begin commands
	ESP32CPP::GPIO::low(_rs_pin);
	ESP32CPP::GPIO::low(_enable_pin);

	//put the LCD into 4 bit or 8 bit mode
	if (! (_displayfunction & LCD_8BITMODE)) {
		// this is according to the hitachi HD44780 datasheet
		// figure 24, pg 46

		// we start in 8bit mode, try to set 4 bit mode
		write4bits(0x03);
		delayMicroseconds(4500); // wait min 4.1ms

		// second try
		write4bits(0x03);
		delayMicroseconds(4500); // wait min 4.1ms

		// third go!
		write4bits(0x03);
		delayMicroseconds(150);

		// finally, set to 4-bit interface
		write4bits(0x02);
	} else {
		// this is according to the hitachi HD44780 datasheet
		// page 45 figure 23

		// Send function set command sequence
		command(LCD_FUNCTIONSET | _displayfunction);
		delayMicroseconds(4500);  // wait more than 4.1ms

		// second try
		command(LCD_FUNCTIONSET | _displayfunction);
		delayMicroseconds(150);

		// third go
		command(LCD_FUNCTIONSET | _displayfunction);
	}

	// finally, set # lines, font size, etc.
	command(LCD_FUNCTIONSET | _displayfunction);

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display();

	// clear it off
	clear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	command(LCD_ENTRYMODESET | _displaymode);

}


inline void LiquidCrystal::command(uint8_t value) {
  send(value, 0);
}

inline size_t LiquidCrystal::write(uint8_t value) {
  send(value, 1);
  return 1; // assume sucess
}


void LiquidCrystal::setRowOffsets(int row0, int row1, int row2, int row3)
{
  _row_offsets[0] = row0;
  _row_offsets[1] = row1;
  _row_offsets[2] = row2;
  _row_offsets[3] = row3;
}

void LiquidCrystal::clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delayMicroseconds(2000);  // this command takes a long time!
}

void LiquidCrystal::pulseEnable(void) {
  ESP32CPP::GPIO::low(_enable_pin);
  delayMicroseconds(1);
  ESP32CPP::GPIO::high(_enable_pin);
  delayMicroseconds(1);    // enable pulse must be >450ns
  ESP32CPP::GPIO::low(_enable_pin);
  delayMicroseconds(100);   // commands need > 37us to settle
}

// write either command or data
void LiquidCrystal::send(uint8_t value, uint8_t mode) {
    if(mode)
      ESP32CPP::GPIO::high(_rs_pin);
    else
      ESP32CPP::GPIO::low(_rs_pin);

    write4bits(value>>4);
    write4bits(value);
}


void LiquidCrystal::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LiquidCrystal::write4bits(uint8_t value) {
  for (int i = 0; i < 4; i++) {
    if( (value >> i) & 0x01 ){
      ESP32CPP::GPIO::high(_data_pins[i]);
    }
    else{
      ESP32CPP::GPIO::low(_data_pins[i]);
    }
  }

  pulseEnable();
}
