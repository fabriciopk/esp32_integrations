
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
#include "Gprs.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

extern "C" {
// #include "gprs.h"
void app_main();
}

using namespace std;

#define buzzer_pin GPIO_NUM_4

#define lcd_rs_pin GPIO_NUM_23
#define lcd_en_pin GPIO_NUM_22
#define lcd_d0_pin GPIO_NUM_19
#define lcd_d1_pin GPIO_NUM_18
#define lcd_d2_pin GPIO_NUM_17
#define lcd_d3_pin GPIO_NUM_16

//RFID_DEFAULT_MOSI_PIN = GPIO_NUM_13
//RFID_DEFAULT_MISO_PIN = GPIO_NUM_12
//RFID_DEFAULT_CLK_PIN  = GPIO_NUM_14
//RFID_DEFAULT_CS_PIN   = GPIO_NUM_15

//#define keypad_row1_pin GPIO_NUM_39
//#define keypad_row2_pin GPIO_NUM_34
//#define keypad_row3_pin GPIO_NUM_35
//#define keypad_row4_pin GPIO_NUM_32
//#define keypad_col1_pin GPIO_NUM_33
//#define keypad_col2_pin GPIO_NUM_25
//#define keypad_col3_pin GPIO_NUM_26
//#define keypad_col4_pin GPIO_NUM_27

//#define UART_GPIO_TX 10 AZUL
//#define UART_GPIO_RX 9  Verde




//Keypad keypad(makeKeymap(hexaKeys), *rowPins, *colPins, ROWS, COLS );
Keypad keypad;
GPRS gprs;
Buzzer buzzer (buzzer_pin);
LiquidCrystal lcd (lcd_rs_pin, lcd_en_pin, lcd_d0_pin, lcd_d1_pin, lcd_d2_pin, lcd_d3_pin);
MFRC522 mfrc522;  // Create MFRC522 instance

size_t len_codes = 11;
size_t len_timestamp = 8;


int current_state = 0;
int passwd = 666;
char cod_motorista[10] =  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
char cod_linha[10] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
char date_time[7] = {'0', '0', '0', '0', '0', '0', '0'};
char terminal_ID[8];


char key;
bool block_read = true;
bool timer_running = true;

void read_data_memory(){

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open
    nvs_handle nvs_handler;
    err = nvs_open("storage", NVS_READONLY, &nvs_handler);
    if (err == ESP_OK) {
        // Read
        err = nvs_get_i32(nvs_handler, "current_state", &current_state);
        err = nvs_get_i32(nvs_handler, "passwd", &passwd);
        err = nvs_get_str(nvs_handler, "cod_motorista", cod_motorista, &len_codes);
        err = nvs_get_str(nvs_handler, "cod_linha", cod_linha, &len_codes);
        err = nvs_get_str(nvs_handler, "date_time", date_time, &len_timestamp);

        // Close
        nvs_close(nvs_handler);
    }

}

void write_data_memory(){

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle nvs_handler;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handler);
    if (err == ESP_OK) {
        // Write
        err = nvs_set_i32(nvs_handler, "current_state", current_state);
        err = nvs_set_i32(nvs_handler, "passwd", passwd);
        err = nvs_set_str(nvs_handler, "cod_motorista", cod_motorista);
        err = nvs_set_str(nvs_handler, "cod_linha", cod_linha);
        err = nvs_set_str(nvs_handler, "date_time", date_time);

        err = nvs_commit(nvs_handler);
//        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(nvs_handler);
    }

}

