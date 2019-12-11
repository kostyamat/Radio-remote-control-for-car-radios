
#include <SPI.h>                      // библиотека для работы с шиной SPI
//#include "nRF24L01.h"                 // библиотека радиомодуля
#include "RF24.h"                     // ещё библиотека радиомодуля
#include "GyverButton.h"              // библиотека для кнопки "Центр", нужна для поддержки мутаций по зажатию этой кнопки
#include "GyverEncoder.h"             // библиотека для Энкодера, единственная, которая нормально поддерживает энкодер от мышки
#include <printf.h>
#include <EEPROM.h>


RF24 radio(9, 10);                    // "создать" радио на пинах 9 и 10 Для Уно\Нано
uint8_t addresses[][6] = {"1Node", "2Node"};          //возможные номера труб

#define CLK 3                         // пин А энкодера
#define DT 2                          // пин Б энкодера
#define SW 7                          // пин для кнопки энкодера
#define buzzer 14                     // пин для подключения буззера (очень желательно использовать пасивный буззер, или маленький динамик)
#define buttonUp  5                   // пин для кнопки "Вверх"
#define buttonCentr 4                 // пин для кнопки "Центр"
#define buttonDown  6                 // пин для кнопки "Вниз"
#define light  8

#define debounce 50                   // настройка антидребезга кнопок, по умолчанию 50 мс
#define timeOut 300                   // настройка таймаута на удержание кнопки (по умолчанию 500 мс)
#define clickTimeOut 200              // настройка таймаута между кликами, по факту многокликовость в программе не используется

#define Tone1 2000                    // высота тона клика по клавишам, в герцах
#define Tone2 2500                    // высота тона зажатых клавиш, в герцах

Encoder enc1(CLK, DT, SW);            // "создать" энкодер на пинах, обозначеных дефайнами CLK, DT, SW
GButton buttonU(buttonUp);            // "создать" кнопку на пине, обозначенном в дефайне buttonUp
GButton buttonC(buttonCentr);         // "создать" кнопку на пине, обозначенном в дефайне buttonCentr
GButton buttonD(buttonDown);          // "создать" кнопку на пине, обозначенном в дефайне buttonDown


bool flag_Encoder;                    // защелка для кнопки "Енкодер"
bool flag_Light;                      // защелка для кнопки "Подсветка"
bool learning = false;                // флаг режима обучения магнитолы, на приемнике формируется длительная задержка после нажатия каждой кнопки, чтобы магнитола могла стабильно запоминать клавишу

bool enableBuzzer = true;
bool flag = false;
bool done = false;


uint8_t lightOn = 0;
uint8_t check_answer = 0;
uint16_t toneVal;
byte buzzerEn;






void setup() {
  Serial.begin(115200);
  printf_begin();
  EEPROM.begin();

  // ----------- Делаем пины входами и включаем внутреннюю подтяжку к плюсу питания, для всех входов кнопок
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonCentr, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);

  if (EEPROM.read(100) == 127) {
    enableBuzzer = true;
  } else enableBuzzer = false;

  if (!digitalRead(buttonCentr)) enableBuzzer = !enableBuzzer;                 // если при включении зажата клавиша "Центр" переключаем режим буззера вкл\выкл
  EEPROM.update(100, (enableBuzzer ? 127 : 222));

  if (!digitalRead(buttonUp) and !digitalRead(buttonDown)) learning  = true;  // зажатие при включении клавиш "Вверх" и "Вниз активирует режим обучения магнитолы

  enc1.setType(TYPE1);                    // тип энкодера TYPE1 одношаговый, TYPE2 двухшаговый. Если энкодер работает странно, смените тип


  pinMode(light, INPUT);                  // вход "подсветка" если есть такой сигнал в руле
  pinMode(buzzer, OUTPUT);



  //----------- Настройки кнопки Up
  buttonU.setDebounce(debounce);                // настройка антидребезга (по умолчанию 80 мс)
  buttonU.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonU.setClickTimeout(clickTimeOut);        // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);               // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND), LOW_PULL  - кнопка подключена к VCC, пин подтянут к GND
  buttonC.setDirection(NORM_OPEN);          // NORM_OPEN - нормально-разомкнутая кнопка, NORM_CLOSE - нормально-замкнутая кнопка

  //----------- Настройки кнопки Centr
  buttonC.setDebounce(debounce);                  // настройка антидребезга (по умолчанию 80 мс)
  buttonC.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonC.setClickTimeout(clickTimeOut);             // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);               // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND), LOW_PULL  - кнопка подключена к VCC, пин подтянут к GND
  buttonC.setDirection(NORM_OPEN);          // NORM_OPEN - нормально-разомкнутая кнопка, NORM_CLOSE - нормально-замкнутая кнопка

  //----------- Настройки кнопки Down
  buttonD.setDebounce(debounce);                  // настройка антидребезга (по умолчанию 80 мс)
  buttonD.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonD.setClickTimeout(clickTimeOut);             // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);               // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND), LOW_PULL  - кнопка подключена к VCC, пин подтянут к GND
  buttonC.setDirection(NORM_OPEN);          // NORM_OPEN - нормально-разомкнутая кнопка, NORM_CLOSE - нормально-замкнутая кнопка




  radio.begin();                          //активировать библиотеку радио-модуля
  delay(250);
  radio.setPALevel(RF24_PA_LOW);          //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);        //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS, должна быть одинакова на приёмнике и передатчике
  radio.setAutoAck(1);                    //режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(1, 15);                //(время между попыткой достучаться, число попыток)
  radio.enableAckPayload();               //разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);               //размер пакета, количество байт
  radio.setChannel(0x2f);                 //выбираем канал
  radio.openWritingPipe(addresses[0]);    //мы - труба 0, открываем канал для передачи данных
  radio.openReadingPipe(1, addresses[1]); // труба 1, резервировано для получения ответной информации с приемника
  radio.powerUp();                        //начать работу радиомодуля
  radio.stopListening();                  //не слушаем радиоэфир, мы передатчик
  radio.printDetails();                   // печатаем системную информацию радиомодуля при старте

}





