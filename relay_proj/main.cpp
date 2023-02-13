#include <LiquidCrystal_I2C.h>
#include <ESPAsyncWebServer.h>
#include <driver/gpio.h>
#include <HTTPClient.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <AsyncTCP.h>
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

/* Quantidade maxima de Reles */
#define QNT_RELE    6

/* Fuso Horario */
#define NTP_SERVER  "pool.ntp.org"  // Servidor NTP para sincronizar o relogio

#define TIMEOUT     3000            // Tempo em milissegundos que o display indicara um novo evento
#define REL_FREQ    0.5             // Tempo em minutos em que o ESP enviara um POST para informar o estado atual dos reles

#define AP_PASS     "zztech@1020"   // Senha de acesso as configuracoes

#define HTTP_RSRC   "/zziot"        // Diretorio do resource do HTTP
#define HTTP_TESTE  "/teste"        // Diretorio para teste do servidor

#define CONF_DIR    "/config.ini"   // Diretorio do arquivo de configuracao
#define POST_DIR    "/post.txt"     // Diretorio do arquivo de post
#define TEMP_DIR    "/temp.txt"     // Diretorio do arquivo temporario

#define CONF_PIN    13              // Pino do switch de configuracao
#define LED_R_PIN   4               // Pino do LED vermelho do LED RGB
#define LED_G_PIN   17              // Pino do LED verde do LED RGB
#define LED_B_PIN   16              // Pino do LED azul do LED RGB
#define I2C_SDA_PIN 26              // Pino SDA do LCD I2C
#define I2C_SCL_PIN 25              // Pino SCL do LCD I2C

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

uint8_t pin[QNT_RELE] = {33, 32, 35, 34, 39, 36};       // Pinos dos Reles

File post;                                              // Variavel para o arquivo de dados
File conf;                                              // Variavel para o arquivo de configuracao
time_t seg;                                             // Controle do relogio
bool wait = 0;                                          // Variavel para sinalizar quando uma task estiver utilizando o arquivo de post
bool def_msg = 1;                                       // Controle de mensagem padrao para ser exibida no display
bool server_auth;                                       // Controle de autenticacao de servidor
bool suspended = 0;                                     // Controle para quando estiver configurando a maquina
bool wifi_status = 0;                                   // Controle do status da conexao wifi
bool http_status = 0;                                   // Controle do status do servidor http
struct tm timeinfo;                                     // Struct que facilita a selecao das informacoes da data/hora
volatile bool conf_ok = 0;                              // Controle de recebimento de request das configuracoes
String DEFAULT_AP = "ZZTECH_MONITOR";                   // Nome padrao da maquina
bool est_rele[QNT_RELE] = {0, 0, 0, 0, 0, 0};           // Indicar o estado atual dos reles
bool ini_flag[QNT_RELE] = {0, 0, 0, 0, 0, 0};           // Indicador para evitar que o programa indique que o estado inicial do rele seja um evento
bool used_rele[QNT_RELE] = {1, 1, 1, 1, 1, 1};          // Controle de quais reles serao usados
unsigned long lcd_timer = 0;                            // Variavel auxiliar para controlar o tempo que um evento e mostrado na tela
unsigned long marcaOff[QNT_RELE] = {0, 0, 0, 0, 0, 0};  // Variavel para armazenar o instante em que um rele e desligado
unsigned long marcaOn[QNT_RELE] = {0, 0, 0, 0, 0, 0};   // Variavel para armazenar o instanto em que um rele e ligado

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE); // Inicializa o display
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
                    <label for="nome_maq"><b>Nome da máquina (Também será o SSID do Ponto de Acesso WiFi de configuração)</b></label>
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
                    Configurações salvas. 
                    Você já pode fechar esta página.
                </p>
            </div> 
        </body>
    </html>)rawliteral";
#pragma endregion

