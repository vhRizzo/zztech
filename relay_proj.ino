#include <LiquidCrystal_I2C.h>
#include <driver/gpio.h>
#include <HTTPClient.h>
#include <time.h>
#include <WiFi.h>

/* Quantidade de Reles que serao usados */
#define QNT_RELE    6

/* Fuso Horario */
#define NTP_SERVER  "pool.ntp.org"  // Servidor NTP para consultar a data/hora
#define GMT_OFFSET  -3 *3600        // Deslocamento de fuso horario (em segundos)
#define HOR_VERAO   3600            // Deslocamento de horario de verao (em segundos)

#define TIMEOUT     3000            // Tempo em milissegundos que o display indicara um novo evento

/* Credenciais do Wi-Fi */
char* wifi_ssid = "ZZTECH-UI2G";
char* wifi_pass = "universo3321";

/* Credenciais do servidor HTTP */
bool server_auth = 0;
char* server_url = "http://10.1.1.182:144/zziot";
char* server_usr = "";
char* server_pas = "";

String rele_nick[QNT_RELE] = {"RELE_0", "RELE_1", "RELE_2", "RELE_3", "RELE_4", "RELE_5"};

uint8_t pin[QNT_RELE] = {14, 27, 26, 25, 33, 32};       // Pinos dos Reles

time_t seg;                                             // Variavel necessaria para consultar a data/hora
struct tm *timeinfo;                                    // Struct que facilita a selecao das informacoes da data/hora
String post_str[QNT_RELE];                              // String para armazenar o JSON do post
bool ini_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};           // Indicador para evitar que o programa indique que o estado inicial do rele seja um evento
bool post_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};          // Variavel para indicar quando um post esta pendente
unsigned long lcd_timer = 0;                            // Variavel auxiliar para controlar o tempo que um evento e mostrado na tela
unsigned long marcaOff[QNT_RELE] = {0, 0, 0, 0, 0, 0};  // Variavel para armazenar o instante em que um rele e desligado
unsigned long marcaOn[QNT_RELE] = {0, 0, 0, 0, 0, 0};   // Variavel para armazenar o instanto em que um rele e ligado

LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa o display

/* Prototipos de funcoes */
void time_task (void *pvParameters);
void wifi_task (void *pvParameters);
void http_task (void *pvParameters);
void lcd_task (void *pvParameters);
void r0_task (void *pvParameters);
void r1_task (void *pvParameters);
void r2_task (void *pvParameters);
void r3_task (void *pvParameters);
void r4_task (void *pvParameters);
void r5_task (void *pvParameters);

void setup()
{
  Serial.begin(115200); // Inicializa o Serial

  /* Inicializa as tasks */
  xTaskCreate(wifi_task,                // Funcao da task
              "WiFi_Task",              // Nome da task
              1024*4,                   // Tamanho da pilha de execucao da task (geralmente 4 kbytes sao o suficiente)
              NULL,                     // Parametros da Task
              configMAX_PRIORITIES - 8, // Prioridade da Task (quanto maior o valor, maior a prioridade)
              NULL);                    // Handler da Task (para que outras tasks ou funcoes da freeRTOS possam se comunicar com esta task)
  xTaskCreate(http_task, "HTTP_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(lcd_task,   "LCD_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(time_task, "Time_Task", 1024*4, NULL, configMAX_PRIORITIES - 9, NULL);
  xTaskCreate(r0_task, "Rele0_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r1_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r2_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r3_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r4_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r5_task, "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
}

void time_task (void *pvParameters)
{
  while(1)
  {
    configTime(GMT_OFFSET, HOR_VERAO, NTP_SERVER);  // Sincroniza o relogio do ESP32 a cada 6 horas
    vTaskDelay( (6*3600000)/portTICK_PERIOD_MS );
  }
}

void wifi_task (void *pvParameters)
{
  WiFi.begin(wifi_ssid, wifi_pass);           // Inicializa o WiFi
  while(1)
  {
    if (WiFi.status() != WL_CONNECTED) {      // Verifica se o WiFi esta conectado a cada 5 minutos
      Serial.print("Conectando ao WiFi.");    // E tenta realizar a reconexao caso nao esteja
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500/portTICK_PERIOD_MS);
        Serial.print(".");
      }
      Serial.println();
      Serial.println("Conectado ao WiFi");
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}

void http_task (void *pvParameters)
{
  char temp[200];         // String auxiliar em formato char* para fazer o post
  int http_response = -1; // Para armazenar o codigo de resposta
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFiClient client;  // Cria o cliente WiFI
      HTTPClient http;    // Cria o objeto para chamar as funcoes http

      http.begin(client, server_url); // Inicializa a conexao com o servidor http
      if (server_auth)                // Realiza a autenticacao caso o servidor exija
        http.setAuthorization(server_usr, server_pas);
      
      for (int i = 0; i < QNT_RELE; i++)
      {
        if (post_flag[i])
        {
          do
          {
            strcpy(temp, post_str[i].c_str());                  // Converte o formato da string que sera feito o POST de 'String' para 'char*'
            http.addHeader("Content-Type", "application/json"); // Gera o cabecalho
            Serial.println(temp);                               // Exibe o JSON no Serial
            http_response = http.POST(temp);                    // Tenta fazer o POST
            Serial.print("HTTP Response Code: ");               // Exibe o codigo de resposta no Serial
            Serial.println(http_response);
            http.end();                                         // Encerra a conexao HTTP
            if (http_response != 200)
              vTaskDelay(5000/portTICK_PERIOD_MS);              // Espera 5 segundos antes de tentar realizar outro POST caso tenha obtido erro
          } while (http_response != 200);                       // Continua tentando fazer POST enquanto nao obtiver sucesso
          post_flag[i] = 0;
        }
      }
    }
    vTaskDelay(5);
  }
}