void loop() {
  uint8_t keyInt = 0;
  enc1.tick();                            // обязательная функция отработки энкодера. Должна постоянно опрашиваться

  // ----------------- обязательные функции отработки кнопок. Кнопки должны постоянно опрашиваться
  buttonU.tick();
  buttonC.tick();
  buttonD.tick();


  // --------------- Отрабатываем клики кнопок
  if (buttonU.isClick()) {
    keyInt = Rutina(1);
    Serial.println(F("Click on button CENTR"));
    done = true;
    toneVal = Tone1;
  }

  if (buttonC.isClick()) {
    keyInt = Rutina(2);
    Serial.println(F("Click on button CENTR"));
    done = true;
    toneVal = Tone1;
  }

  if (buttonD.isClick()) {
    keyInt = Rutina(3);
    Serial.println(F("Click on button CENTR"));
    done = true;
    toneVal = Tone1;
  }


  // --------------- Отрабатываем зажатие кнопок
  if (buttonU.isHolded()) {
    keyInt = Rutina(4);
    Serial.println(F("Pressed button UP"));
    done = true;
    toneVal = Tone2;
  }

  if (buttonC.isHolded()) {
    keyInt = Rutina(5);
    Serial.println(F("Pressed button CENTR"));
    done = true;
    toneVal = Tone2;
  }

  if (buttonD.isHolded()) {
    keyInt = Rutina(6);
    Serial.println(F("Pressed button DOWN"));
    done = true;
    toneVal = Tone2;
  }


  // ---------- Обрабатываем клик на кнопке энкодера
  if (enc1.isClick()) {
    Serial.println(F("Click on encoder switch"));
    keyInt = Rutina(7);
    done = true;
    toneVal = Tone1;
  }

  if (enc1.isHolded()) {                                     // если была нажата кнопка энкодера
    done = true;
    keyInt = Rutina(8);
    Serial.println(F("Encoder switch holded"));
    toneVal = Tone2;
  }

  if (enc1.isRightH()) {                                    // если был поворот вправо, зажатый энкодер, будем считать, что нажата кнопка энкодера, а поворот случаен
    done = true;
    keyInt = Rutina(7);
    Serial.println(F("-> Right Holded"));
    toneVal = Tone1;
  }
  if (enc1.isLeftH()) {                                    // если был поворот вправо, зажатый энкодер, будем считать, что нажата кнопка энкодера, а поворот случаен
    done = true;
    keyInt = Rutina(7);
    Serial.println(F("<- Left Holded"));
    toneVal = Tone1;
  }

  if (enc1.isRight()) {                                    // если был поворот вправо encoder
    done = true;
    keyInt = Rutina(9);
    Serial.println(F("-> Right"));
    toneVal = Tone1;
  }
  if (enc1.isLeft()) {                                     // если был поворот влево encoder
    done = true;
    keyInt = Rutina(10);
    Serial.println(F("<- Left"));
    toneVal = Tone1;
  }



  if (done) {
    if (enableBuzzer) tone(buzzer, toneVal, 100);
    Serial.println(keyInt);
    if (!radio.write(&keyInt, sizeof(keyInt))) {           // отправить код по радио
      Serial.println(F("Sending failed"));
      if (enableBuzzer) {
        tone(buzzer, 2500, 20);
        delay(50);
        tone(buzzer, 2250, 20);
        delay(50);
        tone(buzzer, 2000, 20);
      }
    }
    Serial.println();
    done = false;
  }
}




int Rutina(byte key) {

  if (digitalRead(light) and !flag_Light) {           // читаем состояние подсветки
    Serial.println(F("Light is ON"));
    flag_Light = true;
    lightOn = 100;
    done = true;
  }
  if (!digitalRead(light) and flag_Light) {            // отрабатываем отключение подсветки
    Serial.println(F("Light is ON"));
    flag_Light = false;
    lightOn = 0;
    done = true;
  }

  return key + lightOn + (learning ? 30 : 0);
}