/* Prototipos de funcoes */
void del_file_line(const char* dir_path, int n_row);
void notFound(AsyncWebServerRequest *request);
void conf_task (void *pvParameters);
void wifi_task (void *pvParameters);  TaskHandle_t wifi_handle;
void http_task (void *pvParameters);  TaskHandle_t http_handle;
void lcd_task (void *pvParameters);   TaskHandle_t lcd_handle;
void led_task (void *pvParameters);
void rel_task (void *pvParameters);   TaskHandle_t rel_handle;
void r0_task (void *pvParameters);
void r1_task (void *pvParameters);
void r2_task (void *pvParameters);
void r3_task (void *pvParameters);
void r4_task (void *pvParameters);
void r5_task (void *pvParameters);
void rgb(bool R, bool G, bool B);
void initialize_sntp ();
void obtain_time();
void set_sdcard ();
void sd_error ();
void get_conf();

void setup()
{
  if (serial_debug)
    Serial.begin(115200);             // Inicializa o Serial

  /* Inicializa o LCD */
  lcd.begin(16, 2, LCD_5x8DOTS, I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" ZZTECH MONITOR ");
  lcd.setCursor(0, 1);
  lcd.print("INICIALIZANDO...");

  /* Inicializa o LED RGB */
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  digitalWrite(LED_R_PIN, LOW);
  digitalWrite(LED_G_PIN, LOW);
  digitalWrite(LED_B_PIN, LOW);

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
  xTaskCreate(http_task,  "HTTP_Task",  1024*8, NULL, configMAX_PRIORITIES -  9, &http_handle);
  xTaskCreate(led_task,   "LED_Task",   1024*2, NULL, configMAX_PRIORITIES -  15, NULL);
  xTaskCreate(conf_task,  "AP_Task",    1024*4, NULL, configMAX_PRIORITIES -  7, NULL);
  
  /* Faz a primeira sincronizacao do relogio e nao inicia o monitoramento enquanto nao estiver sincronizado */
  if (serial_debug)
    Serial.println("\"setup\" Sincronizando relogio...");
  initialize_sntp();
  while (timeinfo.tm_year+1900 < 2020)
  {
    obtain_time();
    if (timeinfo.tm_year+1900 < 2020)
      vTaskDelay(150/portTICK_PERIOD_MS);
  }
  if (serial_debug)
    Serial.println("\"setup\" Feito!");
  
  xTaskCreate(lcd_task,   "LCD_Task",   1024*4, NULL, configMAX_PRIORITIES -  9, &lcd_handle);
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
  xTaskCreate(rel_task,   "Rel_Task",   1024*4, NULL, configMAX_PRIORITIES - 10, &rel_handle);
}

