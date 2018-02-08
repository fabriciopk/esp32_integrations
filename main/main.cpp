
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>

#include "Task.h"
#include "MFRC522.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "LiquidCrystal.h"
#include "Buzzer.h"
#include "Keypad.h"

using namespace std;

#define buzzer_pin GPIO_NUM_4

#define lcd_rs_pin GPIO_NUM_22
#define lcd_en_pin GPIO_NUM_23
#define lcd_d0_pin GPIO_NUM_16
#define lcd_d1_pin GPIO_NUM_17
#define lcd_d2_pin GPIO_NUM_18
#define lcd_d3_pin GPIO_NUM_19
/* RFID_DEFAULT_MOSI_PIN = GPIO_NUM_13
 * RFID_DEFAULT_MISO_PIN = GPIO_NUM_12
 * RFID_DEFAULT_CLK_PIN  = GPIO_NUM_14
 * RFID_DEFAULT_CS_PIN   = GPIO_NUM_15
*/
//
//#define keypad_row1_pin GPIO_NUM_39
//#define keypad_row2_pin GPIO_NUM_34
//#define keypad_row3_pin GPIO_NUM_35
//#define keypad_row4_pin GPIO_NUM_32

//#define keypad_col1_pin GPIO_NUM_33
//#define keypad_col2_pin GPIO_NUM_25
//#define keypad_col3_pin GPIO_NUM_26
//#define keypad_col4_pin GPIO_NUM_27




//Keypad keypad(makeKeymap(hexaKeys), *rowPins, *colPins, ROWS, COLS );
Keypad keypad;

Buzzer buzzer (buzzer_pin);
LiquidCrystal lcd (lcd_rs_pin, lcd_en_pin, lcd_d0_pin, lcd_d1_pin, lcd_d2_pin, lcd_d3_pin);
//
MFRC522 mfrc522;  // Create MFRC522 instance


void lcd_write(char *line1, char *line2){
    lcd.clear();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    lcd.setCursor(0,0);
    lcd.write_word(line1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    lcd.setCursor(1,0);
    lcd.write_word(line2);
}
//
char* read_rfid(){


    while(1) {
        // Look for new cards
        if ( ! mfrc522.PICC_IsNewCardPresent()) {
            continue;
        }

        // Select one of the cards
        if ( ! mfrc522.PICC_ReadCardSerial()) {
            continue;
        }

        string UID = mfrc522.get_UID(&(mfrc522.uid));

        char *new_UID = new char[UID.size()+1];
        strcpy(new_UID, UID.c_str());

       buzzer.beep(confirm_beep);

        return new_UID;
    }

}


void testRFID() {


    lcd_write("Leitor de", "RFID");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while(1) {

        lcd_write("Leitor de", "RFID");
        char *new_UID = read_rfid();

        lcd_write("RFID Lido", new_UID);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
        lcd.clear();
//        cout << "UID: "<< new_UID << endl;

    }
}


extern "C" {
void app_main();
}
//
//class MyTask: public Task {
//    void run(void* data) {
//        int count = 0;
//        testRFID();
//        printf("Done\n");
//    }
//};


int i = 0;

void app_main() {
    mfrc522.PCD_Init();		// Init MFRC522

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    lcd_write("Welcome", "Beep");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    buzzer.beep(welcome_beep);
    vTaskDelay(1000 / portTICK_PERIOD_MS);


//    ESP32CPP::GPIO::setOutput(GPIO_NUM_33);
//    ESP32CPP::GPIO::setInput(GPIO_NUM_39);
//
//    while(1){
//
//        ESP32CPP::GPIO::high(GPIO_NUM_33);
//        int value = ESP32CPP::GPIO::read(GPIO_NUM_39);
//        printf("Pin 39 Value: %d\n", value);
//
//        vTaskDelay(2000 / portTICK_PERIOD_MS);
//
//        ESP32CPP::GPIO::low(GPIO_NUM_33);
//        int value2 = ESP32CPP::GPIO::read(GPIO_NUM_39);
//        printf("Pin 39 Value2(LOW): %d\n", value2);
//
//        vTaskDelay(2000 / portTICK_PERIOD_MS);
//
//    }


//
//    char key;
//    while (1){
//        key = keypad.getKey();
//        printf("Read Key: %c\n", key);
//    }

//    lcd_write("Read Key", *key);

    printf("INIT %d\n", i++);
    testRFID();

    while(1) {
        printf("%d\n", i++);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
//    MyTask* pMyTask = new MyTask();
//    pMyTask->setStackSize(20000);
//    pMyTask->start();






    //   fflush(stdout);
    //    esp_restart();
}