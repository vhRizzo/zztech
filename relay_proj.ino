#include <LiquidCrystal_I2C.h>
#include "driver/gpio.h"
#include "time.h"
#include <WiFi.h>

#define QNT_RELE  2

/* Wi-Fi */
#define WIFI_SSID "ZZTECH-UI2G"
#define WIFI_PASS "universo3321"

/* Fuso Horario */
#define NTP_SERVER  "pool.ntp.org"
#define GMT_OFFSET  -3*3600
#define HOR_VERAO   3600

#define TIMEOUT     3000

uint8_t pin[QNT_RELE] = {32, 33};

time_t seg;
char temp[17];
struct tm *timeinfo;
bool ini_flag[QNT_RELE] = {0, 0};
unsigned long lcd_timer = 0;
unsigned long marcaOff[QNT_RELE] = {0, 0};
unsigned long marcaOn[QNT_RELE] = {0, 0};

LiquidCrystal_I2C lcd(0x27, 16, 2);

void time_task (void *pvParameters);
void wifi_task (void *pvParameters);
void r0_task (void *pvParameters);
void r1_task (void *pvParameters);

void setup()
{
  Serial.begin(115200);

  xTaskCreate(wifi_task, "WiFi_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(lcd_task,   "LCD_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(time_task, "time_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(r0_task, "Rele0_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r1_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
}

void time_task (void *pvParameters)
{
  while(1)
  {
    configTime(GMT_OFFSET, HOR_VERAO, NTP_SERVER);
    vTaskDelay( (6*3600000)/portTICK_PERIOD_MS );
  }
}

void wifi_task (void *pvParameters)
{
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(1)
  {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Conectando ao WiFi.");
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500/portTICK_PERIOD_MS);
        Serial.print(".");
      }
      Serial.println();
      Serial.println("Conectado ao WiFi");
    }
    // vTaskDelay(300000/portTICK_PERIOD_MS);
    vTaskDelay(3000/portTICK_PERIOD_MS);
  }
}

void lcd_task (void *pvParameters)
{
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ZZTECH MONITOR");

  bool lcd_flag = 0;
  while (1)
  {
    if (millis() - lcd_timer >= TIMEOUT)
      lcd_flag = 1;
    if (lcd_flag)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ZZTECH MONITOR");
      lcd_flag = 0;
      while (millis() - lcd_timer >= TIMEOUT) {vTaskDelay(QNT_RELE+3);}
    }
    vTaskDelay(QNT_RELE+3);
  }
}

void r0_task (void *pvParameters)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[0]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[0])){
      if (ini_flag[0]) {
        marcaOff[0] = millis();
        lcd_timer = marcaOff[0];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R0 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      } else {ini_flag[0] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[0])) {vTaskDelay(QNT_RELE+3);}
    }
    else if (gpio_get_level((gpio_num_t)pin[0])){
      if(ini_flag[0]) {
        marcaOn[0] = millis();
        lcd_timer = marcaOn[0];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R0 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      } else {ini_flag[0] = 1;}
      while(gpio_get_level((gpio_num_t)pin[0])) {vTaskDelay(QNT_RELE+3);}
    }
    vTaskDelay(QNT_RELE+3);
  }
}

void r1_task (void *pvParameters)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[1]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[1])){
      if (ini_flag[1]) {
        marcaOff[1] = millis();
        lcd_timer = marcaOff[1];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      } else {ini_flag[1] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[1])) {vTaskDelay(QNT_RELE+3);}
    }
    else if (gpio_get_level((gpio_num_t)pin[1])){
      if (ini_flag[1]) {
        marcaOn[1] = millis();
        lcd_timer = marcaOn[1];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      } else {ini_flag[1] = 1;}
      while(gpio_get_level((gpio_num_t)pin[1])) {vTaskDelay(25+3);}
    }
    vTaskDelay(QNT_RELE+3);
  }
}

void loop() {}