void conf_task (void *pvParameters)
{
  pinMode(CONF_PIN, INPUT_PULLUP);  // Inicializa o pino do botao de configuracao em modo de pullup
  char aux[17];                     // String auxiliar para print no display
  while (1)
  {
    if (!digitalRead(CONF_PIN)) // Caso o botao de configuracao seja acionado
    {
      suspended = 1;            // Avisa as tasks pertinentes de que o sistema esta suspenso
      vTaskSuspend(rel_handle); // Suspende as tasks de relatorio, wifi, http e lcd
      vTaskSuspend(wifi_handle);
      vTaskSuspend(http_handle);
      vTaskSuspend(lcd_handle);
      WiFi.disconnect();        // Desconecta o wifi
      WiFi.mode(WIFI_AP);       // Altera o modo de wifi para Access Point
      if (serial_debug)
        Serial.printf("\n\"conf_task\" Iniciando ponto de acesso de configuracao...\n\n");
      if (nome_maq.isEmpty())   // Verifica se o campo de nome da maquina esta vazio
        nome_maq = DEFAULT_AP;  // Caso esteja, define ele como o nome padrao
      WiFi.softAP(nome_maq.c_str(), AP_PASS); // Inicia o ponto de acesso WiFi
      if (serial_debug) {
        Serial.printf("\"conf_task\" Ponto de acesso WiFi gerado.\n\"conf_task\" Acesse com as credenciais abaixo para configurar a maquina\n");
        Serial.printf("\"conf_task\" SSID: %s\n\"conf_task\" Senha: %s\n\n", nome_maq.c_str(), AP_PASS);
      }
      IPAddress IP = WiFi.softAPIP(); // Gera o endereco IP do ESP32
      lcd.setCursor(0, 0);
      lcd.print("MODO CONF LIGADO");
      sprintf(aux, "%16s", IP.toString().c_str());  // E exibe no display
      lcd.setCursor(0, 1);
      lcd.print(aux);

      /* Gera a pagina html de configuracao */
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
      });

      if (serial_debug) {
        Serial.print("\"conf_task\" Apos conectar ao ponto de acesso, abra seu navegador e acesse o seguinte endereco: ");
        Serial.println(IP); Serial.println();
      }

      /* Recebe os parametros passados pelo usuario e os salva em suas respectivas variaveis */
      server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        nome_maq = request->getParam(param_nome_maq)->value();
        wifi_ssid = request->getParam(param_wifi_ssid)->value();
        wifi_pass = request->getParam(param_wifi_pass)->value();
        server_url = request->getParam(param_server_url)->value();
        server_usr = request->getParam(param_server_usr)->value();
        server_pas = request->getParam(param_server_pas)->value();
        if (server_usr.isEmpty() || server_pas.isEmpty())
          server_auth = 0;
        for (int i = 0; i < QNT_RELE; i++) {
          rele_nick[i] = request->getParam(param_rele_nick[i])->value();
          if (rele_nick[i].isEmpty())
            rele_nick[i] = "UNUSED";
        }

        /* Ao final, exibe uma mensagem ao usuario indicando que as configuracoes foram salvas */
        request->send(200, "text/html", confok_html);
        conf_ok = 1;  // Se chegou aqui recebeu a get request, entao ativa a variavel de configuracao
      });
      server.onNotFound(notFound);  // Avisa caso o request retorne erro 404
      server.begin();               // Inicia o servidor

      while(!conf_ok){vTaskDelay(10);} // Mantem o programa preso em um loop enquanto nao receber uma get request
      vTaskDelay(1000/portTICK_PERIOD_MS);

      server.end();                   // Finaliza o servidor
      WiFi.softAPdisconnect();        // Desativa o Ponto de Acesso
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
      lcd.setCursor(0, 0);
      lcd.print(" CONFIG SALVAS  ");
      if (serial_debug)
        Serial.println("\"conf_task\" Configuracoes salvas.");
      for(int i = 9; i >= 0; i--) {
        sprintf(aux, "REINICIANDO EM %i", i);
        lcd.setCursor(0, 1);
        lcd.print(aux);
        if(serial_debug)
          Serial.printf("\"conf_task\" Reiniciando em %i...\n", i);
        vTaskDelay(1000/portTICK_PERIOD_MS);
      }
      ESP.restart();  // Reinicia o ESP
    }
    vTaskDelay(10);    // Pequeno delay para evitar overflow de memoria
  }
}

