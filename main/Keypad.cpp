
#include "Keypad.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "GPIO.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


Keypad::Keypad(){
	begin();
}

void Keypad::pin_mode(gpio_num_t _pin, int mode){
    switch(mode){
        case OUTPUT:
            ESP32CPP::GPIO::setOutput(_pin);
            break;
        case INPUT:
            ESP32CPP::GPIO::setInput(_pin);
            break;
        case INPUT_PULLUP:
            ESP32CPP::GPIO::setInput(_pin);
            break;
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


void Keypad::begin() {

    for(int i=0; i< ROWS; i++){
        pin_mode(rowPins[i], INPUT);
    }
    for(int i=0; i< COLS; i++){
        pin_mode(colPins[i], OUTPUT);
        digital_write(colPins[i], HIGH);
    }

}

//// Returns a single key only. Retained for backwards compatibility.

char Keypad::get_key(bool *block_read) {
    int num = 1;

    char key = ' ';

    while (num && *block_read) {
        for (int i = 0; i < COLS; i++) {
            pin_mode(colPins[i], OUTPUT);
            digital_write(colPins[i], LOW);
            vTaskDelay(0.1 / portTICK_PERIOD_MS);

            for (int j = 0; j < ROWS; j++) {
                int value = digital_read(rowPins[j]);
                if (value == 0) {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    value = digital_read(rowPins[j]);
                    if (value == 0) {
                        key = keypadKeys[j][i];
                        num = 0;
                        while (digital_read(rowPins[j]) == 0)
                            continue;
                        break;
                    }
                }

            }
            digital_write(colPins[i], HIGH);
            vTaskDelay(0.1 / portTICK_PERIOD_MS);
        }
    }
    return key;
}
