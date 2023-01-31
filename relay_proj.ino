#include <LiquidCrystal_I2C.h>
#include <ESPAsyncWebSrv.h>
#include <driver/gpio.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <time.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

/* Quantidade maxima de Reles */
#define QNT_RELE    6

/* Fuso Horario */
#define NTP_SERVER  "pool.ntp.org"  // Servidor NTP para consultar a data/hora
#define GMT_OFFSET  -3 *3600        // Deslocamento de fuso horario (em segundos)
#define HOR_VERAO   3600            // Deslocamento de horario de verao (em segundos)

#define TIMEOUT     3000            // Tempo em milissegundos que o display indicara um novo evento

#define AP_PASS     "zztech@1020"

#define CONF_DIR    "/config.ini"
#define POST_DIR    "/post.txt"

#define CONF_PIN    13

/* Parametros para o HTTP GET */
String param_nome_maq   = "nome_maq";
String param_wifi_ssid  = "wifi_ssid";
String param_wifi_pass  = "wifi_pass";
String param_server_url = "server_url";
String param_server_usr = "server_usr";
String param_server_pas = "server_pas";
String param_rele_nick[QNT_RELE] = {"rele_nick_0","rele_nick_1","rele_nick_2","rele_nick_3","rele_nick_4","rele_nick_5"};

/* Variaveis para salvar as configuracoes */
String nome_maq;
String wifi_ssid;
String wifi_pass;
String server_url;
String server_usr;
String server_pas;
String rele_nick[QNT_RELE];                             // Apelido para os reles que serao definidos pelo usuario

bool serial_debug = 1;                                  // Para debug do dispositivo

uint8_t pin[QNT_RELE] = {14, 27, 26, 25, 33, 32};       // Pinos dos Reles

File post;                                              // Variavel para o arquivo de dados
File conf;                                              // Variavel para o arquivo de configuracao
time_t seg;                                             // Variavel necessaria para consultar a data/hora
bool def_msg = 1;
bool server_auth;
bool suspended = 0;
struct tm *timeinfo;                                    // Struct que facilita a selecao das informacoes da data/hora
String post_str[QNT_RELE];                              // String para armazenar o JSON do post
String DEFAULT_AP = "ZZTECH_MONITOR";
bool ini_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};           // Indicador para evitar que o programa indique que o estado inicial do rele seja um evento
bool post_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};          // Variavel para indicar quando um post esta pendente
unsigned long lcd_timer = 0;                            // Variavel auxiliar para controlar o tempo que um evento e mostrado na tela
unsigned long marcaOff[QNT_RELE] = {0, 0, 0, 0, 0, 0};  // Variavel para armazenar o instante em que um rele e desligado
unsigned long marcaOn[QNT_RELE] = {0, 0, 0, 0, 0, 0};   // Variavel para armazenar o instanto em que um rele e ligado

LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa o display
AsyncWebServer server(80);          // Inicializa o server interno

