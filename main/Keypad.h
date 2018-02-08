
#ifndef KEYPAD_H
#define KEYPAD_H

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "GPIO.h"


#define makeKeymap(x) ((char*)x)

#define OUTPUT 0
#define INPUT 1
#define INPUT_PULLUP 2

#define HIGH 1
#define LOW 0

#define ROWS 4 //four rows
#define COLS 4 //four columns

#define keypad_row1_pin GPIO_NUM_39
#define keypad_row2_pin GPIO_NUM_34
#define keypad_row3_pin GPIO_NUM_35
#define keypad_row4_pin GPIO_NUM_32
#define keypad_col1_pin GPIO_NUM_33
#define keypad_col2_pin GPIO_NUM_25
#define keypad_col3_pin GPIO_NUM_26
#define keypad_col4_pin GPIO_NUM_27




class Keypad {
public:

	Keypad();
    char getKey();


private:
    void pin_mode(gpio_num_t _pin, int mode);
    void digital_write(gpio_num_t _pin, int level);
    int digital_read(gpio_num_t _pin);
    void begin();

    char *keymap;



};

#endif
