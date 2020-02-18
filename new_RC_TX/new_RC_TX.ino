#include <avr/sleep.h>
#include <avr/power.h>
#include <SPI.h>                      // библиотека для работы с шиной SPI
#include "RF24.h"                     // библиотека радиомодуля
#include "GyverButton.h"              // библиотека для кнопок, нужна для поддержки мутаций команд по зажатию кнопки
#include "GyverEncoder.h"             // библиотека для Энкодера, единственная которая поддерживает энкодер от мышки
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
#define timeOut 300                   // настройка таймаута на удержание кнопки
#define clickTimeOut 200              // настройка таймаута между кликами, по факту многокликовость в программе не используется

#define Tone1 2000                    // высота тона клика по клавишам, в герцах
#define Tone2 2500                    // высота тона зажатых клавиш, в герцах

const byte retryError = 5;            // количество неудавшихся сеансов передачи данных до того как даже дезактивированный буззер начнет пикать, указывая на неисправность приемника

Encoder encoder(CLK, DT, SW);         // "создать" энкодер на пинах, обозначеных дефайнами CLK, DT, SW
GButton buttonU(buttonUp);            // "создать" кнопку на пине, обозначенном в дефайне buttonUp
GButton buttonC(buttonCentr);         // "создать" кнопку на пине, обозначенном в дефайне buttonCentr
GButton buttonD(buttonDown);          // "создать" кнопку на пине, обозначенном в дефайне buttonDown


bool flag_Light;                      // защелка для кнопки "Подсветка"
bool learning = false;                // флаг режима обучения магнитолы, на приемнике формируется длительная задержка после нажатия каждой кнопки, чтобы магнитола могла стабильно запоминать клавишу
bool enableBuzzer = true;             // флаг который указывает, что Буззер используется или не используется
bool done = false;                    // флаг, указывает на то, что что-то крутилось/нажималось, и нужно отправить в эфир новые данные

volatile bool state = false;

unsigned long timeToSleep;



void pciSetup(byte pin) {
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // Разрешаем PCINT для указанного пина
  PCIFR  |= bit (digitalPinToPCICRbit(pin)); // Очищаем признак запроса прерывания для соответствующей группы пинов
  PCICR  |= bit (digitalPinToPCICRbit(pin)); // Разрешаем PCINT для соответствующей группы пинов
}



ISR (PCINT2_vect) {                          // Обработчик запросов прерывания от пинов D0..D7, используется для выхода из сна при нажатии на кнопки или вращении энкодера
  cli();                                     // мы проснулись, запрещаем прерывания
}



ISR (PCINT0_vect) {                          // Обработчик запросов прерывания от пинов D8..D13, нам интересен пин D8 - подсветка, исли на пине изменения - выходим из сна для обработки события
  cli();
  state = true;                              // поднимаем флаг - состояние подсветки изменилось
}



void goingToSleep() {
  radio.powerDown();                        // деактивируем радиомодуль
  Serial.println(F("Going to sleep..."));
  Serial.flush();
  ADCSRA = 0;                               // отключаем АЦП, мы им не пользуемся, экономим энергию
  power_all_disable();                      // отключаем практически всю переферию МК
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);      // устанавливаем режим сна
  noInterrupts();                           // пока не готовы спать, запрещаем прерывания
  sleep_enable();                           // позволяем режим сна
  MCUCR = bit(BODS) | bit(BODSE);           // отключаем детектор пониженного напряжения
  MCUCR = bit(BODS);
  interrupts();                             // перед сном даем разрешения на прерывания сна
  sleep_cpu();                              // тут уводим МК в сон

  sleep_disable();                          // именно в эту строчку попадаем сразу после того как проснулись, соответственно запрещаем сон
  power_all_enable();                       // включаем переферию, которую выключили перед сном
  ADCSRA = 0;                               // и снова выключаем АЦП, так как он включился со всей переферией
  radio.powerUp();                          // активируем радиомодуль
}



