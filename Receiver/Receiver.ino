#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include "RF24.h"
#include <printf.h>


#define ONEWIRE_PIN 18                         // пин A4 для датчика DS18B20
#define LIGHT_OUT_PIN  8                       // пин D6 для подключения ко входу магнитолы, управляющему включением подсветки, не нужен если подключено от проводки авто
#define FAN_OUT_PIN 19                         // пин А5 для управления вентилятором охлаждения
#define FAN_ON true                            /* установить в false, если нужно инвертировать выход управления вентилятором, в случае управления через PNP-транзистор,
                                                  оставить true, если транзистор NPN */
#define LIGHT_ON true                          /* установить в false, если нужно инвертировать выход "подсветка", в случае управления через PNP-транзистор,
                                                  true в случае прямого подключения, или NPN-транзистор */
const byte hold = 100;                         /* длительность импульсов энкодера,  желательно подобрать минимально возможный,
                                                  который четко будет понимать аналоговый вход KEY1/KEY2 магнитолы */
const uint16_t keyHold = 500;                  /* длительность "нажатия" кнопок, так как передатчик не передает для них код отпускания, желательно подобрать минимально
                                                  возможный, который четко будет понимать аналоговый вход KEY1/KEY2 магнитолы */
const int8_t maxTemp = 45;                     /* температура, при которой включаем вентилятор, не стоит сильно завышать, температура радиатора всегда ниже реальной
                                                  на усилителе и процессоре магнитолы */
const int8_t destTemp = 30;                    /* температура, к которой стремимся, не стоит занижать, вентилятор не холодильник, нужно быть реалистом, ниже
                                                  температуры в салоне+10 градусов врядли удастся опустить температуру. Вы рискуете заставить вращатся вентилятор
                                                  постоянно*/
const uint8_t tempSensPeriod = 30;             /* период измерения температуры, в секундах, максимум 255, но не стоит ставить менше 5-ти чтобы не саморазогревать датчик
                                                  (реальный период будет на одну секунду больше, так как 1000мс отводится датчику для подготовки данных)*/


bool learning = false;
bool done = false;
bool keyDone = false;


OneWire oneWire(ONEWIRE_PIN);

DallasTemperature tempSensor(&oneWire);

RF24 radio(9, 10);                             // радио на пинах 9 и 10

byte addresses[][6] = {"1Node", "2Node"};      //возможные номера труб
unsigned long loopTime = 0;
unsigned long tempSensTime = 0;





void resetPins() {
  for (byte i = 2; i <= 7; i++) {
    pinMode(i, INPUT);
  }
  for (byte i = 14; i <= 17; i++) {
    pinMode(i, INPUT);
  }

}





