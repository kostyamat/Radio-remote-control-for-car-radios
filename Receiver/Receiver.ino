#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include "RF24.h"
#include <printf.h>


#define ONEWIRE_PIN 18                         // ENG: Pin A4 for the DS18B20 sensor / UKR: Пін A4 для датчика DS18B20
#define LIGHT_OUT_PIN  8                       // ENG: Pin D6 for connecting to the head unit's illumination input, not needed if connected via car wiring / UKR: Пін D6 для підключення до входу підсвітки магнітоли, не потрібен, якщо підключено від проводки авто
#define FAN_OUT_PIN 19                         // ENG: Pin A5 for cooling fan control / UKR: Пін А5 для керування вентилятором охолодження
#define FAN_ON true                            /* ENG: Set to false to invert the fan control output (for PNP transistor), leave true for NPN transistor.
                                                  UKR: Встановити `false`, якщо потрібно інвертувати вихід керування вентилятором (для PNP-транзистора), залишити `true` для NPN-транзистора. */
#define LIGHT_ON true                          /* ENG: Set to false to invert the illumination output (for PNP transistor), leave true for direct connection or NPN transistor.
                                                  UKR: Встановити `false`, якщо потрібно інвертувати вихід підсвітки (для PNP-транзистора), залишити `true` для прямого підключення або NPN-транзистора. */
const byte hold = 100;                         /* ENG: Encoder pulse duration. It's advisable to find the minimum possible value that the KEY1/KEY2 analog input of the head unit will clearly recognize.
                                                  UKR: Тривалість імпульсів енкодера. Бажано підібрати мінімально можливе значення, яке буде чітко розпізнавати аналоговий вхід KEY1/KEY2 магнітоли. */
const uint16_t keyHold = 500;                  /* ENG: Button "press" duration. Since the transmitter doesn't send a release code, it's advisable to find the minimum value the head unit will recognize.
                                                  UKR: Тривалість "натискання" кнопок. Оскільки передавач не надсилає код відпускання, бажано підібрати мінімально можливе значення. */
const int8_t maxTemp = 45;                     /* ENG: Temperature at which the fan turns on. Don't set it too high, as the heatsink temperature is always lower than the actual temp on the amplifier and processor.
                                                  UKR: Температура, при якій вмикається вентилятор. Не варто встановлювати занадто високу, оскільки температура радіатора завжди нижча за реальну на підсилювачі та процесорі. */
const int8_t destTemp = 30;                    /* ENG: The target temperature. Don't set it too low; a fan is not a refrigerator. Be realistic, you're unlikely to cool below the cabin temp + 10°C. You risk making the fan run constantly.
                                                  UKR: Цільова температура. Не варто занижувати, вентилятор — не холодильник. Навряд чи вдасться опустити температуру нижче, ніж у салоні +10°C. Ви ризикуєте змусити вентилятор працювати постійно. */
const uint8_t tempSensPeriod = 30;             /* ENG: Temperature measurement period in seconds. Max 255, but shouldn't be less than 5 to avoid self-heating the sensor (real period will be 1 sec longer).
                                                  UKR: Період вимірювання температури в секундах. Максимум 255, але не варто ставити менше 5, щоб уникнути саморозігріву датчика (реальний період буде на 1 сек довшим). */


bool learning = false;
bool done = false;
bool keyDone = false;


OneWire oneWire(ONEWIRE_PIN);

DallasTemperature tempSensor(&oneWire);

RF24 radio(9, 10);                             // ENG: Radio on pins 9 and 10 / UKR: Радіо на пінах 9 та 10

