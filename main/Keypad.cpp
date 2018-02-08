
#include "Keypad.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "GPIO.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


char keypadKeys[ROWS][COLS] = {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
};


gpio_num_t rowPins[ROWS] = {keypad_row1_pin, keypad_row2_pin, keypad_row3_pin, keypad_row4_pin};
gpio_num_t colPins[COLS] = {keypad_col1_pin, keypad_col2_pin, keypad_col3_pin, keypad_col4_pin};


Keypad::Keypad(){

	begin();

}

void Keypad::pin_mode(gpio_num_t _pin, int mode){
    switch(mode){
        case OUTPUT:
            ESP32CPP::GPIO::setOutput(_pin);
//            printf("Setando como saída\n");
            break;
        case INPUT:
            ESP32CPP::GPIO::setInput(_pin);
            break;
        case INPUT_PULLUP:
            ESP32CPP::GPIO::setInput(_pin);
            break;
//            digital_write(_pin, HIGH);
    }

}

void Keypad::digital_write(gpio_num_t _pin, int level){
    if(level == HIGH)
        ESP32CPP::GPIO::high(_pin);
    else
        ESP32CPP::GPIO::low(_pin);
}
int Keypad::digital_read(gpio_num_t _pin){
    return ESP32CPP::GPIO::read(_pin);
}



//
//// Let the user define a keymap - assume the same row/column count as defined in constructor
void Keypad::begin() {

    for(int i=0; i< ROWS; i++){
        pin_mode(rowPins[i], INPUT);
//        printf("Setando linha %d como INPUT_PULLUP\n", i);
    }
    for(int i=0; i< COLS; i++){
        pin_mode(colPins[i], OUTPUT);
        ESP32CPP::GPIO::high(colPins[i]);
//        printf("Setando coluna %d como OUTPUT\n", i);
    }



    keymap = makeKeymap(*keypadKeys);
}

//// Returns a single key only. Retained for backwards compatibility.

char Keypad::getKey() {
    int num = 1;

//
//    for(int i=0; i< ROWS; i++){
//        pin_mode(rowPins[i], INPUT);
//    }


    char key = 'k';
    while (num) {
        for (int i = 0; i < COLS; i++) {
            pin_mode(colPins[i], OUTPUT);
//            ESP32CPP::GPIO::low(colPins[i]);
            digital_write(colPins[i], LOW);
            vTaskDelay(0.1 / portTICK_PERIOD_MS);
            for (int j = 0; j < ROWS; j++) {
                int value = digital_read(rowPins[j]);
//                printf("Read column %d row %d: %d\n", i, j, value);
                if (value == 0) {
                    key = keypadKeys[j][i];
                    num = 0;
                    printf("Read column %d row %d: %d %c \n", i, j, value, key);
                    while (digital_read(rowPins[j]) == 0)
                        continue;
                    break;
                }
            }

            digital_write(colPins[i], HIGH);
            vTaskDelay(0.1 / portTICK_PERIOD_MS);
            //pin_mode(colPins[i], INPUT);

        }
    }
    return key;

}