void wifi_task (void *pvParameters)
{
  WiFi.mode(WIFI_STA);  // Configura o wifi para modo de estacao
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str()); // Inicializa o wifi com as configuracoes no cartao sd
  while(1)
  {
    if (WiFi.status() != WL_CONNECTED) {      // Se o wifi nao estiver conectado
      int i = 0;
      if (serial_debug)
        Serial.print("\"wifi_task\" Conectando ao WiFi...");
      while (WiFi.status() != WL_CONNECTED) { // Tenta realizar a reconexao
        if (i >= 30)
          ESP.restart();
        /* Na lib WiFi existe a funcao de reconexao, porem desconectar e reiniciar e mais seguro */
        WiFi.disconnect();
        vTaskDelay(150/portTICK_PERIOD_MS);
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        vTaskDelay(850/portTICK_PERIOD_MS);
        i++;
      }
      if (serial_debug) {
        wifi_status = 1;
        Serial.println();
        Serial.println("\"wifi_task\" WiFi Conectado");
      }
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}

void http_task (void *pvParameters)
{
  WiFiClient client;        // Cria o cliente WiFi
  HTTPClient http_resource; // Cria o objeto para chamar as funcoes http
  HTTPClient http_teste;    // Cria o objeto para testar a disponibilidade do servidor
  int http_response = -1;   // Para armazenar o codigo de resposta

  while (!http_resource.begin(client, server_url + HTTP_RSRC)) { // Inicializa a conexao com o servidor http
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  if (serial_debug)
    Serial.println("\"http_task\" Servidor HTTP resource inicializado.");
  while (!http_teste.begin(client, server_url + HTTP_RSRC + HTTP_TESTE)) {
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
  if (serial_debug)
    Serial.println("\"http_task\" Servidor HTTP teste inicializado.");
  if (server_auth) { // Realiza a autenticacao caso o servidor exija
    http_resource.setAuthorization(server_usr.c_str(), server_pas.c_str());
    http_teste.setAuthorization(server_usr.c_str(), server_pas.c_str());
  }
  do {
    http_response = http_teste.GET(); // Envia uma GET request ao servidor e continua tentando ate que o servidor responda OK
    if (serial_debug)
      Serial.printf("\n\"http_task\" HTTP GET response code: %i\n", http_response);
    if (http_response != 200) {
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  } while(http_response != 200);
  http_status = 1;  // Sair do loop acima indica que o servidor esta OK, entao ativa a variavel de controle sobre o status do servidor
  if (serial_debug)
    Serial.printf("\"http_task\" Servidor HTTP GET OK.\n\n");
  while (1)
  {
    String temp = "[";                  // Inicializa o JSON array para post
    String aux;                         // Variavel auxiliar para montar o post
    File ftemp;                         // Variavel do arquivo de backup
    unsigned int n_row = 0;             // Variavel para armazenar a quantidade de linhas que foram postadas do arquivo de post
    while(wait){vTaskDelay(10);}        // Aguarda outras possiveis tasks que possam estar utilizando o arquivo
    wait = 1;                           // Antes de abrir o arquivo de post, avisa as outras task para que nao o abram
    post = SD.open(POST_DIR, FILE_READ);  // Abre o arquivo de post como somente leitura
    while(1)
    {
      aux = post.readStringUntil('\n');     // Le ate a quebra de linha
      if(aux.isEmpty())                     // Se a leitura for vazia, chegou ao final do arquivo
        break;                                // Sai do loop
      n_row++;                              // Incrementa a quantidade de linhas
      aux.remove(aux.length()-1);           // Se ainda nao tiver saido do loop, remove o caractere lixo que gera ao final de quebra de linhas
      temp += aux + ",";                    // Complementa a string para post
    }
    post.close();                         // Fecha o arquivo de post
    wait = 0;                             // Libera o arquivo para uso em outras tasks
    temp.remove(temp.length()-1);         // Remove a ultima virgula
    /* Se o arquivo estava vazio, nao vai existir uma virgula, entao esta funcao removera o '[' que foi gerado na inicializacao da string
    *  resultando em uma string vazia. */
    if(!temp.isEmpty()) // Se a string nao for vazia
    {
      temp.concat("]");                         // Fecha o JSON array
      int i = 0;
      http_response = -1;
      while (http_response != 200)              // Continua tentando fazer POST enquanto nao obtiver sucesso
      {
        i++;
        http_resource.GET();                      // Faz uma GET request (o POST estava sempre dando resposta -2 na primeira tentativa e sucesso na segunda, essa GET request "consome" o erro da primeira tentativa)
        http_response = http_resource.POST(temp); // Tenta fazer o POST
        if (serial_debug) {
          Serial.printf("\n\"http_task\" ");
          Serial.println(temp);                   // Exibe o JSON no Serial
          Serial.print("\"http_task\" HTTP Response Code: ");   // Exibe o codigo de resposta no Serial
          Serial.println(http_response);
        }
        if (i == 3)
          http_status = 0;
        if (temp.substring(0, 2) != "[{" || temp.substring(temp.length()-2, temp.length()) != "}]") // Se a string de post nao comecar e terminar no padrao JSON, provavelmente o ESP reiniciou antes de terminar de escrever os dados no arquivo
          break;                                                      // Entao sai do loop para nao tentar mais fazer o post e ficar num loop eterno retornando Bad Request
        if (http_response != 200)
          vTaskDelay(250/portTICK_PERIOD_MS);  // Espera 250 ms antes de tentar realizar outro POST
      }
      del_file_line(POST_DIR, n_row);       // Remove os dados postados do arquivo
      http_status = 1;
    }
    vTaskDelay(10);
  }
}

void lcd_task (void *pvParameters)
{
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
      while (millis() - lcd_timer >= TIMEOUT) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void led_task (void *pvParameters)
{
  while (1)
  {
    while(suspended)          // Se o programa estiver suspenso, pisca o LED em azul
    {
      rgb(0,0,1);
      vTaskDelay(1000/portTICK_PERIOD_MS);
      rgb(0,0,0);
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
    if(!wifi_status) {        // Se o WiFi estiver desconectado, o LED fica vermelho
      rgb(1,0,0);
      while(!wifi_status && !suspended){vTaskDelay(10);}
    }
    else if (!http_status) {  // Caso contrario, se o servidor HTTP estiver desconectado, o LED fica amarelo
      rgb(1,1,0);
      while(!http_status && !suspended){vTaskDelay(10);}
    }
    else {                    // Se nao tiver nenhum outro erro, o LED fica verde
      rgb(0,1,0);
      while(http_status && http_status && !suspended){vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r0_task (void *pvParameters)
{
  int ind = 0;              // Indica o indice do rele
  /* Inicializa o pino com instrucoes proprias da espressif */
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);    // Tudo isso equivale a pinMode(pin[ind], OUTPUT)
  char temp[20];            // String auxiliar para mostrar informacoes no d20ay
  while(1)
  {
    if (!gpio_get_level((gpio_num_t)pin[ind]))  // Caso a leitura do pino seja LOW
    {
      est_rele[ind] = 0;            // Armazena o estado do rele para a task de relatorio periodico
      marcaOff[ind] = millis();    
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;                   // String para escrever no arquivo
      if (!ini_flag[ind]) {         // Se for o primeiro evento desde a inicializacao do ESP, envia um evento personalizado informando o estado do rele na inicializacao
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;          // E ativa a variavel para indicar que as demais leituras nao sao mais a primeira
      }
      else                          // Caso nao seja mais a primeira, somente avisa o evento do rele
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}  // Prende a task em um loop enquanto o arquivo de post estiver aberto em alguma outra task
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);  // Abre o arquivo em modo de "append" (escrita apenas no final)
      post.println(aux);            // Escreve os dados no arquivo
      post.close();                 // Fecha o arquivo
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS); // Verifica se uma mensagem não acabou de ser exibida no lcd. Caso tenha, aguarda um pequeno tempo
        lcd_timer = marcaOff[ind];  // Atribui esse tempo para o timer do LCD (isso indicara para a task do LCD que ele deve esperar o tempo de timeout antes de exibir a mensagem padrao novamente)
        lcd.setCursor(0,0);
        lcd.print(" R0 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}  // Mantem a execucao presa em um loop enquanto a leitura do rele permanecer a mesma para evitar que esses comandos sejam executados continuamente
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))  // Realiza o mesmo procedimento para quando a leitura do pino for HIGH
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R0 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r1_task (void *pvParameters)
{
  int ind = 1;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
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
      est_rele[ind] = 0;
      marcaOff[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.setCursor(0,0);
        lcd.print(" R1 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R1 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r2_task (void *pvParameters)
{
  int ind = 2;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
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
      est_rele[ind] = 0;
      marcaOff[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.setCursor(0,0);
        lcd.print(" R2 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R2 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r3_task (void *pvParameters)
{
  int ind = 3;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
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
      est_rele[ind] = 0;
      marcaOff[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.setCursor(0,0);
        lcd.print(" R3 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R3 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r4_task (void *pvParameters)
{
  int ind = 4;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
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
      est_rele[ind] = 0;
      marcaOff[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.setCursor(0,0);
        lcd.print(" R4 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R4 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void r5_task (void *pvParameters)
{
  int ind = 5;
  gpio_config_t io_conf = {
      .pin_bit_mask = (uint64_t)(1ULL << pin[ind]),
      .mode = GPIO_MODE_INPUT,
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
      est_rele[ind] = 0;
      marcaOff[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Desativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Desativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOff[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOff[ind];
        lcd.setCursor(0,0);
        lcd.print(" R5 DESATIVADO  ");
        lcd.setCursor(0,1);
        lcd.print(temp);
      }
      while(!gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    else if (gpio_get_level((gpio_num_t)pin[ind]))
    {
      est_rele[ind] = 1;
      marcaOn[ind] = millis();
      obtain_time();
      sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      String aux;
      if (!ini_flag[ind]) {
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"ESP Ligando - Rele Ativado\",\"horario\":\"" + temp + "\"}";
        ini_flag[ind] = 1;
      }
      else
        aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[ind] + "\",\"evento\":\"Ativado\",\"horario\":\"" + temp + "\"}";
      while(wait){vTaskDelay(10);}
      wait = 1;
      post = SD.open(POST_DIR, FILE_APPEND);
      post.println(aux);
      post.close();
      wait = 0;
      if(serial_debug)
        Serial.printf("\n\"r%i_task\" %s\n", ind, aux.c_str());
      sprintf(temp, "%02d/%02d/%04d %02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min);
      if (!suspended) {
        if(marcaOn[ind] - lcd_timer < 300) vTaskDelay(300/portTICK_PERIOD_MS);
        lcd_timer = marcaOn[ind];
        lcd.setCursor(0,0);
        lcd.print("   R5 ATIVADO   ");
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
      while(gpio_get_level((gpio_num_t)pin[ind])) {vTaskDelay(10);}
    }
    vTaskDelay(10);
  }
}

void set_sdcard ()
{
  /* Tenta inicializar o cartao sd, e avisa caso nao consiga */
  if (!SD.begin(5)) {
    if (serial_debug)
      Serial.println("\"set_sdcard\" Falha no Cartao SD.");
    lcd.setCursor(0, 0);
    lcd.print("FALHA NO SD CARD");
    sd_error();
  }

  /* Verifica o tipo do cartao sd, e avisa caso nao tenha um conectado */
  uint8_t tipoSD = SD.cardType();
  if (tipoSD == CARD_NONE) {
    if (serial_debug)
      Serial.println("\"set_sdcard\" Sem Cartao SD.");
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
  if (serial_debug) Serial.println("\"set_sdcard\" Cartao SD OK.");
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
      Serial.println("\"get_conf\" Falha no Cartao SD.");
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
  else if (temp.substring(0, 7) == "http://")
  {
    server_url = temp;
  } else {
    server_url = "http://" + temp;
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
      Serial.println("\"get_conf\" Existem configuracoes necessarias faltantes.");
      Serial.println("\"get_conf\" Acione o switch de configuracao da maquina e acesse o ponto de acesso WiFi gerado para configurar.");
    }
  } else {
    def_msg = 1;
    if (serial_debug) {
      Serial.printf("\n\"get_conf\" Setup OK\n\"get_conf\" Configuracoes atuais:\n\n");
      Serial.printf("\"get_conf\" Nome da maquina = %s\n", nome_maq.c_str());
      Serial.printf("\"get_conf\" WiFi SSID = %s\n", wifi_ssid.c_str());
      Serial.printf("\"get_conf\" WiFi Password = %s\n", wifi_pass.c_str());
      Serial.printf("\"get_conf\" Servidor URL = %s\n", server_url.c_str());
      Serial.printf("\"get_conf\" Servidor Usuario = %s\n", server_usr.c_str());
      Serial.printf("\"get_conf\" Servidor Senha = %s\n\"get_conf\" Apelido dos reles = ", server_pas.c_str());
      for (int i = 0; i < QNT_RELE-1; i++)
        Serial.printf("%s, ", rele_nick[i].c_str());
      Serial.printf("%s\n\n", rele_nick[QNT_RELE-1].c_str());
    }
  }
}

void sd_error ()
{
  rgb(1,0,0);
  char temp[17];
  for (int i = 9; i >= 0; i--)
  {
    sprintf(temp, "REINICIANDO EM %i", i);
    lcd.setCursor(0, 1);
    lcd.print(temp);
    if (serial_debug)
      Serial.printf("\"sd_error\" Reiniciando em %i...\n", i);
    delay(1000);
  }
  ESP.restart();
}

void rel_task (void *pvParameters)
{
  /* Essa task tem o mesmo comportamento das tasks dos reles, mas em vez de permanecer "escutando" por eventos do rele,
   * apenas envia o estado atual do rele */
  char temp[20];
  while(1)
  {
    vTaskDelay((int)(REL_FREQ * 60000)/portTICK_PERIOD_MS);
    while(wait){vTaskDelay(10);}
    wait = 1;
    post = SD.open(POST_DIR, FILE_APPEND);
    for (int i = 0; i < QNT_RELE; i++)
    {
      if (used_rele[i])
      {
        obtain_time();
        sprintf(temp, "%02d/%02d/%04d-%02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        String aux;
        if (est_rele[i])  // Para evitar de ficar lendo o estado do rele toda hora, apenas utiliza uma variavel global para salvar o estado do rele
          aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[i] + "\",\"evento\":\"Relatorio - Ativado\",\"horario\":\"" + temp + "\"}";
        else
          aux = "{\"maquina\":\"" + nome_maq + "\",\"dispositivo\":\"" + rele_nick[i] + "\",\"evento\":\"Relatorio - Desativado\",\"horario\":\"" + temp + "\"}";
        post.println(aux);
        if(serial_debug)
          Serial.printf("\n\"rel_task\" %s\n", aux.c_str());
      }
    }
    post.close();
    wait = 0;
  }
}

void rgb (bool R, bool G, bool B)
{
  digitalWrite(LED_R_PIN, R);
  digitalWrite(LED_G_PIN, G);
  digitalWrite(LED_B_PIN, B);
}

void notFound (AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void del_file_line (const char* dir_path, int n_row)
{
  File temp;    // Arquivo temporario
  String aux;   // String auxiliar
  temp = SD.open(TEMP_DIR, FILE_WRITE, true); // Gera o arquivo temporario
  while(wait){vTaskDelay(10);}  // Aguarda o arquivo de post ficar disponivel
  wait = 1;                     // Sinaliza que o arquivo esta sendo utilizado
  post = SD.open(dir_path);     // Abre o arquivo para leitura
  for (int i = 0; i < n_row; i++) // Le a quantidade de linhas que se deseja remover (isso faz com que o cursor seja posicionado apos as linhas lidas)
    aux = post.readStringUntil('\n');
  while (1) {                   // Se mantem em um loop infinito
    aux = post.readStringUntil('\n'); // Le uma nova linha
    if (aux.isEmpty())          // Se essa linha for vazia, sai do loop
      break;
    aux.remove(aux.length()-1); // Caso nao seja, remove o \r do final da string
    temp.println(aux);          // E escreve essa linha no arquivo temporario
  }
  temp.close();                 // Fecha o arquivo temporario
  post.close();                 // Fecha o arquivo de post
  SD.remove(dir_path);          // Deleta o arquivo de post
  SD.rename(TEMP_DIR, dir_path);// Renomeia o arquivo temporario para o arquivo de post
  wait = 0;                     // E libera o arquivo para ser utilizado pelas demais tasks
}

void obtain_time ()
{
  time(&seg);
  setenv("TZ", "UTC+3", 1);
  tzset();
  localtime_r(&seg, &timeinfo);
}

void initialize_sntp ()
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_init();
}

void loop() { /* Ao utilizar tasks, a funcao loop() nao e utilizada */ }