void setup() {
  ADCSRA &= ~(1 << ADEN);                   // Отключаем АЦП, мы им не пользуемся

  Serial.begin(115200);
  EEPROM.begin();

  // ----------- Делаем пины входами и включаем внутреннюю подтяжку к плюсу питания, для всех входов кнопок
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonCentr, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);

  for (byte i = 15; i <= 19; i++) {         // объявляем все "лишние" выводы МК выходами, и "заземляем" их.
    pinMode(i, OUTPUT);                     // Исходя из этого http://heliosoph.mit-links.info/arduino-powered-by-capacitor-reducing-consumption/
    digitalWrite(i, LOW);                   // это уменьшает потребление энергии от батареи
  }

  // ----------- подвешиваем прерывания по изменению уровня, для вывода из сна, на пины кнопок и энкодера
  pciSetup(buttonUp);
  pciSetup(buttonCentr);
  pciSetup(buttonDown);
  pciSetup(CLK);
  pciSetup(DT);
  pciSetup(SW);
  pciSetup(light);

  if (EEPROM.read(100) == 127) {                // проверяем в ЕЕПРОМ будет ли использоваться буззер, если в ЕЕПРОМ значение 127 - буззер активен
    enableBuzzer = true;
  } else enableBuzzer = false;

  if (!digitalRead(buttonCentr)) enableBuzzer = !enableBuzzer;                 // если при включении зажата клавиша "Центр" переключаем режим буззера вкл\выкл
  EEPROM.update(100, (enableBuzzer ? 127 : 222));                              // и записываем состояние буззера в ЕЕПРОМ

  if (!digitalRead(buttonUp) and !digitalRead(buttonDown)) learning  = true;   // зажатие при включении клавиш "Вверх" и "Вниз активирует режим обучения магнитолы

  encoder.setType(TYPE1);                       // тип энкодера TYPE1 одношаговый, TYPE2 двухшаговый. Если энкодер работает странно, смените тип
  encoder.setPinMode(HIGH_PULL);                // тип подключения пинов энкодера, подтяжка HIGH_PULL (внутренняя) или LOW_PULL (внешняя на GND)
  encoder.setBtnPinMode(HIGH_PULL);             // тип подключения кнопки, подтяжка HIGH_PULL (внутренняя) или LOW_PULL (внешняя на GND)

  pinMode(light, INPUT);                        // вход "подсветка" если есть такой сигнал в руле
  pinMode(buzzer, OUTPUT);                      // устанавливаем пин Буззера как выход



  //----------- Настройки кнопки Up
  buttonU.setDebounce(debounce);                // настройка антидребезга (по умолчанию 80 мс)
  buttonU.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonU.setClickTimeout(clickTimeOut);        // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);                   // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // NORM_OPEN - нормально-разомкнутая кнопка

  //----------- Настройки кнопки Centr
  buttonC.setDebounce(debounce);                // настройка антидребезга (по умолчанию 80 мс)
  buttonC.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonC.setClickTimeout(clickTimeOut);        // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);                   // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // NORM_OPEN - нормально-разомкнутая кнопка

  //----------- Настройки кнопки Down
  buttonD.setDebounce(debounce);                // настройка антидребезга (по умолчанию 80 мс)
  buttonD.setTimeout(timeOut);                  // настройка таймаута на удержание (по умолчанию 500 мс)
  buttonD.setClickTimeout(clickTimeOut);        // настройка таймаута между кликами (по умолчанию 300 мс)
  buttonC.setType(HIGH_PULL);                   // HIGH_PULL - кнопка подключена к GND, пин подтянут к VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // NORM_OPEN - нормально-разомкнутая кнопка




  radio.begin();                          // активировать библиотеку радио-модуля
  delay(250);
  radio.setPALevel(RF24_PA_LOW);          // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);        // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS, должна быть одинакова на приёмнике и передатчике
  radio.setAutoAck(1);                    // режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(1, 15);                // (время между попыткой достучаться, число попыток)
  radio.enableAckPayload();               // разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);               // размер пакета, количество байт
  radio.setChannel(0x2f);                 // выбираем канал
  radio.openWritingPipe(addresses[0]);    // мы - труба 0, открываем канал для передачи данных
  radio.openReadingPipe(1, addresses[1]); // труба 1, резервировано для получения ответной информации с приемника
  radio.powerUp();                        // начать работу радиомодуля
  radio.stopListening();                  // не слушаем радиоэфир, мы передатчик
  goingToSleep();                         // отправляем контроллер спать
}







