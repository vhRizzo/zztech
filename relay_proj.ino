#include "driver/gpio.h"

#define QNT_RELE 2

int debounce = 100;
volatile uint8_t pin[QNT_RELE] = {32, 33};
volatile int estadoAnterior[QNT_RELE] = {1, 1};
volatile unsigned long marcaOff[QNT_RELE] = {0, 0};
volatile unsigned long marcaOn[QNT_RELE] = {0, 0};
volatile int flag[QNT_RELE] = {0, 0};

void IRAM_ATTR r0_isr (void *arg);
void IRAM_ATTR r1_isr (void *arg);
void r0_task (void *pvParameters);
void r1_task (void *pvParameters);

void setup()
{
  Serial.begin(115200);

  xTaskCreate(r0_task, "Rele0_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r1_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
}

void IRAM_ATTR r0_isr (void *arg)
{
  uint32_t gpio_estado = gpio_get_level((gpio_num_t)pin[0]);
  if (gpio_estado == estadoAnterior[0]) return;
  else if (gpio_estado == 0)
  {
    marcaOff[0] = millis();
    estadoAnterior[0] = gpio_estado;
    flag[0] = 1;
  } else {
    marcaOn[0] = millis();
    estadoAnterior[0] = gpio_estado;
    flag[0] = 2;
  }
}

void IRAM_ATTR r1_isr (void *arg)
{
  uint32_t gpio_estado = gpio_get_level((gpio_num_t)pin[1]);
  if (gpio_estado == estadoAnterior[1]) return;
  else if (gpio_estado == 0)
  {
    marcaOff[1] = millis();
    estadoAnterior[1] = gpio_estado;
    flag[1] = 1;
  } else {
    marcaOn[1] = millis();
    estadoAnterior[1] = gpio_estado;
    flag[1] = 2;
  }
}

void r0_task (void *pvParameters)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[0]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  gpio_config(&io_conf);
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)pin[0], r0_isr, NULL);
  while(1)
  {
    if(flag[0] == 1) {
      Serial.printf("R0 Desativado\n");
      flag[0] = 0;
    } else if(flag[0] == 2) {
      Serial.printf("R0 Ativado\nTempo fora de operacao: %d\n\n", marcaOn[0] - marcaOff[0]);
      flag[0] = 0;
    }
    vTaskDelay(QNT_RELE+2);
  }
}

void r1_task (void *pvParameters)
{
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[1]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  gpio_config(&io_conf);
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)pin[1], r1_isr, NULL);
  while(1)
  {
    if(flag[1] == 1) {
      Serial.printf("R1 Desativado\n");
      flag[1] = 0;
    } else if(flag[1] == 2) {
      Serial.printf("R1 Ativado\nTempo fora de operacao: %d\n\n", marcaOn[1] - marcaOff[1]);
      flag[1] = 0;
    }
    vTaskDelay(QNT_RELE+2);
  }
}

void loop() {}
