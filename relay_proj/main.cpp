#include <LiquidCrystal_I2C.h>
#include <ESPAsyncWebServer.h>
#include <driver/gpio.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <Arduino.h>
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

#define AP_PASS     "zztech@1020"   // Senha de acesso as configuracoes

#define CONF_DIR    "/config.ini"   // Diretorio do arquivo de configuracao
#define POST_DIR    "/post.txt"     // Diretorio do arquivo de post

#define CONF_PIN    13              // Pino do switch de configuracao

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
String rele_nick[QNT_RELE];

bool serial_debug = 1;                                  // Para debug do dispositivo

uint8_t pin[QNT_RELE] = {14, 27, 26, 25, 33, 32};       // Pinos dos Reles

File post;                                              // Variavel para o arquivo de dados
File conf;                                              // Variavel para o arquivo de configuracao
time_t seg;                                             // Variavel necessaria para consultar a data/hora
String str0;
String str1;
uint8_t ctrl;
bool def_msg = 1;                                       // Controle de mensagem padrao para ser exibida no display
bool server_auth;                                       // Controle de autenticacao de servidor
bool suspended = 0;                                     // Controle para quando estiver configurando a maquina
struct tm *timeinfo;                                    // Struct que facilita a selecao das informacoes da data/hora
volatile bool scroll = 0;
String DEFAULT_AP = "ZZTECH_MONITOR";                   // Nome padrao da maquina
bool ini_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};           // Indicador para evitar que o programa indique que o estado inicial do rele seja um evento
bool used_rele[QNT_RELE] = {1, 1, 1, 1, 1, 1};          // Controle de quais reles serao usados
unsigned long lcd_timer = 0;                            // Variavel auxiliar para controlar o tempo que um evento e mostrado na tela
unsigned long marcaOff[QNT_RELE] = {0, 0, 0, 0, 0, 0};  // Variavel para armazenar o instante em que um rele e desligado
unsigned long marcaOn[QNT_RELE] = {0, 0, 0, 0, 0, 0};   // Variavel para armazenar o instanto em que um rele e ligado

LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa o display
AsyncWebServer server(80);          // Inicializa o server interno

/* Pagina html de configuracao */
#pragma region
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
                        />
                        <label for="rele_nick_1"><b>Dispositivo 2</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 2."
                            name="rele_nick_1"
                            id="rele_nick_1"
                        />
                        <label for="rele_nick_2"><b>Dispositivo 3</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 3."
                            name="rele_nick_2"
                            id="rele_nick_2"
                        />
                        <label for="rele_nick_3"><b>Dispositivo 4</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 4."
                            name="rele_nick_3"
                            id="rele_nick_3"
                        />
                        <label for="rele_nick_4"><b>Dispositivo 5</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 5."
                            name="rele_nick_4"
                            id="rele_nick_4"
                        />
                        <label for="rele_nick_5"><b>Dispositivo 6</b></label>
                        <input
                            type="text"
                            placeholder="Digite o nome do dispositivo 6."
                            name="rele_nick_5"
                            id="rele_nick_5"
                        />
                    </div>
                    <button type="submit">Confirmar</button>
                </div>
            </form>     
        </body>
    </html>)rawliteral";
#pragma endregion

/* Pagina html para indicar que as configuracoes foram salvas */
#pragma region 
const char confok_html[] PROGMEM = R"rawliteral(
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
                    display: flex;
                    flex-direction: column;
                    align-items: center;
                }
                .container{
                    display: flex;
                    flex-direction: column;
                    width: 8cm;
                }
                h1{
                    text-align: center;
                }
                #fst{
                    text-align: center;
                    margin-bottom: 1cm;
                }
                #other{
                    text-align: left;
                }
            </style>
            <title>ZZTech Monitor</title>
        </head>
        <body>
            <div class="container">
                <h1>ZZTech Monitor</h1>
                <p id="fst">Configurações salvas.</p>
                <p id="other">
                    Você pode fechar esta página e sair do modo de configuração, ou
                    <a href="/">Retornar para a página anterior</a>
                    para alterar as configurações.
                </p>
            </div> 
        </body>
    </html>)rawliteral";
#pragma endregion

/* Prototipos de funcoes */
void notFound(AsyncWebServerRequest *request);
void scroll_msg (void *pvParameters);
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
void sd_error ();
void get_conf();

