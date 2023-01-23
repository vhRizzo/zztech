#define QNT_RELE 1

uint8_t pin[QNT_RELE] = {35};
volatile int estadoAnterior[QNT_RELE] = {HIGH};
volatile unsigned long marcaOff[QNT_RELE] = {0};
volatile unsigned long marcaOn[QNT_RELE] = {0};
bool rst = false;

void IRAM_ATTR itr0()
{
  int estadoAtual = digitalRead(pin[0]);
  if (estadoAtual == estadoAnterior[0]) return;
  if (estadoAtual == LOW)
  {
    marcaOff[0] = millis();
    estadoAnterior[0] = estadoAtual;
  } else
  {
    marcaOn[0] = millis();
    estadoAnterior[0] = estadoAtual;    
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(pin[0], INPUT);
  attachInterrupt(digitalPinToInterrupt(pin[0]), itr0, CHANGE);
}

void loop()
{
  if(rst)
  {
    Serial.print("Tempo da desativacao: ");
    Serial.println(marcaOff[0]);
    Serial.print("Tempo da ativacao: ");
    Serial.println(marcaOn[0]);
    Serial.print("Tempo total desligado: ");
    Serial.println(marcaOn[0] - marcaOff[0]);
  }
}