void setup() {
  pinMode(FAN_OUT_PIN, OUTPUT);                 // задаем статус пинов и устанавливаем в начальное состояние, для вентилятора и выхода подсветки
  digitalWrite(FAN_OUT_PIN, !FAN_ON);
  pinMode(LIGHT_OUT_PIN, OUTPUT);
  digitalWrite(LIGHT_OUT_PIN, !LIGHT_ON);

  for (byte i = 2; i <= 7; i++) {               // устанавливаем состояния пинов с резисторами в нужное нам состояние выходов с "подтяжкой к земле"
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  for (byte i = 14; i <= 17; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  resetPins();                        /* функция устанавливает все пины с резисторами в состояние входов, что обеспечивает бесконечно высокий импенданс
                                         относительно "земли". Такая "игра" с изменением статуса вывода "вход <-> выход" обеспечивает нам нужный функционал  */

  Serial.begin(115200);
  printf_begin();
  tempSensor.begin();                            // запуск библиотеки температурного датчика

  radio.begin();                                 // запуск библиотеки радиомодуля
  delay(250);
  radio.setPALevel(RF24_PA_LOW);                 // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);               // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  radio.setAutoAck(1);                           // режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(1, 15);                       // (время между попыткой достучаться, число попыток)
  radio.enableAckPayload();                      // разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);                      // размер пакета, в байтах
  radio.setChannel(0x2f);                        // выбираем канал (в котором нет шумов!)
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1, addresses[0]);        // хотим слушать трубу 0
  radio.powerUp();                               // начать работу
  radio.startListening();                        // начинаем слушать эфир, мы приёмный модуль
  radio.printDetails();                          // печатаем системную информацию радиомодуля при старте
  
  tempSensTime = millis();                       // начинаем отсчет периода чтения датчика температуры
}




void loop() {
  byte inData = 20;
  byte resistorPin = 0;
  int Temp;

  if (millis() - tempSensTime >= tempSensPeriod * 1000) {                         // спустя запрограммированный период чтения температуры посылаем запрос датчику
    tempSensor.requestTemperatures();                                             // на подготовку данных
  }
  if (millis() - tempSensTime >= (tempSensPeriod * 1000) + 1000) {                // а теперь, спустя 1000мс после запроса датчика, вычитываем данные с датчика
    Temp = tempSensor.getTempCByIndex(0);
    Serial.print(F("Temperature: "));
    Serial.println(Temp);
    tempSensTime = millis();

    if (Temp >= maxTemp and digitalRead(FAN_OUT_PIN) == !FAN_ON) {                         // если температура превысила верхний порог, включаем вентилятор
      Serial.println(F("The cooler was turned ON"));
      digitalWrite(FAN_OUT_PIN, FAN_ON);
    }
    if (Temp <= destTemp and digitalRead(FAN_OUT_PIN) == FAN_ON) {                         // если температура достигла нужного значения, выключаем вентилятор
      Serial.println(F("The cooler was turned OFF"));
      digitalWrite(FAN_OUT_PIN, !FAN_ON);
    }
  }

  while (radio.available()) {                   // слушаем эфир
    radio.read(&inData, sizeof(inData));        // читаем входящий сигнал
    Serial.print(F("Recived Code "));
    Serial.println(inData);
  }

  if (inData > 100) {                          // проверяем передает ли передатчик код "подсветка включена", к коду кнопки передатчик плюсует значение 100
    inData = inData - 100;                     // очищаем код команды от служебного кода 100
    digitalWrite(LIGHT_OUT_PIN, LIGHT_ON);                                             // если код 100+код кнопки есть, включаем подсветку на магнитоле
  } else if (inData != 20) digitalWrite(LIGHT_OUT_PIN, !LIGHT_ON);          // если 100 отсутствует в принятом коде то выключаем подсветку


  if (inData > 30) {                                         // проверяем, не получен ли код 30+код клавиши, если это так, - мы в режиме "обучение магнитолы"
    learning = true;
    inData = inData - 30;                                    // очищаем код команды от служебного кода 30
  }
  if (inData == 9 or inData == 10) {                         // проверяем не от энкодера ли пришлел код, если это так, то взводим "таймер отпускания" для энкодера
    loopTime = millis();
    done = true;
  }
  if (inData >= 1 and inData <= 8) {                // проверяем не от кнопок ли пришел код, если это так, то взводим "таймер отпускания" для кнопок
    loopTime = millis();
    keyDone = true;
  }

  switch (inData) {                            // выполняем установку номера резисторного пина в соответствии полученому коду
    case 1:
      Serial.println(F("Click UP"));
      resistorPin = 2;
      break;
    case 2:
      Serial.println(F("Click CENTR"));
      resistorPin = 3;
      break;
    case 3:
      Serial.println(F("Click DOWN"));
      resistorPin = 4;
      break;
    case 4:
      Serial.println(F("Holded UP"));
      resistorPin = 14;
      break;
    case 5:
      Serial.println(F("Holded CENTR"));
      resistorPin = 15;
      break;
    case 6:
      Serial.println(F("Holded Down"));
      resistorPin = 16;
      break;
    case 7:
      Serial.println(F("Click SW"));
      resistorPin = 5;
      break;
    case 8:
      Serial.println(F("Holded SW"));
      resistorPin = 17;
      break;
    case 9:
      Serial.println(F("Enc turn RIGHT >>"));
      resistorPin = 6;
      break;
    case 10:
      Serial.println(F("Enc turn LEFT <<"));
      resistorPin = 7;
      break;
    default:                                              // отключаем все резисторы в случае если код клавиши неизвестен
      if (inData != 20) {
        if (!learning) {
          resetPins();
        }
        if (inData != 0) Serial.println(F("Unknown Code")), Serial.println();
      }
      break;
  }

  if (inData >= 1 and inData <= 10) {                          // если код команды верный, ресетим все пины с резисторами, и активируем нужный
    resetPins();

    pinMode(resistorPin, OUTPUT);
    digitalWrite(resistorPin, LOW);

    Serial.print(F("Controller PIN "));
    Serial.print(resistorPin);
    Serial.println();
  }

  if (done and (millis() - loopTime >= hold + (learning ? 2000 : 0))) {                  //обрабатываем таймаут импульсов энкодера, "таймер отпускания" который ввзвели выше
    Serial.println(F("Encoder timeout"));
    resetPins();
    done = false;
  }

  if (keyDone and (millis() - loopTime >= keyHold + (learning ? 2000 : 0))) {            //обрабатываем таймаут импульса кнопок, "таймер отпускания" который ввзвели выше
    Serial.println(F("Key timeout"));
    resetPins();
    keyDone = false;
  }
}
