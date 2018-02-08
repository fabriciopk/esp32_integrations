#ifndef Buzzer_h
#define Buzzer_h

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "GPIO.h"


#define confirm_beep 0
#define cancel_beep 1
#define welcome_beep 2
#define func_beep 3
#define start_beep 4
#define end_beep 5
#define number_beep 6
#define wrong_key 7

class Buzzer {
public:
    Buzzer(gpio_num_t buzzer_pin);
    void beep(int beep);

private:
    void on();
    void off();

    gpio_num_t _buzzer_pin;

};

#endif