/* Pagina html de configuracao */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
    <html lang="en">
        <head>
            <meta charset="UTF-8" />
            <meta http-equiv="X-UA-Compatible" content="IE=edge" />
            <meta name="viewport" content="width=device-width, initial-scale=1.0" />
            <link rel="stylesheet" href="data:,">
            <style>
                body {
                    font-family: Arial, Helvetica, sans-serif;
                }
                form{
                    display: flex;
                    flex-direction: column;
                    align-items: center;
                }
                .container{
                    display: flex;
                    flex-direction: column;
                    width: 8cm;
                }
                .sub_container{
                    position: relative;
                    left: 0.5cm;
                    display: flex;
                    flex-direction: column;
                    width: 7.5cm;
                    margin-top: 1em;
                }
                h1, p{
                    text-align: center;
                }
                input {
                    margin: 0.25em 0em 1em 0em;
                    outline: none;
                    padding: 0.5em;
                    border: none;
                    background-color: rgb(225, 225, 225);
                    border-radius: 0.25em;
                    color: black;
                }
                button {
                    padding: 0.75em;
                    border: none;
                    outline: none;
                    background-color: rgb(232, 100, 18);
                    color: white;
                    border-radius: 0.25em;
                }
                button:hover {
                    cursor: pointer;
                    background-color: rgb(232, 75, 4);
                }
            </style>
            <title>ZZTech Monitor</title>
        </head>
        <body>
            <form action="/get">
                <div class="container">
                    <h1>ZZTech Monitor</h1>
                    <p>Configurações da máquina.</p>
                    <label for="nome_maq"><b>Nome da máquina</b></label>
                    <input
                        type="text"
                        placeholder="Digite o nome da máquina (máximo 16 caracteres)."
                        name="nome_maq"
                        id="nome_maq"
                        maxlength="16"
                        required
                    />
                    <label for="wifi_ssid"><b>SSID do WiFi</b></label>
                    <input
                        type="text"
                        placeholder="Digite o SSID."
                        name="wifi_ssid"
                        id="wifi_ssid"
                        required
                    />
                    <label for="wifi_pass"><b>Senha do WiFi</b></label>
                    <input
                        type="text"
                        placeholder="Digite a senha."
                        name="wifi_pass"
                        id="wifi_pass"
                        required
                    />
                    <label for="server_url"><b>URL do servidor</b></label>
                    <input
                        type="text"
                        placeholder="Digite a URL."
                        name="server_url"
                        id="server_url"
                        required
                    />
                    <label for="server_usr"><b>Nome de usuário do servidor</b></label>
                    <input
                        type="text"
                        placeholder="Digite o usuário."
                        name="server_usr"
                        id="server_usr"
                    />
                    <label for="server_pas"><b>Senha do servidor</b></label>
                    <input
                        type="text"
                        placeholder="Digite a senha."
                        name="server_pas"
                        id="server_pas"
                    />
                    <label for="rele_nick"><b>Apelido para os dispositivos</b></label>
                    <div class="sub_container">
                        <label for="rele_nick_0"><b>Dispositivo 1</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 1."
                            name="rele_nick_0"
                            id="rele_nick_0"
                            required
                        />
                        <label for="rele_nick_1"><b>Dispositivo 2</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 2."
                            name="rele_nick_1"
                            id="rele_nick_1"
                            required
                        />
                        <label for="rele_nick_2"><b>Dispositivo 3</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 3."
                            name="rele_nick_2"
                            id="rele_nick_2"
                            required
                        />
                        <label for="rele_nick_3"><b>Dispositivo 4</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 4."
                            name="rele_nick_3"
                            id="rele_nick_3"
                            required
                        />
                        <label for="rele_nick_4"><b>Dispositivo 5</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 5."
                            name="rele_nick_4"
                            id="rele_nick_4"
                            required
                        />
                        <label for="rele_nick_5"><b>Dispositivo 6</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 6."
                            name="rele_nick_5"
                            id="rele_nick_5"
                            required
                        />
                    </div>
                    <button type="submit">Confirmar</button>
                </div>
            </form>     
        </body>
    </html>)rawliteral";

/* Prototipos de funcoes */
void notFound(AsyncWebServerRequest *request);
void conf_task (void *pvParameters);
void time_task (void *pvParameters);
void wifi_task (void *pvParameters);  TaskHandle_t wifi_handle;
void http_task (void *pvParameters);  TaskHandle_t http_handle;
void lcd_task (void *pvParameters);   TaskHandle_t lcd_handle;
void r0_task (void *pvParameters);
void r1_task (void *pvParameters);
void r2_task (void *pvParameters);
void r3_task (void *pvParameters);
void r4_task (void *pvParameters);
void r5_task (void *pvParameters);
void set_sdcard ();
void get_conf();