void setup()
{
  Serial.begin(115200);             // Inicializa o Serial

  /* Inicializa o LCD */
  lcd.init();
  lcd.backlight();
  lcd.clear();

  pinMode(CONF_PIN, INPUT_PULLUP);  // Inicializa o pino de configuracao
  if(!digitalRead(CONF_PIN)) {      // Caso este pino ja esteja acionado ao ligar a maquina
    if (serial_debug) {             // Avisa para desligar
      Serial.printf("\nModo de configuracao ativado.\n");
      Serial.printf("\nPor favor, desative o modo de configuracao durante a inicializacao.\n");
    }
    lcd.setCursor(0, 0);
    lcd.print("MODO CONF LIGADO");
    str1 = "DESLIGUE O MODO CONF DURANTE A INICIALIZACAO";
    ctrl = 1;
    scroll = 1;
    xTaskCreate(scroll_msg, "Scroll_LCD", 1024*4, NULL, configMAX_PRIORITIES - 15, NULL);
    while(!digitalRead(CONF_PIN)) {if (digitalRead(CONF_PIN)) scroll = 0;} // E prende o programa em um loop ate que o switch seja desligado
  }

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
  if(used_rele[0])  // Inicializa apenas as tasks com reles ativos
    xTaskCreate(r0_task,  "Rele0_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  if(used_rele[1])
    xTaskCreate(r1_task,  "Rele1_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  if(used_rele[2])
    xTaskCreate(r2_task,  "Rele2_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  if(used_rele[3])
    xTaskCreate(r3_task,  "Rele3_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  if(used_rele[4])
    xTaskCreate(r4_task,  "Rele4_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  if(used_rele[5])
    xTaskCreate(r5_task,  "Rele5_Task", 1024*4, NULL, configMAX_PRIORITIES - 10, NULL);
  xTaskCreate(conf_task,  "AP_Task",    1024*4, NULL, configMAX_PRIORITIES -  7, NULL);
}

void conf_task (void *pvParameters)
{
  char aux[17];
  while (1)
  {
    if (!digitalRead(CONF_PIN)) // Caso o botao de configuracao seja acionado
    {
      vTaskSuspend(wifi_handle);// Suspende as tasks de wifi, http, e  lcd
      vTaskSuspend(http_handle);
      vTaskSuspend(lcd_handle);
      WiFi.disconnect();        // Desconecta o wifi
      WiFi.mode(WIFI_AP);       // Altera o modo de wifi para Access Point
      suspended = 1;            // Ativa a variavel de suspensao
      if (serial_debug)
        Serial.printf("\nIniciando ponto de acesso de configuracao...\n\n");
      if (nome_maq.isEmpty())   // Verifica se o campo de nome da maquina esta vazio
        nome_maq = DEFAULT_AP;  // Caso esteja, define ele como o nome padrao
      WiFi.softAP(nome_maq.c_str(), AP_PASS); // Inicia o ponto de acesso WiFi
      ctrl = 1;
      lcd.setCursor(0, 0);
      lcd.print("MODO CONF LIGADO");
      str1 = "ACESSE O WIFI GERADO PELA MAQUINA";
      scroll = 1;
      xTaskCreate(scroll_msg, "Scroll_LCD", 1024*4, NULL, configMAX_PRIORITIES - 15, NULL);
      vTaskDelay(10000/portTICK_PERIOD_MS);
      scroll = 0;
      vTaskDelay(1000/portTICK_PERIOD_MS);
      if (serial_debug) {
        Serial.printf("Ponto de acesso WiFi gerado.\nAcesse com as credenciais abaixo para configurar a maquina\n");
        Serial.printf("SSID: %s\nSenha: %s\n\n", nome_maq.c_str(), AP_PASS);
      }
      sprintf(aux, "%16s", nome_maq.c_str());
      lcd.setCursor(0, 0);
      lcd.print(aux);
      lcd.setCursor(0, 1);
      sprintf(aux, "%16s", AP_PASS);
      lcd.print(aux);
      IPAddress IP = WiFi.softAPIP(); // Gera o endereco IP do ESP32

      /* Gera a pagina html de configuracao */
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
      });
      if (serial_debug) {
        Serial.print("Apos conectar ao ponto de acesso, abra seu navegador e acesse o seguinte endereco: ");
        Serial.println(IP); Serial.println();
      }
      vTaskDelay(30000/portTICK_PERIOD_MS);
      ctrl = 0;
      str0 = "ABRA O SEGUINTE ENDERECO NO SEU NAVEGADOR";
      scroll = 1;
      xTaskCreate(scroll_msg, "Scroll_LCD", 1024*4, NULL, configMAX_PRIORITIES - 15, NULL);
      lcd.setCursor(0, 1);
      sprintf(aux, "%16s", IP.toString().c_str());
      lcd.print(aux);

      /* Recebe os parametros passados pelo usuario e os salva em suas respectivas variaveis */
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
        for (int i = 0; i < QNT_RELE; i++) {
          if (request->hasParam(param_rele_nick[i])) {
            rele_nick[i] = request->getParam(param_rele_nick[i])->value();
            if (rele_nick[i].isEmpty())
              rele_nick[i] = "UNUSED";
          }
        }

        /* Ao final, exibe uma mensagem ao usuario indicando que as configuracoes foram salvas */
        request->send(200, "text/html", confok_html);
      });
      server.onNotFound(notFound);  // Avisa caso o request retorne erro 404
      server.begin();

      while(!digitalRead(CONF_PIN)) {vTaskDelay(5);}  // Mantem a task presa em um loop pois o codigo acima deve ser executado somente uma vez
    } else if (digitalRead(CONF_PIN))   // Caso o switch seja desligado
    {
      if (suspended)                    // E o programa ja tenha sido suspenso
      {
        server.end();                   // Finaliza o servidor
        WiFi.softAPdisconnect();        // Desativa o Ponto de Acesso
        WiFi.mode(WIFI_STA);            // Retorna o wifi do ESP para modo de Station
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str()); // Inicializa o wifi com as potenciais novas credenciais
        conf = SD.open(CONF_DIR, FILE_WRITE); // Abre o arquivo de configuracao em modo de escrita

        /* Escreve no arquivo as novas configuracoes geradas */
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

        conf.close();             // Fecha o arquivo
        ESP.restart();            // Reinicia o ESP
      }
      while(digitalRead(CONF_PIN)) {vTaskDelay(5);} // Mantem o programa preso em um loop (teoricamente nao deve chegar ate aqui)
    }
    vTaskDelay(5);                // Pequeno delay para evitar overflow de memoria
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
  WiFi.mode(WIFI_STA);  // Confiugra o wifi para  modo de estacao
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str()); // Inicializa o wifi com as configuracoes no cartao sd
  while(1)
  {
    if (WiFi.status() != WL_CONNECTED) {      // Se o wifi nao estiver conectado
      if (serial_debug) {
        Serial.print("Conectando ao WiFi");
      }
      while (WiFi.status() != WL_CONNECTED) { // Tenta realizar a reconexao
        WiFi.reconnect();
        vTaskDelay(1000/portTICK_PERIOD_MS);
        if (serial_debug) {
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
  WiFiClient client;  // Cria o cliente WiFi
  HTTPClient http;    // Cria o objeto para chamar as funcoes http

  http.begin(client, server_url.c_str()); // Inicializa a conexao com o servidor http
  if (server_auth)                        // Realiza a autenticacao caso o servidor exija
    http.setAuthorization(server_usr.c_str(), server_pas.c_str());
  int http_response = -1; // Para armazenar o codigo de resposta
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String temp = "[";                      // Inicializa o JSON array para post
      String aux;                             // Variavel auxiliar para montar o post
      File post_ro = SD.open(POST_DIR, FILE_READ);  // Abre o arquivo de post como somente leitura
      while(1)
      {
        aux = post_ro.readStringUntil('\n');  // Le ate a quebra de linha
        if(aux.isEmpty())                     // Se a leitura for vazia, chegou ao final do arquivo
          break;                              // Sai do loop
        aux.remove(aux.length()-1);           // Se ainda nao tiver saido do loop, remove o caractere lixo que gera ao final de quebra de linhas
        temp += aux + ",";                    // Complementa a string para post
      }
      post_ro.close();                        // Fecha o arquivo
      SD.remove(POST_DIR);                    // Deleta o arquivo
      SD.open(POST_DIR, FILE_WRITE, true);    // Recria o arquivo
      temp.remove(temp.length()-1);           // Remove a ultima virgula
      /*  Se o arquivo estava vazio, nao vai existir uma virgula, entao esta funcao removera o '[' que foi gerado na inicializacao da string
       *  resultando em  uma string vazia. */
      if(!temp.isEmpty()) // Se a string nao for vazia
      {
        temp.concat("]"); // Fecha o JSON array
        do
        {
          http_response = http.POST(temp);                    // Tenta fazer o POST
          if (serial_debug) {
            Serial.println(temp);                             // Exibe o JSON no Serial
            Serial.print("HTTP Response Code: ");             // Exibe o codigo de resposta no Serial
            Serial.println(http_response);
          }
          vTaskDelay(5000/portTICK_PERIOD_MS);                // Espera 5 segundos antes de tentar realizar outro POST
        } while (http_response != 200);                       // Continua tentando fazer POST enquanto nao obtiver sucesso
        http_response = -1;
      }
    }
    vTaskDelay(5);
  }
}

void lcd_task (void *pvParameters)
{
  /* Exibe a mensagem padrao no LCD */
  lcd.setCursor(0, 0);
  lcd.print(" ZZTECH MONITOR ");
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
      lcd.print(" ZZTECH MONITOR ");
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
  char temp[2000];            // String auxiliar para mostrar informacoes no d20ay
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))  // Caso a leitura do pino seja LOW
    {
      marcaOff[ind] = millis();     // Armazena o momento em que o rele foi desativado
      time(&seg);                   // Armazena a data e hora atuais em segundos
      timeinfo = localtime(&seg);   // Converte a data e hora para uma struct que premite a leitura mais dinamica das informacoes de data e hora// Armazena as informacoes de data e hora na string auxiliar
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;                   // String para escrever no arquivo
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);  // Abreo arquivo em modo de "append" (escrita apenas no final)
      post.println(aux);            // Escreve os dados no arquivo
      post.close();                 // Fecha o arquivo
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS); // Verifica se uma mensagem não acabou de ser exibida no lcd. Caso tenha, aguarda um pequeno tempo
        lcd_timer = marcaOff[ind];  // Atribui esse tempo para o timer do LCD (isso indicara para a task do LCD que ele deve esperar o tempo de timeout antes de exibir a mensagem padrao novamente)
        lcd.clear();                // E exibe a informacao de que o rele foi desativado, bem como o horario de sua desativacao
        lcd.setCursor(0,0);
        lcd.print("R0 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}  // Mantem a execucao presa em um loop enquanto a leitura do rele permanecer a mesma para evitar que esses comandos sejam executados continuamente
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))  // Realiza o mesmo procedimento para quando a leitura do pino for HIGH
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R0 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
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
  char temp[20];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOff[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R1 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
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
  char temp[20];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOff[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R2 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R2 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
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
  char temp[20];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOff[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R3 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R3 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
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
  char temp[20];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOff[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R4 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R4 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
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
  char temp[20];
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOff[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R5 Desativado");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      marcaOn[ind] = millis();
      time(&seg);
      timeinfo = localtime(&seg);
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      if(serial_debug)
        Serial.println(aux);
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 150) vTaskDelay(150/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("R5 Reativado");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(5);}
    }
    vTaskDelay(5);
  }
}

void set_sdcard ()
{
  /* Tenta inicializar o cartao sd, e avisa caso nao consiga */
  if (!SD.begin(5)) {
    if (serial_debug)
      Serial.println("Falha no Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("FALHA NO SD CARD");
    sd_error();
  }

  /* Verifica o tipo do cartao sd, e avisa caso nao tenha um conectado */
  uint8_t tipoSD = SD.cardType();
  if (tipoSD == CARD_NONE) {
    if (serial_debug)
      Serial.println("Sem Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("SEM SD CARD");
    sd_error();
  }

  /* Abre o arquivo de configuracao, ou cria um caso nao exista */
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
  }
  conf.close();

  /* Abre o arquivo de post, ou cria um caso nao exista */
  if (!SD.exists(POST_DIR)) {
    post = SD.open(POST_DIR, FILE_WRITE);
    post.close();
  }
  if (serial_debug) Serial.println("Cartao SD OK.");
}

void get_conf ()
{
  bool missing_conf = 0;    // Variavel para controle de configuracoes faltantes
  conf = SD.open(CONF_DIR); // Abre o arquivo de configuracao

  /* Avisa de erro caso nao consiga abrir o arquivo (ele deveria ter sido criado na funcao set_sdcard()
     portanto se aqui ele ainda nao existe, algum erro ocorreu) */
  if (!conf)
  {
    if (serial_debug)
      Serial.println("Falha no Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("FALHA NO SD CARD");
    sd_error();
  }

  String temp;                            // String temporaria para armazenar a leitura do cartao sd

  /* Le o nome da maquina */
  conf.readStringUntil('=');              // Faz a leitura ate o sinal de igual (=) e nao salva em lugar nenhum, apenas para consumir o texto de parametro no arquivo (e.g. "WIFI_SSID")
  temp = conf.readStringUntil('\n');      // Agora faz a leitura ate a primeira quebra de linha e armazena em temp
  temp.remove(strlen(temp.c_str())-1, 1); // Remove o ultimo caractere (ler ate uma quebra de linha gera um caractere de lixo na ultima posicao)
  if (temp.isEmpty())                     // Caso nao tenha nada no arquivo
    missing_conf = 1;                     // Ativa a variavel de configuracoes faltantes
  else
  {
    nome_maq = temp;                      // Caso contrario salva em sua respectiva variavel (a leitura dos demais parametros segue de forma analoga)
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
      temp = conf.readStringUntil(',');       // Para o apelido dos reles, como sao varios e sao separados por virgula, nao ha a remocao do ultimo caractere por nao ser uma quebra de linha
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

  for (int i = 0; i < QNT_RELE; i++) {
    if (rele_nick[i] == "UNUSED")
      used_rele[i] = 0;
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

void scroll_msg (void *pvParameters)
{
  str0 = "                " + str0 + "                ";// Adiciona espacos para nao deixar lixo no lcd
  int i = 16;                                           // Inicializa um indice auxiliar em 16 para comecar o scroll a partir do inicio da mensagem
  int len0 = str0.length()-16;                            // Inicializa uma variavel para armazenar o tamanho da mensagem com somente um dos 16 espacos em branco                         // 0 = linha de cima, 1 = linha de baixo, 2 = ambas
  str1 = "                " + str1 + "                ";// Adiciona espacos para nao deixar lixo no lcd
  int j = 16;                                           // Inicializa um indice auxiliar em 16 para comecar o scroll a partir do inicio da mensagem
  int len1 = str1.length()-16;                            // Inicializa uma variavel para armazenar o tamanho da mensagem com somente um dos 16 espacos em branco
  /* Fica em um loop enquanto a variavel global "scroll" estiver ativa
   * Esta variavel foi declarada como "volatile", portanto o compilador ira checar seu estado em cada loop
   * Em seu comportamento comum, o compilador checaria o estado da variavel apenas na primeira vez, e esperaria que esta variavel alterasse dentro do loop
   * Ao declarar como "volatile" voce avisa o compilador que esta variavel pode ser alterada por outras rotinas, portanto o while ira sempre checar o estado dela
   */
  while (scroll) {
    if (ctrl == 0 || ctrl == 2) { // 0 = linha de cima, 1 = linha de baixo, 2 = ambas
      if (i > len0) i = 0;      // Reinicia o indice auxiliar
      lcd.setCursor(0, 0);      // Posiciona o cursor na linha selecionada
      lcd.print(str0.substring(i, i+16));  // printa uma substring de tamanho 16
      if (i == 16) vTaskDelay(1000/portTICK_PERIOD_MS); // Se o indice estiver em 16, ou seja, no inicio da mensagem, faz um delay um pouco maior
      else vTaskDelay(250/portTICK_PERIOD_MS);  // Caso contrario faz um delay de 250 ms
      i++;                      // Incrementa o indice auxiliar
    }
    if (ctrl == 1 || ctrl == 2) {
      if (j > len1) j = 0;      // Reinicia o indice auxiliar
      lcd.setCursor(0, 1);      // Posiciona o cursor na linha selecionada
      lcd.print(str1.substring(j, j+16));  // printa uma substring de tamanho 16
      if (j == 16) vTaskDelay(1000/portTICK_PERIOD_MS); // Se o indice estiver em 16, ou seja, no inicio da mensagem, faz um delay um pouco maior
      else vTaskDelay(250/portTICK_PERIOD_MS);  // Caso contrario faz um delay de 250 ms
      j++;                      // Incrementa o indice auxiliar
    }
  }
  vTaskDelete(NULL);        // Mata esta task
}

void loop() {}
