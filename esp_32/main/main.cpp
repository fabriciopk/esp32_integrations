#include "LiquidCrystal.h"
#include "GPIO.h"


LiquidCrystal lcd(GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4,
                    GPIO_NUM_5, GPIO_NUM_6);


extern "C" {
   void app_main();
}

void app_main() {
   // Your code goes here
}