byte addresses[][6] = {"1Node", "2Node"};      // ENG: Possible pipe addresses / UKR: Можливі адреси каналів (труб)
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
  pinMode(FAN_OUT_PIN, OUTPUT);                 // ENG: Set pin modes and initial states for fan and illumination outputs / UKR: Встановлюємо режими пінів та їх початковий стан для вентилятора і підсвітки
  digitalWrite(FAN_OUT_PIN, !FAN_ON);
  pinMode(LIGHT_OUT_PIN, OUTPUT);
  digitalWrite(LIGHT_OUT_PIN, !LIGHT_ON);

  for (byte i = 2; i <= 7; i++) {               // ENG: Set resistor pins to the required state (outputs "pulled" to ground) / UKR: Встановлюємо піни з резисторами в потрібний нам стан виходів з "підтяжкою до землі"
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  for (byte i = 14; i <= 17; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  resetPins();                        /* ENG: This function sets all resistor pins to INPUT mode, providing a high impedance relative to ground. This "trick" of switching pin modes gives us the needed functionality.
                                         UKR: Ця функція встановлює всі піни з резисторами в стан входів, що забезпечує нескінченно високий імпеданс відносно "землі". Така "гра" зі зміною статусу виводу "вхід <-> вихід" забезпечує нам потрібний функціонал. */

  Serial.begin(115200);
  printf_begin();
  tempSensor.begin();                            // ENG: Initialize the temperature sensor library / UKR: Запуск бібліотеки температурного датчика

  radio.begin();                                 // ENG: Initialize the radio module library / UKR: Запуск бібліотеки радіомодуля
  delay(250);
  radio.setPALevel(RF24_PA_LOW);                 // ENG: Transmitter power level. Options: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX / UKR: Рівень потужності передавача. Варіанти: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);               // ENG: Data rate. Options: RF24_2MBPS, RF24_1MBPS, RF24_250KBPS / UKR: Швидкість обміну. Варіанти: RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  radio.setAutoAck(1);                           // ENG: Auto-acknowledgment mode, 1 for on, 0 for off / UKR: Режим підтвердження прийому, 1 - увімк., 0 - вимк.
  radio.setRetries(1, 15);                       // ENG: (delay between retries, number of retries) / UKR: (час між спробами достукатися, кількість спроб)
  radio.enableAckPayload();                      // ENG: Allow sending data back in response to an incoming signal / UKR: Дозволити відправку даних у відповідь на вхідний сигнал
  radio.setPayloadSize(32);                      // ENG: Packet size in bytes / UKR: Розмір пакета, в байтах
  radio.setChannel(0x2f);                        // ENG: Select a channel (choose one with no noise!) / UKR: Вибираємо канал (в якому немає шумів!)
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1, addresses[0]);        // ENG: We want to listen to pipe 0 / UKR: Хочемо слухати трубу 0
  radio.powerUp();                               // ENG: Begin operation / UKR: Почати роботу
  radio.startListening();                        // ENG: Start listening to the airwaves, we are the receiver module / UKR: Починаємо слухати ефір, ми - приймальний модуль
  radio.printDetails();                          // ENG: Print radio module system info at startup / UKR: Друкуємо системну інформацію радіомодуля при старті
  
  tempSensTime = millis();                       // ENG: Start counting the period for temperature sensor reading / UKR: Починаємо відлік періоду читання датчика температури
}