void lcd_task (void *pvParameters)
{
  /* Inicializa o LCD e exibe na tela a mensagem padrao */
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ZZTECH MONITOR");

  bool lcd_flag = 0;  // Flag para indicar quando a mensagem padrao deve ser exibida
  while (1)
  {
    if (millis() - lcd_timer >= TIMEOUT)
      lcd_flag = 1;   // Ativa a flag quando passar 3 segundos desde que o timer foi acionado
    if (lcd_flag)     // Se a flag estiver ativa, exibe a mensagem padrao
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ZZTECH MONITOR");
      lcd_flag = 0;   // Desativa a flag
      // E prende a execucao em um loop enquanto o timer nao  for reativado
      // Isso impede que o ESP continue enviando instrucoes para o LCD sempre que o timer estiver expirado
      while (millis() - lcd_timer >= TIMEOUT) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r0_task (void *pvParameters)
{
  int ind = 0;              // Indica o indice do rele
  /* Inicializa o pino com instrucoes proprias da espressif */
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);    // Tudo isso equivale a pinMode(pin[ind], OUTPUT)
  char temp[200];           // String auxiliar para mostrar informacoes no display
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))  // Caso a leitura do pino seja LOW
    {
      if (ini_flag[ind]) {            // Caso a flag de inicializacao ja esteja ativa
        marcaOff[ind] = millis();     // Armazena o momento em que o rele foi desativado
        lcd_timer = marcaOff[ind];    // Atribui esse tempo para o timer do LCD (isso indicara para a task do LCD que ele deve esperar o tempo de timeout antes de exibir a mensagem padrao novamente)
        time(&seg);                 // Armazena a data e hora atuais em segundos
        timeinfo = localtime(&seg); // Converte a data e hora para uma struct que premite a leitura mais dinamica das informacoes de data e hora
                                    // Armazena as informacoes de data e hora na string auxiliar
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();                // E exibe a informacao de que o rele foi desativado, bem como o horario de sua desativacao
        lcd.setCursor(0,0);
        lcd.print("R0 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}     // Caso a flag de inicializacao nao esteja ativa, ativa ela
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}  // Mantem a execucao presa em um loop enquanto a leitura do rele permanecer a mesma para evitar que esses comandos sejam executados continuamente
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))  // Realiza o mesmo procedimento para quando a leitura do pino for HIGH
    {
      if(ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R0 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r1_task (void *pvParameters)
{
  int ind = 1;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  char temp[200];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOff[ind] = millis();
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r2_task (void *pvParameters)
{
  int ind = 2;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  char temp[200];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOff[ind] = millis();
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R2 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R2 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r3_task (void *pvParameters)
{
  int ind = 3;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  char temp[200];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOff[ind] = millis();
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R3 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R3 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r4_task (void *pvParameters)
{
  int ind = 4;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  char temp[200];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOff[ind] = millis();
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R4 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R4 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void r5_task (void *pvParameters)
{
  int ind = 5;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  char temp[200];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOff[ind] = millis();
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R5 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        lcd_timer = marcaOn[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R5 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
        sprintf(temp, "{\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void loop() {}
