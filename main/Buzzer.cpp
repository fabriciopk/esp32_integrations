#include "Buzzer.h"

#include <stdio.h>
//#include <string.h>
//#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define sleep(x) vTaskDelay(x*1100/portTICK_PERIOD_MS)

Buzzer::Buzzer(gpio_num_t buzzer_pin) {
    _buzzer_pin = buzzer_pin;
    ESP32CPP::GPIO::setOutput(_buzzer_pin);
}


void Buzzer::beep(int beep) {

    switch(beep) {
        case confirm_beep:
            on();
            sleep(0.1);
            off();
            sleep(0.1);
            on();
            sleep(0.1);
            off();
            break;

        case cancel_beep:
            on();
            sleep(0.4);
            off();
            sleep(0.1);
            on();
            sleep(0.1);
            off();
            break;

        case welcome_beep:
            on();
            sleep(0.2);
            off();
            sleep(0.5);

            on();
            sleep(0.2);
            off();
            sleep(0.1);

            on();
            sleep(0.2);
            off();
            sleep(0.5);

            on();
            sleep(0.3);
            off();
            sleep(0.25);
            break;

        case func_beep:
            on();
            sleep(0.02);
            off();
            sleep(0.02);
            on();
            sleep(0.02);
            off();
            sleep(0.5);
            on();
            sleep(0.02);
            off();
            sleep(0.02);
            on();
            sleep(0.02);
            off();
            break;

        case start_beep:
            on();
            sleep(0.05);
            off();
            sleep(0.05);
            on();
            sleep(0.05);
            off();
            sleep(0.1);
            on();
            sleep(0.05);
            off();
            sleep(0.05);
            on();
            sleep(0.05);
            off();
            break;

        case end_beep:
            on();
            sleep(0.02);
            off();
            sleep(0.02);
            on();
            sleep(0.02);
            off();
            sleep(0.1);
            on();
            sleep(0.02);
            off();
            sleep(0.02);
            on();
            sleep(0.02);
            off();
            break;

        case number_beep:
            on();
            sleep(0.05);
            off();
            break;

        case wrong_key:
            on();
            sleep(0.5);
            off();
            break;

        default:
            off();
            break;

    }

}

void Buzzer::on() {
    ESP32CPP::GPIO::high(_buzzer_pin);
}
void Buzzer::off() {
    ESP32CPP::GPIO::low(_buzzer_pin);
}