void loop() {
  uint8_t keyInt = 0;
  uint16_t toneVal;
  static byte txError;

  
  encoder.tick();                          // обязательная функция отработки энкодера. Должна постоянно опрашиваться

  // ----------------- обязательные функции отработки кнопок. Кнопки должны постоянно опрашиваться при каждом проходе цыкла loop
  buttonU.tick();
  buttonC.tick();
  buttonD.tick();

  if (state) {                             // состояние подсветки изменилось, обрабатываем
    keyInt = Rutina(11);
    state = false;
  }


  // --------------- Отрабатываем клики кнопок
  if (buttonU.isClick()) {
    keyInt = Rutina(1);               /* функция Rutina опрашивает вход подсветки клавиш (когда включены/выключены габариты), а также состояние
                                         передатчика "обучение магнитолы" и добавляет к коду клавиши число 100 и\или 30. Это укажит приемнику на то, что нужно
                                         подать напряжение на выход "подсветка" на стороне магнитолы, или на то, что передатчик в режиме "обучение". Все то же
                                         самое и для других кнопок или энкодера.*/
    Serial.println(F("Click on button UP"));
    toneVal = Tone1;
  }

  if (buttonC.isClick()) {
    keyInt = Rutina(2);
    Serial.println(F("Click on button CENTR"));
    toneVal = Tone1;
  }

  if (buttonD.isClick()) {
    keyInt = Rutina(3);
    Serial.println(F("Click on button DOWN"));
    toneVal = Tone1;
  }


  // --------------- Отрабатываем зажатие кнопок
  if (buttonU.isHolded()) {
    keyInt = Rutina(4);
    Serial.println(F("Pressed button UP"));
    toneVal = Tone2;
  }

  if (buttonC.isHolded()) {
    keyInt = Rutina(5);
    Serial.println(F("Pressed button CENTR"));
    toneVal = Tone2;
  }

  if (buttonD.isHolded()) {
    keyInt = Rutina(6);
    Serial.println(F("Pressed button DOWN"));
    toneVal = Tone2;
  }


  // ---------- Обрабатываем клик на кнопке энкодера
  if (encoder.isClick()) {
    Serial.println(F("Click on encoder switch"));
    keyInt = Rutina(7);
    toneVal = Tone1;
  }

  if (encoder.isHolded()) {                                    // если была нажата кнопка энкодера
    keyInt = Rutina(8);
    Serial.println(F("Encoder switch holded"));
    toneVal = Tone2;
  }

  if (encoder.isRightH()) {                                    // если был поворот вправо, зажатый энкодер, будем считать, что нажата кнопка энкодера, а поворот случаен
    keyInt = Rutina(7);
    Serial.println(F("-> Right Holded"));
    toneVal = Tone1;
  }
  if (encoder.isLeftH()) {                                     // если был поворот вправо, зажатый энкодер, будем считать, что нажата кнопка энкодера, а поворот случаен
    keyInt = Rutina(7);
    Serial.println(F("<- Left Holded"));
    toneVal = Tone1;
  }

  if (encoder.isRight()) {                                     // если был поворот энкодера вправо
    keyInt = Rutina(9);
    Serial.println(F("-> Right"));
    toneVal = Tone1;
  }
  if (encoder.isLeft()) {                                      // если был поворот энкодера влево
    keyInt = Rutina(10);
    Serial.println(F("<- Left"));
    toneVal = Tone1;
  }



  if (done) {                                                  // если что-то нажимали done == true, значит будем передавать в эфир
    if (enableBuzzer) tone(buzzer, toneVal, 100);              // если буззер активирован, - бибикнем, подтверждая нажатия\вращение энкодера
    Serial.println(keyInt);
    if (!radio.write(&keyInt, sizeof(keyInt))) {               // отправить код по радио, и если не удалось...
      Serial.println(F("Sending failed"));
      txError++;                                               // увеличиваем счетчик неудачных попыток отправки данных
      if (enableBuzzer or txError >= retryError) {             // и коротко бибикнем три раза
        tone(buzzer, 2500, 20);
        delay(50);
        tone(buzzer, 2250, 20);
        delay(50);
        tone(buzzer, 2000, 20);
      }
    } else txError = 0;
    Serial.println();
    done = false;                                             // в эфир отправили, збрасываем состояние флага done
    timeToSleep = millis();                   // обновляем таймер ухода ко сну, соответственно ждем 1000мс нажатия следующей клавиши или импульс энкодера
  }

  if (millis() - timeToSleep >= 1000) {                       // и если после последних действий ничего не происходило более 1000 миллисекунды то...
    goingToSleep();                                           // уходим спать.
    timeToSleep = millis();                                   // после того как проснулись по прерыванию, попадаем сюда и взводим таймер ко сну. Если ничего
                                                              // не произойдет (мы больше не нажимаем кнопки), МК снова уснет по истичению таймера. 
  }
}




int Rutina(byte key) {                                     // функция проверяет состояние входа "подсветка", а так же дополняет коды клавиш служебными кодами
  static uint8_t lightOn = 0;

  if (digitalRead(light) and !flag_Light) {                // читаем состояние подсветки
    Serial.println(F("Light is ON"));
    flag_Light = true;
    lightOn = 100;
  }
  if (!digitalRead(light) and flag_Light) {                // отрабатываем отключение подсветки
    Serial.println(F("Light is ON"));
    flag_Light = false;
    lightOn = 0;
  }
  
  done = true;                                             // данные готовы, поднимаем флаг для передачи в эфир
  
  return key + lightOn + (learning ? 30 : 0);              // возвращаем окончательное значение кода команды для передачи в эфир
}
