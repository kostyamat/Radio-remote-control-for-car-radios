#include <SPI.h>
#include "RF24.h"
#include <printf.h>



#define light  8                               // пин D6 для подключения ко входу магнитолы, управляющему включением подсветки, не нужен если подключено иначе

const bool revers = true;                      /* установить в false, если нужно инвертировать выход "подсветка", в случае управления через транзистор,
                                                  с управлением минусом */
const byte hold = 100;                      /* таймаут импульсов энкодера, так как передатчик не передает код 0 для энкодера,
                                                  чтобы не выбивать передатчик из цикла
                                                  желательно подобрать минимально возможный,
                                                  который четко будет понимать аналоговый вход KEY1/KEY2 магнитолы */
const uint16_t keyHold = 500;                  /* таймаут импульсов кнопки энкодера и центральной клавиши, так как передатчик не передает для них код 0,
                                                  чтобы не выбивать передатчик из цикла
                                                  желательно подобрать минимально возможный,
                                                  который четко будет понимать аналоговый вход KEY1/KEY2 магнитолы */


bool learning = false;
bool done = false;
bool keyDone = false;



RF24 radio(9, 10);                             // радио на пинах 9 и 10

byte addresses[][6] = {"1Node", "2Node"};      //возможные номера труб
unsigned long loopTime = 0;





void resetPin() {
  for (byte i = 2; i <= 7; i++) {
    pinMode(i, INPUT);
  }
  for (byte i = 14; i <= 17; i++) {
    pinMode(i, INPUT);
  }

}





void setup() {

  for (byte i = 2; i <= 7; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  for (byte i = 14; i <= 16; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  resetPin();

  Serial.begin(115200);
  printf_begin();

  radio.begin(); //активировать модуль
  delay(250);
  radio.setPALevel(RF24_PA_LOW);                //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);               //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  radio.setAutoAck(1);                           //режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(1, 15);                       //(время между попыткой достучаться, число попыток)
  radio.enableAckPayload();                      //разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);                      //размер пакета, в байтах
  radio.setChannel(0x2f);                        //выбираем канал (в котором нет шумов!)
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1, addresses[0]);        //хотим слушать трубу 0

  radio.powerUp(); //начать работу
  radio.startListening();                        //начинаем слушать эфир, мы приёмный модуль
  radio.printDetails();                   // печатаем системную информацию радиомодуля при старте
}




void loop() {
  byte inData = 20;
  byte levelR = 0;

  while (radio.available()) {                   // слушаем эфир
    radio.read(&inData, sizeof(inData));        // читаем входящий сигнал
    Serial.print(F("Recived Code "));
    Serial.println(inData);
  }

  if (!learning and inData > 100) {                          // проверяем передает ли передатчик код "подсветка включена", к коду кнопки передатчик плюсует значение 100
    inData = inData - 100;
    digitalWrite(light, revers);                                             // если код 100 + код кнопки есть, включаем подсветку на магнитоле
  } else if (!learning and inData != 20) digitalWrite(light, !revers);          // если 100 отсутствует в принятом коде то выключаем подсветку


  if (inData > 30) {
    learning = true;  // вычитаем идентификатор mode2 из кода (если есть), чтобы потом проверить не от энкодера пришел ли код. Энкодер посылает коди 4-6 (mode 1) или 14-16 (mode 2) */
    inData = inData - 30;
  }
  if (inData == 9 or inData == 10) {                // проверяем не пришел ли код от энкодера, и взводим таймер
    loopTime = millis();
    done = true;
  }
  if (inData >= 1 and inData <= 8) {                // проверяем не пришел ли код от кнопки "Центр" или кнопки єнкодера, и взводим таймер
    loopTime = millis();
    keyDone = true;
  }

  switch (inData) {                            // выполняем установку номера пина в соответствии полученому коду
    case 1:
      Serial.println(F("Click UP"));
      levelR = 2;
      break;
    case 2:
      Serial.println(F("Click CENTR"));
      levelR = 3;
      break;
    case 3:
      Serial.println(F("Click DOWN"));
      levelR = 4;
      break;
    case 4:
      Serial.println(F("Holded UP"));
      levelR = 14;
      break;
    case 5:
      Serial.println(F("Holded CENTR"));
      levelR = 15;
      break;
    case 6:
      Serial.println(F("Holded Down"));
      levelR = 16;
      break;
    case 7:
      Serial.println(F("Click SW"));
      levelR = 5;                             // пин А0
      break;
    case 8:
      Serial.println(F("Holded SW"));
      levelR = 17;                             // пин А1
      break;
    case 9:
      Serial.println(F("Enc turn RIGHT >>"));
      levelR = 6;                             // пин А2
      break;
    case 10:
      Serial.println(F("Enc turn LEFT <<"));
      levelR = 7;
      break;


    default:                                              // отключаем все резисторы в случае если код клавиши неизвестен, или пришел "отбой" по коду 0 или 10
      if (inData != 20) {
        if (!learning) {
          resetPin();
        }
        if (inData != 0) Serial.println(F("Unknown Code")), Serial.println();
      }
      break;
  }

  if (inData >= 1 and inData <= 10) {
    resetPin();

    pinMode(levelR, OUTPUT);
    digitalWrite(levelR, LOW);

    Serial.print(F("Controller PIN "));
    Serial.print(levelR);
    Serial.println();
  }

  if (done and (millis() - loopTime >= hold + (learning ? 2000 : 0))) {                  //обрабатываем таймаут импульсов энкодера
    Serial.println(F("Encoder timeout"));
    resetPin();
    done = false;
  }

  if (keyDone and (millis() - loopTime >= keyHold + (learning ? 2000 : 0))) {            //обрабатываем таймаут импульса кнопки "Центр" или кнопки энкодера
    Serial.println(F("Key timeout"));
    resetPin();
    keyDone = false;
  }
}