void lcd_write(char *line1, char *line2){
    lcd.clear();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    lcd.setCursor(0,0);
    lcd.write_word(line1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    lcd.setCursor(1,0);
    lcd.write_word(line2);
}

void cancel(){
    current_state = 0;
    buzzer.beep(cancel_beep);
    write_data_memory();
}

void confirm(int beep, int state){
    buzzer.beep(beep);
    current_state = state;
    write_data_memory();
}

void return_state(int state){
    buzzer.beep(cancel_beep);
    current_state = state;
    write_data_memory();
}

void wrongkey(){
    lcd_write("Tecla", "Incorreta");
    buzzer.beep(cancel_beep);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void read_code(){

    int size = 0;
    char *message;
    char temp[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

    if(current_state == 1){
        message = "Cod. Motorista:";
        for(int i = 0; i < 10; i++) {
            temp[i] = cod_motorista[i];
            if(temp[i] != ' '){
                size++;
            }
        }
    }

    else{
        message = "Cod. da Linha:";
        for(int i = 0; i < 10; i++) {
            temp[i] = cod_linha[i];
            if (temp[i] != ' ') {
                size++;
            }
        }
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bool continue_reading = true;
    while(continue_reading){
        lcd_write(message, temp);

        key = keypad.get_key(&block_read);
        if(key == ' ')
            return;
        else if(key == '1' || key == '2' || key == '3' || key == '4' || key == '5' ||
           key == '6' || key == '7' || key == '8' || key == '9' || key == '0' ){
            if(size < 10){
                buzzer.beep(number_beep);
                temp[size] = key;
                size++;
            } else{
                lcd_write("Codigo", "Incorreto");
                buzzer.beep(wrong_key);
            }
        }
        else if(key == 'B'){
            continue_reading = false;
            block_read = false;
            cancel();
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else if(key == 'D'){
            if(size > 0){
                temp[size-1] = ' ';
                size--;
                buzzer.beep(number_beep);
            }
            else {
                return_state(current_state-1);
                continue_reading = false;
                block_read = false;
                vTaskDelay(100 / portTICK_PERIOD_MS);

            }
        }
        else if(key == 'C' and size > 0){

            for(int i = 0; i < 10; i++){
                if(current_state == 1)
                    cod_motorista[i]= temp[i];
                else
                    cod_linha[i] = temp[i];
            }
            continue_reading = false;
            block_read = false;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else
            wrongkey();
    }

}

void read_rfid(void *pvParameter){

    bool continue_reading = true;

    while(continue_reading){

        if(!block_read){
//            printf("Breaking from loop RFID\n");
            break;
        }

        if ( ! mfrc522.PICC_IsNewCardPresent())
            continue;

        if ( ! mfrc522.PICC_ReadCardSerial())
            continue;

        string UID = mfrc522.get_UID(&(mfrc522.uid));

        char *new_UID = new char[UID.size()+1];
        strcpy(new_UID, UID.c_str());

        for(int i = 0; i < 10; i++){
            if(current_state == 1)
                cod_motorista[i]= new_UID[i];
            else
                cod_linha[i] = new_UID[i];
        }
        block_read = false;
        continue_reading = false;
    }
    vTaskDelete(NULL);
}

void verify_time(void *pvParameter){
//    printf("Iniciando verify_time\n");
    int count = 0;
    while(timer_running){
        if(count > 120){
            buzzer.beep(cancel_beep);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        count++;
    }
//    printf("Encerrando verify_time\n");
    vTaskDelete(NULL);

}

void app_main() {
    mfrc522.PCD_Init();        // Init MFRC522

    lcd.begin(16, 2);
    read_data_memory();
    current_state = 0;
    write_data_memory();

//    printf("Leitura dos dasdos: Codigo motorista: %s\n", cod_motorista);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    lcd_write("Bem Vindo ao", "SysJourney");

    buzzer.beep(welcome_beep);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    while (1) {
        if (current_state == 0) {
            timer_running = true;   // Lock verify_time task
            xTaskCreate(&verify_time, "verify_time", 1024, NULL, 3, NULL);

            lcd_write("Pressione", "Inicio ou Func");
            key = keypad.get_key(&block_read);

            if (key == 'A') {

                timer_running = false;
                confirm(start_beep, current_state + 1);
                vTaskDelay(500 / portTICK_PERIOD_MS);    //Time necessary to finish verify_time task

            } else if (key == '*') {

                timer_running = false;  //Release lock from verify_time task
                lcd_write("Menu", "Funcao");
                confirm(func_beep, 0);
                vTaskDelay(500 / portTICK_PERIOD_MS);    //Time necessary to finish verify_time task

            } else {
                wrongkey();
            }
        } else if (current_state == 1) {

            timer_running = true;  // Lock verify_time task
             xTaskCreate(&verify_time, "verify_time", 1024, NULL, 3, NULL);

            lcd_write("Informe Codigo", "do Motorista");
            block_read = true;   // Lock read_rfid task
            xTaskCreate(&read_rfid, "read_rfid", 2048, NULL, 1, NULL);
            read_code();

            timer_running = false;  //Release lock from verify_time task
            block_read = true;   //Release lock from read_rfid task

            if(current_state == 1)
                confirm(confirm_beep, current_state + 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);  //Time necessary to finish verify_time task

        } else if (current_state == 2) {
            timer_running = true;  // Lock verify_time task
            xTaskCreate(&verify_time, "verify_time", 1024, NULL, 3, NULL);

            lcd_write("Informe Codigo", "da Linha");

            block_read = true;    // Lock read_rfid task
            xTaskCreate(&read_rfid, "read_rfid", 2048, NULL, 1, NULL);
            read_code();

            timer_running = false;  //Release lock from verify_time task
            if(current_state == 2)
                confirm(confirm_beep, current_state + 1);
            block_read = true;
            vTaskDelay(500 / portTICK_PERIOD_MS);  //Time necessary to finish verify_time task

        } else if (current_state == 3) {
            lcd_write("Enviando", "Dados");

            gprs.getTerminalID(terminal_ID);
            gprs.getTime(date_time);
            int n_times = 0;
            while (true){
                int response = gprs.encode_data(date_time, terminal_ID, cod_motorista, cod_linha);
                if(response!=0 && n_times < 3)
                    break;
                n_times++;
            }


            lcd_write("Jornada", "Iniciada");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            confirm(start_beep, current_state + 1);

        } else if (current_state == 4) {
            while (true) {
                lcd_write("Jornada", "em Progresso");
                key = keypad.get_key(&block_read);
                if (key == 'B') {
                    confirm(end_beep, current_state + 1);
                    break;
                } else
                    wrongkey();
            }

        } else if (current_state == 5) {
            lcd_write("Encerrando", "Jornada");

            gprs.getTerminalID(terminal_ID);
            gprs.getTime(date_time);

            while (true){
                int response = gprs.encode_data(date_time, terminal_ID, cod_motorista, cod_linha);
                if(response!=0 && n_times < 3)
                    break;
            }
            lcd_write("Jornada", "Encerrada");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            confirm(confirm_beep, 0);
        }
    }
}