void setup()
{
  Serial.begin(115200); // Inicializa o Serial

  pinMode(CONF_PIN, INPUT_PULLUP);
  if(!digitalRead(CONF_PIN)) {
    Serial.println("Modo de configuracao ativado.");
    Serial.println("Por favor, desative o modo de configuracao ao iniciar a maquina.");
  }
  while(!digitalRead(CONF_PIN)) {delay(500);}
  
  /* Inicializa o LCD */
  lcd.init();
  lcd.backlight();
  lcd.clear();

  /* Inicializa o Cartao SD */
  set_sdcard();
  get_conf();

  /* Inicializa as tasks */
  xTaskCreate(wifi_task,                // Funcao da task
              "WiFi_Task",              // Nome da task
              1024*4,                   // Tamanho da pilha de execucao da task (geralmente 4 kbytes sao o suficiente)
              NULL,                     // Parametros da Task
              configMAX_PRIORITIES - 8, // Prioridade da Task (quanto maior o valor, maior a prioridade)
              &wifi_handle);            // Handler da Task (para que outras tasks ou funcoes da freeRTOS possam se comunicar com esta task)
  xTaskCreate(http_task,  "HTTP_Task",  1024*4, NULL, configMAX_PRIORITIES -  9, &http_handle);
  xTaskCreate(lcd_task,   "LCD_Task",   1024*4, NULL, configMAX_PRIORITIES -  9, &lcd_handle);
  xTaskCreate(time_task,  "Time_Task",  1024*4, NULL, configMAX_PRIORITIES -  9, NULL);
  xTaskCreate(r0_task,    "Rele0_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r1_task,    "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r2_task,    "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r3_task,    "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r4_task,    "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(r5_task,    "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(conf_task,  "AP_Task",    1024*4, NULL, configMAX_PRIORITIES -  7, NULL);
}

void conf_task (void *pvParameters)
{
  while (1)
  {
    if (!digitalRead(CONF_PIN)) // Caso o switch de configuracao for acionado
    {
      vTaskSuspend(wifi_handle);
      vTaskSuspend(http_handle);
      vTaskSuspend(lcd_handle);
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      suspended = 1;            // Ativa a variavel de suspensao
      if (serial_debug)
        Serial.printf("\nIniciando ponto de acesso de configuracao...\n\n");
      if (nome_maq.isEmpty())
        nome_maq = DEFAULT_AP;
      WiFi.softAP(nome_maq.c_str(), AP_PASS); // Inicia o ponto de acesso WiFi
      if (serial_debug) {
        Serial.printf("Ponto de acesso WiFi gerado.\nAcesse com as credenciais abaixo para configurar a maquina\n");
        Serial.printf("SSID: %s\nSenha: %s\n\n", nome_maq.c_str(), AP_PASS);
      }
      IPAddress IP = WiFi.softAPIP(); // Gera o endereco IP do ESP32
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
      });
      if (serial_debug) {
        Serial.print("Apos conectar ao ponto de acesso, abra seu navegador e acesse o seguinte endereco: ");
        Serial.println(IP); Serial.println();
      }

      server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        nome_maq = request->getParam(param_nome_maq)->value();
        wifi_ssid = request->getParam(param_wifi_ssid)->value();
        wifi_pass = request->getParam(param_wifi_pass)->value();
        server_url = request->getParam(param_server_url)->value();
        if (request->hasParam(param_server_usr) && request->hasParam(param_server_pas))
          server_auth = 1;
        if (server_auth)
        {
          server_usr = request->getParam(param_server_usr)->value();
          server_pas = request->getParam(param_server_pas)->value();
        } else {
          server_usr = "Servidor sem autenticacao";
          server_pas = "Servidor sem autenticacao";
        }
        for (int i = 0; i < QNT_RELE; i++)
          rele_nick[i] = request->getParam(param_rele_nick[i])->value();
        request->send(200, "text/html", "Máquina configurada. Desative o modo de configuração ou"
                                        "<br><a href=\"/\">Retorne para as configurações.</a>");
      });
      server.onNotFound(notFound);
      server.begin();

      while(!digitalRead(CONF_PIN)) {vTaskDelay(5);}
    } else if (digitalRead(CONF_PIN))
    {
      if (suspended)
      {
        server.end();
        WiFi.softAPdisconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        conf.close();
        conf = SD.open(CONF_DIR, FILE_WRITE);
        conf.print("NOME_MAQ=");    conf.println(nome_maq);
        conf.print("WIFI_SSID=");   conf.println(wifi_ssid);
        conf.print("WIFI_PASS=");   conf.println(wifi_pass);
        conf.print("SERVER_URL=");  conf.println(server_url);
        conf.print("SERVER_USR=");  conf.println(server_usr);
        conf.print("SERVER_PAS=");  conf.println(server_pas);
        conf.print("RELE_NICK=");   conf.print(rele_nick[0]);
        conf.print(",");            conf.print(rele_nick[1]);
        conf.print(",");            conf.print(rele_nick[2]);
        conf.print(",");            conf.print(rele_nick[3]);
        conf.print(",");            conf.print(rele_nick[4]);
        conf.print(",");            conf.println(rele_nick[5]);
        conf.close();
        conf = SD.open(CONF_DIR);
        get_conf();
        vTaskResume(wifi_handle);
        vTaskResume(http_handle);
        vTaskResume(lcd_handle);
        suspended = 0;
      }
      while(digitalRead(CONF_PIN)) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  while(1)
  {
    if (WiFi.status() != WL_CONNECTED) {
      if (serial_debug) {
        Serial.print("Conectando ao WiFi");  // E tenta realizar a reconexao caso nao esteja
      }
      while (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        vTaskDelay(1000/portTICK_PERIOD_MS);
        if (serial_debug && i < 3) {
          Serial.print(".");
        }
      }
      if (serial_debug) {
        Serial.println();
        Serial.println("WiFi Conectado");
      }
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}

void http_task (void *pvParameters)
{
  String temp;         // String auxiliar em formato char* para fazer o post
  int array_set = 1;
  int http_response = -1; // Para armazenar o codigo de resposta
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFiClient client;  // Cria o cliente WiFi
      HTTPClient http;    // Cria o objeto para chamar as funcoes http

      http.begin(client, server_url.c_str()); // Inicializa a conexao com o servidor http
      if (server_auth)                // Realiza a autenticacao caso o servidor exija
        http.setAuthorization(server_usr.c_str(), server_pas.c_str());
      
      for (int i = 0; i < QNT_RELE; i++)
      {
        if (post_flag[i])
        {
          if (array_set)
          {
            temp = "[" + post_str[i];
            array_set = 0;
          } else temp = "," + post_str[i];
          post_flag[i] = 0;
        }
      }
      temp = temp + "]";
      array_set = 1;
      if(temp[0] == '[')
      {
        do
        {
          // http.addHeader("Content-Type", "application/json"); // Gera o cabecalho
          http_response = http.POST(temp.c_str());            // Tenta fazer o POST
          if (serial_debug) {
            Serial.println(temp);                             // Exibe o JSON no Serial
            Serial.print("HTTP Response Code: ");             // Exibe o codigo de resposta no Serial
            Serial.println(http_response);
          }
          http.end();                                         // Encerra a conexao HTTP
          if (http_response != 200)
            vTaskDelay(5000/portTICK_PERIOD_MS);              // Espera 5 segundos antes de tentar realizar outro POST caso tenha obtido erro
        } while (http_response != 200);                       // Continua tentando fazer POST enquanto nao obtiver sucesso
        temp = "";
      }
    }
    vTaskDelay(5);
  }
}