void loop() {
  byte inData = 20;
  byte resistorPin = 0;
  int Temp;

  if (millis() - tempSensTime >= tempSensPeriod * 1000) {                         // ENG: After the programmed period, send a request to the sensor... / UKR: Після запрограмованого періоду надсилаємо запит датчику...
    tempSensor.requestTemperatures();                                             // ENG: ...to prepare the data / UKR: ...на підготовку даних
  }
  if (millis() - tempSensTime >= (tempSensPeriod * 1000) + 1000) {                // ENG: And now, 1000ms after the request, we read the data from the sensor / UKR: А тепер, через 1000мс після запиту, зчитуємо дані з датчика
    Temp = tempSensor.getTempCByIndex(0);
    Serial.print(F("Temperature: "));
    Serial.println(Temp);
    tempSensTime = millis();

    if (Temp >= maxTemp and digitalRead(FAN_OUT_PIN) == !FAN_ON) {                         // ENG: If temperature exceeds the upper threshold, turn on the fan / UKR: Якщо температура перевищила верхній поріг, вмикаємо вентилятор
      Serial.println(F("The cooler was turned ON"));
      digitalWrite(FAN_OUT_PIN, FAN_ON);
    }
    if (Temp <= destTemp and digitalRead(FAN_OUT_PIN) == FAN_ON) {                         // ENG: If the target temperature is reached, turn off the fan / UKR: Якщо температура досягла потрібного значення, вимикаємо вентилятор
      Serial.println(F("The cooler was turned OFF"));
      digitalWrite(FAN_OUT_PIN, !FAN_ON);
    }
  }

  while (radio.available()) {                   // ENG: Listen to the airwaves / UKR: Слухаємо ефір
    radio.read(&inData, sizeof(inData));        // ENG: Read the incoming signal / UKR: Читаємо вхідний сигнал
    Serial.print(F("Recived Code "));
    Serial.println(inData);
  }

  if (inData > 100) {                          // ENG: Check if the transmitter is sending the "illumination on" code (button code + 100) / UKR: Перевіряємо, чи передавач надсилає код "підсвітка увімкнена" (до коду кнопки додається 100)
    inData = inData - 100;                     // ENG: Clean the command code from the service code 100 / UKR: Очищаємо код команди від службового коду 100
    digitalWrite(LIGHT_OUT_PIN, LIGHT_ON);                                             // ENG: If code is 100 + button code, turn on the head unit's illumination / UKR: Якщо код 100 + код кнопки, вмикаємо підсвітку на магнітолі
  } else if (inData != 20) digitalWrite(LIGHT_OUT_PIN, !LIGHT_ON);          // ENG: If 100 is absent from the received code, turn off illumination / UKR: Якщо 100 відсутній у прийнятому коді, то вимикаємо підсвітку


  if (inData > 30) {                                         // ENG: Check if code 30 + key code is received; if so, we are in "learning mode" / UKR: Перевіряємо, чи не отримано код 30 + код клавіші; якщо так, - ми в режимі "навчання магнітоли"
    learning = true;
    inData = inData - 30;                                    // ENG: Clean the command code from the service code 30 / UKR: Очищаємо код команди від службового коду 30
  }
  if (inData == 9 or inData == 10) {                         // ENG: Check if the code is from the encoder; if so, set the "release timer" for the encoder / UKR: Перевіряємо, чи не від енкодера прийшов код; якщо так, то зводимо "таймер відпускання" для енкодера
    loopTime = millis();
    done = true;
  }
  if (inData >= 1 and inData <= 8) {                // ENG: Check if the code is from a button; if so, set the "release timer" for buttons / UKR: Перевіряємо, чи не від кнопок прийшов код; якщо так, то зводимо "таймер відпускання" для кнопок
    loopTime = millis();
    keyDone = true;
  }

  switch (inData) {                            // ENG: Set the resistor pin number according to the received code / UKR: Виконуємо встановлення номера резисторного піна відповідно до отриманого коду
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
    default:                                              // ENG: Disable all resistors if the key code is unknown / UKR: Відключаємо всі резистори, якщо код клавіші невідомий
      if (inData != 20) {
        if (!learning) {
          resetPins();
        }
        if (inData != 0) Serial.println(F("Unknown Code")), Serial.println();
      }
      break;
  }

  if (inData >= 1 and inData <= 10) {                          // ENG: If the command code is valid, reset all resistor pins and activate the correct one / UKR: Якщо код команди вірний, скидаємо всі піни з резисторами та активуємо потрібний
    resetPins();

    pinMode(resistorPin, OUTPUT);
    digitalWrite(resistorPin, LOW);

    Serial.print(F("Controller PIN "));
    Serial.print(resistorPin);
    Serial.println();
  }

  if (done and (millis() - loopTime >= hold + (learning ? 2000 : 0))) {                  // ENG: Handle the encoder pulse timeout (the "release timer" set earlier) / UKR: Обробляємо таймаут імпульсів енкодера ("таймер відпускання", який звели вище)
    Serial.println(F("Encoder timeout"));
    resetPins();
    done = false;
  }

  if (keyDone and (millis() - loopTime >= keyHold + (learning ? 2000 : 0))) {            // ENG: Handle the button pulse timeout (the "release timer" set earlier) / UKR: Обробляємо таймаут імпульсу кнопок ("таймер відпускання", який звели вище)
    Serial.println(F("Key timeout"));
    resetPins();
    keyDone = false;
  }
}