void lcd_task (void *pvParameters)
{
  /* Exibe a mensagem padrao no LCD */
  lcd.setCursor(0, 0);
  lcd.print("ZZTECH MONITOR  ");
  lcd.setCursor(0, 1);
  if (!def_msg)
    lcd.print("NAO CONFIGURADO ");
  else
    lcd.print("                ");

  bool lcd_flag = 0;  // Flag para indicar quando a mensagem padrao deve ser exibida
  while (1)
  {
    if (millis() - lcd_timer >= TIMEOUT)
      lcd_flag = 1;   // Ativa a flag quando passar 3 segundos desde que o timer foi acionado
    if (lcd_flag)     // Se a flag estiver ativa, exibe a mensagem padrao
    {
      lcd.setCursor(0, 0);
      lcd.print("ZZTECH MONITOR  ");
      lcd.setCursor(0, 1);
      if (!def_msg)
        lcd.print("NAO CONFIGURADO ");
      else
        lcd.print("                ");
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
        time(&seg);                 // Armazena a data e hora atuais em segundos
        timeinfo = localtime(&seg); // Converte a data e hora para uma struct que premite a leitura mais dinamica das informacoes de data e hora
                                    // Armazena as informacoes de data e hora na string auxiliar
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOff[ind];    // Atribui esse tempo para o timer do LCD (isso indicara para a task do LCD que ele deve esperar o tempo de timeout antes de exibir a mensagem padrao novamente)
          lcd.clear();                // E exibe a informacao de que o rele foi desativado, bem como o horario de sua desativacao
          lcd.setCursor(0,0);
          lcd.print("R0 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}     // Caso a flag de inicializacao nao esteja ativa, ativa ela
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}  // Mantem a execucao presa em um loop enquanto a leitura do rele permanecer a mesma para evitar que esses comandos sejam executados continuamente
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))  // Realiza o mesmo procedimento para quando a leitura do pino for HIGH
    {
      if(ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R0 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
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
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          marcaOff[ind] = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R1 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R1 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
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
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          marcaOff[ind] = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R2 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R2 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
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
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          marcaOff[ind] = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R3 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R3 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
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
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          marcaOff[ind] = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R4 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R4 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
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
        lcd_timer = marcaOff[ind];
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          marcaOff[ind] = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R5 Desativado");
          lcd.setCursor(0,1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Desativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      if (ini_flag[ind]) {
        marcaOn[ind] = millis();
        time(&seg);
        timeinfo = localtime(&seg);
        sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
        if (!suspended) {
          lcd_timer = marcaOn[ind];
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("R5 Reativado");
          lcd.setCursor(0, 1);
          lcd.print(temp);
        }
        sprintf(temp, "{\"maquina\":\"%s\",\"dispositivo\":\"%s\",\"evento\":\"Ativado\",\"horario\":\"%02d/%02d/%04d-%02d:%02d:%02d\"}", nome_maq, rele_nick[ind], timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        post_str[ind] = temp;
        post_flag[ind] = 1;
      } else {ini_flag[ind] = 1;}
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void set_sdcard ()
{
  if (!SD.begin(5)) {
    if (serial_debug)
      Serial.println("Falha no Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("FALHA NO SD CARD");
    sd_error();
  }

  uint8_t tipoSD = SD.cardType();
  if (tipoSD == CARD_NONE) {
    if (serial_debug)
      Serial.println("Sem Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("SEM SD CARD");
    sd_error();
  }

  conf = SD.open(CONF_DIR);
  if (!conf) {
    conf.close();
    conf = SD.open(CONF_DIR, FILE_WRITE);
    conf.print("NOME_MAQ=\n"
               "WIFI_SSID=\n"
               "WIFI_PASS=\n"
               "SERVER_URL=\n"
               "SERVER_USR=\n"
               "SERVER_PAS=\n"
               "RELE_NICK=\n");
    conf.close();
    conf = SD.open(CONF_DIR);
  }
  post = SD.open(POST_DIR);
  if (!post) {
    post.close();
    post = SD.open(POST_DIR, FILE_WRITE);
    post.close();
    post = SD.open(POST_DIR);
  }

  if (serial_debug) Serial.println("Cartao SD OK.");
}

void get_conf ()
{
  bool missing_conf = 0;
  conf = SD.open(CONF_DIR);

  if (!conf)
  {
    if (serial_debug)
      Serial.println("Falha no Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("FALHA NO SD CARD");
    sd_error();
  }

  String temp;

  /* Le o nome da maquina */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    missing_conf = 1;
  else
  {
    nome_maq = temp;
  }

  /* Le o SSID do WiFi */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    missing_conf = 1;
  else
  {
    wifi_ssid = temp;
  }

  /* Le a senha do WiFi */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    missing_conf = 1;
  else
  {
    wifi_pass = temp;
  }

  /* Le o URL do server */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    missing_conf = 1;
  else 
  {
    server_url = temp;
  }

  /* Le o usuario do server */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    server_auth = 0;
  else
  {
    server_usr = temp;
  }

  /* Le a senha do server */
  conf.readStringUntil('=');
  temp = conf.readStringUntil('\n');
  temp.remove(strlen(temp.c_str())-1, 1);
  if (temp.isEmpty())
    server_auth = 0;
  else
  {
    server_pas = temp;
  }

  /* Le os apelidos dos reles */
  conf.readStringUntil('=');
  for (int i = 0; i < QNT_RELE; i++)
  {
    if (i < QNT_RELE-1) {
      temp = conf.readStringUntil(',');
      if (temp.isEmpty()) {
        missing_conf = 1;
        break;
      }
    } else {
      temp = conf.readStringUntil('\n');
      temp.remove(strlen(temp.c_str())-1, 1);
      if (temp.isEmpty()) {
        missing_conf = 1;
        break;
      }
    }
    rele_nick[i] = temp;
  }

  if (missing_conf) {
    def_msg = 0;
    if (serial_debug) {
      Serial.println("Existem configuracoes necessarias faltantes.");
      Serial.println("Acione o switch de configuracao da maquina e acesse o ponto de acesso WiFi gerado para configurar.");
    }
  } else {
    def_msg = 1;
    if (serial_debug) {
      Serial.printf("\nSetup OK\nConfiguracoes atuais:\n\n");
      Serial.printf("Nome da maquina = %s\n", nome_maq.c_str());
      Serial.printf("WiFi SSID = %s\n", wifi_ssid.c_str());
      Serial.printf("WiFi Password = %s\n", wifi_pass.c_str());
      Serial.printf("Servidor URL = %s\n", server_url.c_str());
      Serial.printf("Servidor Usuario = %s\n", server_usr.c_str());
      Serial.printf("Servidor Senha = %s\nApelido dos reles = ", server_pas.c_str());
      for (int i = 0; i < QNT_RELE-1; i++)
        Serial.printf("%s, ", rele_nick[i].c_str());
      Serial.printf("%s\n\n", rele_nick[QNT_RELE-1].c_str());
    }
  }
}

void sd_error ()
{
  char temp[17];
  for (int i = 9; i >= 0; i--)
  {
    sprintf(temp, "REINICIANDO EM %i", i);
    lcd.setCursor(0, 1);
    lcd.print(temp);
    if (serial_debug)
      Serial.printf("Reiniciando em %i...\n", i);
    delay(1000);
  }
  ESP.restart();
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void loop() {}
