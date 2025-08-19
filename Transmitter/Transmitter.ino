#include <avr/sleep.h>
#include <avr/power.h>
#include <SPI.h>                      // ENG: Library for SPI bus communication / UKR: Бібліотека для роботи з шиною SPI
#include "RF24.h"                     // ENG: Radio module library / UKR: Бібліотека радіомодуля
#include "GyverButton.h"              // ENG: Button library, needed for hold-to-modify command functionality / UKR: Бібліотека для кнопок, потрібна для підтримки мутацій команд по затисканню
#include "GyverEncoder.h"             // ENG: Encoder library, the only one that supports a mouse encoder / UKR: Бібліотека для енкодера, єдина, що підтримує енкодер від мишки
#include <EEPROM.h>


RF24 radio(9, 10);                    // ENG: "Create" radio object on pins 9 and 10 for Uno/Nano / UKR: "Створити" радіо на пінах 9 та 10 для Uno/Nano
uint8_t addresses[][6] = {"1Node", "2Node"};          // ENG: Possible pipe addresses / UKR: Можливі номери труб

#define CLK 3                         // ENG: Encoder pin A / UKR: Пін А енкодера
#define DT 2                          // ENG: Encoder pin B / UKR: Пін Б енкодера
#define SW 7                          // ENG: Pin for the encoder button / UKR: Пін для кнопки енкодера
#define buzzer 14                     // ENG: Pin for the buzzer (a passive buzzer or small speaker is highly recommended) / UKR: Пін для буззера (дуже бажано використовувати пасивний буззер або маленький динамік)
#define buttonUp  5                   // ENG: Pin for the "Up" button / UKR: Пін для кнопки "Вгору"
#define buttonCentr 4                 // ENG: Pin for the "Center" button / UKR: Пін для кнопки "Центр"
#define buttonDown  6                 // ENG: Pin for the "Down" button / UKR: Пін для кнопки "Вниз"
#define light  8

#define debounce 50                   // ENG: Button debounce setting, default 50 ms / UKR: Налаштування антибрязкоту кнопок, за замовчуванням 50 мс
#define timeOut 300                   // ENG: Button hold timeout setting / UKR: Налаштування таймауту на утримання кнопки
#define clickTimeOut 200              // ENG: Timeout between clicks, multi-click is not actually used in the program / UKR: Налаштування таймауту між кліками, по факту багаторазові кліки в програмі не використовуються

#define Tone1 2000                    // ENG: Tone pitch for key clicks, in Hertz / UKR: Висота тону кліку по клавішах, в Герцах
#define Tone2 2500                    // ENG: Tone pitch for held keys, in Hertz / UKR: Висота тону затиснутих клавіш, в Герцах

const byte retryError = 5;            // ENG: Number of failed data transmission sessions before even a disabled buzzer starts beeping, indicating a receiver fault / UKR: Кількість невдалих сеансів передачі даних до того, як навіть деактивований буззер почне пищати, вказуючи на несправність приймача

Encoder encoder(CLK, DT, SW);         // ENG: "Create" encoder on pins defined by CLK, DT, SW / UKR: "Створити" енкодер на пінах, визначених у CLK, DT, SW
GButton buttonU(buttonUp);            // ENG: "Create" button on the pin defined in buttonUp / UKR: "Створити" кнопку на піні, визначеному в buttonUp
GButton buttonC(buttonCentr);         // ENG: "Create" button on the pin defined in buttonCentr / UKR: "Створити" кнопку на піні, визначеному в buttonCentr
GButton buttonD(buttonDown);          // ENG: "Create" button on the pin defined in buttonDown / UKR: "Створити" кнопку на піні, визначеному в buttonDown


bool flag_Light;                      // ENG: Latch for the "Backlight" button / UKR: Засувка для кнопки "Підсвітка"
bool learning = false;                // ENG: Flag for head unit learning mode; the receiver creates a long delay after each button press so the head unit can reliably save the key / UKR: Прапор режиму навчання магнітоли; на приймачі формується тривала затримка після натискання кожної кнопки, щоб магнітола могла стабільно запам'ятати клавішу
bool enableBuzzer = true;             // ENG: Flag indicating whether the Buzzer is used or not / UKR: Прапор, що вказує, чи використовується Буззер
bool done = false;                    // ENG: Flag indicating that something was rotated/pressed, and new data needs to be transmitted / UKR: Прапор, що вказує, що щось крутилося/натискалося, і потрібно відправити в ефір нові дані

volatile bool state = false;

unsigned long timeToSleep;



void pciSetup(byte pin) {
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // ENG: Enable PCINT for the specified pin / UKR: Дозволяємо PCINT для вказаного піна
  PCIFR  |= bit (digitalPinToPCICRbit(pin)); // ENG: Clear the interrupt request flag for the corresponding pin group / UKR: Очищаємо прапор запиту переривання для відповідної групи пінів
  PCICR  |= bit (digitalPinToPCICRbit(pin)); // ENG: Enable PCINT for the corresponding pin group / UKR: Дозволяємо PCINT для відповідної групи пінів
}



ISR (PCINT2_vect) {                          // ENG: Interrupt request handler for pins D0..D7, used to wake from sleep on button press or encoder rotation / UKR: Обробник запитів переривань від пінів D0..D7, використовується для виходу зі сну при натисканні кнопок або обертанні енкодера
  cli();                                     // ENG: We woke up, disable interrupts / UKR: Ми прокинулися, забороняємо переривання
}



ISR (PCINT0_vect) {                          // ENG: Interrupt request handler for pins D8..D13; we're interested in pin D8 - backlight. If the pin changes, wake up to handle the event / UKR: Обробник запитів переривань від пінів D8..D13; нас цікавить пін D8 - підсвітка. Якщо на піні є зміни - виходимо зі сну для обробки події
  cli();
  state = true;                              // ENG: Raise the flag - backlight state has changed / UKR: Піднімаємо прапор - стан підсвітки змінився
}



void goingToSleep() {
  radio.powerDown();                        // ENG: Deactivate the radio module / UKR: Деактивуємо радіомодуль
  Serial.println(F("Going to sleep..."));
  Serial.flush();
  ADCSRA = 0;                               // ENG: Disable the ADC, we don't use it, saving power / UKR: Відключаємо АЦП, ми ним не користуємося, економимо енергію
  power_all_disable();                      // ENG: Disable almost all MCU peripherals / UKR: Відключаємо практично всю периферію МК
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);      // ENG: Set the sleep mode / UKR: Встановлюємо режим сну
  noInterrupts();                           // ENG: Disable interrupts until we are ready to sleep / UKR: Поки не готові спати, забороняємо переривання
  sleep_enable();                           // ENG: Enable sleep mode / UKR: Дозволяємо режим сну
  MCUCR = bit(BODS) | bit(BODSE);           // ENG: Disable the brown-out detector / UKR: Відключаємо детектор зниженої напруги
  MCUCR = bit(BODS);
  interrupts();                             // ENG: Allow interrupts just before sleeping / UKR: Перед сном дозволяємо переривання
  sleep_cpu();                              // ENG: Here we put the MCU to sleep / UKR: Тут відправляємо МК в сон

  sleep_disable();                          // ENG: We land on this line right after waking up, so we disable sleep / UKR: Саме на цей рядок потрапляємо одразу після пробудження, відповідно, забороняємо сон
  power_all_enable();                       // ENG: Enable the peripherals we turned off before sleeping / UKR: Вмикаємо периферію, яку вимкнули перед сном
  ADCSRA = 0;                               // ENG: And disable the ADC again, as it was re-enabled with all peripherals / UKR: І знову вимикаємо АЦП, оскільки він увімкнувся з усією периферією
  radio.powerUp();                          // ENG: Activate the radio module / UKR: Активуємо радіомодуль
}



void setup() {
  ADCSRA &= ~(1 << ADEN);                   // ENG: Disable the ADC, we are not using it / UKR: Відключаємо АЦП, ми ним не користуємося

  Serial.begin(115220);
  EEPROM.begin();

  // ----------- Set pins as inputs and enable internal pull-up resistors for all button inputs
  // UKR: Робимо піни входами і вмикаємо внутрішню підтяжку до плюса живлення для всіх входів кнопок
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonCentr, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);

  for (byte i = 15; i <= 19; i++) {         // ENG: Declare all "extra" MCU pins as outputs and ground them. / UKR: Оголошуємо всі "зайві" виводи МК виходами і "заземлюємо" їх.
    pinMode(i, OUTPUT);                     // ENG: According to http://heliosoph.mit-links.info/arduino-powered-by-capacitor-reducing-consumption/ / UKR: Виходячи з цього http://heliosoph.mit-links.info/arduino-powered-by-capacitor-reducing-consumption/
    digitalWrite(i, LOW);                   // ENG: this reduces battery consumption. / UKR: це зменшує споживання енергії від батареї.
  }

  // ----------- Attach pin change interrupts to wake from sleep for button and encoder pins
  // UKR: Підвішуємо переривання по зміні рівня для виводу зі сну на піни кнопок та енкодера
  pciSetup(buttonUp);
  pciSetup(buttonCentr);
  pciSetup(buttonDown);
  pciSetup(CLK);
  pciSetup(DT);
  pciSetup(SW);
  pciSetup(light);

  if (EEPROM.read(100) == 127) {                // ENG: Check EEPROM to see if the buzzer is enabled; if the value is 127, the buzzer is active / UKR: Перевіряємо в ЕЕПРОМ, чи буде використовуватися буззер; якщо в ЕЕПРОМ значення 127 - буззер активний
    enableBuzzer = true;
  } else enableBuzzer = false;

  if (!digitalRead(buttonCentr)) enableBuzzer = !enableBuzzer;                 // ENG: If the "Center" button is held on startup, toggle the buzzer on/off mode / UKR: Якщо при включенні затиснута клавіша "Центр", перемикаємо режим буззера вкл/викл
  EEPROM.update(100, (enableBuzzer ? 127 : 222));                              // ENG: And write the buzzer state to EEPROM / UKR: І записуємо стан буззера в ЕЕПРОМ

  if (!digitalRead(buttonUp) and !digitalRead(buttonDown)) learning  = true;   // ENG: Holding "Up" and "Down" on startup activates the head unit learning mode / UKR: Затискання при включенні клавіш "Вгору" і "Вниз" активує режим навчання магнітоли

  encoder.setType(TYPE1);                       // ENG: Encoder type: TYPE1 for single-step, TYPE2 for two-step. If the encoder behaves strangely, change the type / UKR: Тип енкодера: TYPE1 однокроковий, TYPE2 двокроковий. Якщо енкодер працює дивно, змініть тип
  encoder.setPinMode(HIGH_PULL);                // ENG: Encoder pin connection type, HIGH_PULL (internal) or LOW_PULL (external to GND) / UKR: Тип підключення пінів енкодера, підтяжка HIGH_PULL (внутрішня) або LOW_PULL (зовнішня на GND)
  encoder.setBtnPinMode(HIGH_PULL);             // ENG: Button connection type, HIGH_PULL (internal) or LOW_PULL (external to GND) / UKR: Тип підключення кнопки, підтяжка HIGH_PULL (внутрішня) або LOW_PULL (зовнішня на GND)

  pinMode(light, INPUT);                        // ENG: "Backlight" input if such a signal is available in the steering wheel / UKR: Вхід "підсвітка", якщо є такий сигнал в кермі
  pinMode(buzzer, OUTPUT);                      // ENG: Set the Buzzer pin as an output / UKR: Встановлюємо пін Буззера як вихід



  //----------- Up button settings
  buttonU.setDebounce(debounce);                // ENG: Debounce setting (default 80 ms) / UKR: Налаштування антибрязкоту (за замовчуванням 80 мс)
  buttonU.setTimeout(timeOut);                  // ENG: Hold timeout setting (default 500 ms) / UKR: Налаштування таймауту на утримання (за замовчуванням 500 мс)
  buttonU.setClickTimeout(clickTimeOut);        // ENG: Timeout between clicks (default 300 ms) / UKR: Налаштування таймауту між кліками (за замовчуванням 300 мс)
  buttonC.setType(HIGH_PULL);                   // ENG: HIGH_PULL - button connected to GND, pin pulled up to VCC (PIN --- BUTTON --- GND) / UKR: HIGH_PULL - кнопка підключена до GND, пін підтягнутий до VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // ENG: NORM_OPEN - normally open button / UKR: NORM_OPEN - нормально-розімкнута кнопка

  //----------- Center button settings
  buttonC.setDebounce(debounce);                // ENG: Debounce setting (default 80 ms) / UKR: Налаштування антибрязкоту (за замовчуванням 80 мс)
  buttonC.setTimeout(timeOut);                  // ENG: Hold timeout setting (default 500 ms) / UKR: Налаштування таймауту на утримання (за замовчуванням 500 мс)
  buttonC.setClickTimeout(clickTimeOut);        // ENG: Timeout between clicks (default 300 ms) / UKR: Налаштування таймауту між кліками (за замовчуванням 300 мс)
  buttonC.setType(HIGH_PULL);                   // ENG: HIGH_PULL - button connected to GND, pin pulled up to VCC (PIN --- BUTTON --- GND) / UKR: HIGH_PULL - кнопка підключена до GND, пін підтягнутий до VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // ENG: NORM_OPEN - normally open button / UKR: NORM_OPEN - нормально-розімкнута кнопка

  //----------- Down button settings
  buttonD.setDebounce(debounce);                // ENG: Debounce setting (default 80 ms) / UKR: Налаштування антибрязкоту (за замовчуванням 80 мс)
  buttonD.setTimeout(timeOut);                  // ENG: Hold timeout setting (default 500 ms) / UKR: Налаштування таймауту на утримання (за замовчуванням 500 мс)
  buttonD.setClickTimeout(clickTimeOut);        // ENG: Timeout between clicks (default 300 ms) / UKR: Налаштування таймауту між кліками (за замовчуванням 300 мс)
  buttonC.setType(HIGH_PULL);                   // ENG: HIGH_PULL - button connected to GND, pin pulled up to VCC (PIN --- BUTTON --- GND) / UKR: HIGH_PULL - кнопка підключена до GND, пін підтягнутий до VCC (PIN --- КНОПКА --- GND)
  buttonC.setDirection(NORM_OPEN);              // ENG: NORM_OPEN - normally open button / UKR: NORM_OPEN - нормально-розімкнута кнопка




  radio.begin();                          // ENG: Activate the radio module library / UKR: Активувати бібліотеку радіо-модуля
  delay(250);
  radio.setPALevel(RF24_PA_LOW);          // ENG: Transmitter power level. Options: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX / UKR: Рівень потужності передавача. Варіанти: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate(RF24_250KBPS);        // ENG: Data rate. Options: RF24_2MBPS, RF24_1MBPS, RF24_250KBPS, must be the same on receiver and transmitter / UKR: Швидкість обміну. Варіанти: RF24_2MBPS, RF24_1MBPS, RF24_250KBPS, має бути однаковою на приймачі та передавачі
  radio.setAutoAck(1);                    // ENG: Auto-acknowledgment mode, 1 for on, 0 for off / UKR: Режим підтвердження прийому, 1 - увімк., 0 - вимк.
  radio.setRetries(1, 15);                // ENG: (delay between retries, number of retries) / UKR: (час між спробами достукатися, кількість спроб)
  radio.enableAckPayload();               // ENG: Allow sending data back in response to an incoming signal / UKR: Дозволити відправку даних у відповідь на вхідний сигнал
  radio.setPayloadSize(32);               // ENG: Packet size, number of bytes / UKR: Розмір пакета, кількість байт
  radio.setChannel(0x2f);                 // ENG: Select a channel / UKR: Вибираємо канал
  radio.openWritingPipe(addresses[0]);    // ENG: We are pipe 0, open a channel for data transmission / UKR: Ми - труба 0, відкриваємо канал для передачі даних
  radio.openReadingPipe(1, addresses[1]); // ENG: Pipe 1, reserved for receiving response information from the receiver / UKR: Труба 1, зарезервовано для отримання відповідної інформації з приймача
  radio.powerUp();                        // ENG: Start the radio module / UKR: Почати роботу радіомодуля
  radio.stopListening();                  // ENG: Don't listen to the airwaves, we are a transmitter / UKR: Не слухаємо радіоефір, ми передавач
  goingToSleep();                         // ENG: Send the controller to sleep / UKR: Відправляємо контролер спати
}







void loop() {
  uint8_t keyInt = 0;
  uint16_t toneVal;
  static byte txError;

  
  encoder.tick();                          // ENG: Mandatory encoder processing function. Must be polled continuously / UKR: Обов'язкова функція обробки енкодера. Має постійно опитуватися

  // ----------------- Mandatory button processing functions. Buttons must be polled on every loop cycle
  // UKR: Обов'язкові функції обробки кнопок. Кнопки мають постійно опитуватися при кожному проході циклу loop
  buttonU.tick();
  buttonC.tick();
  buttonD.tick();

  if (state) {                             // ENG: Backlight state has changed, process it / UKR: Стан підсвітки змінився, обробляємо
    keyInt = Rutina(11);
    state = false;
  }


  // --------------- Process button clicks
  if (buttonU.isClick()) {
    keyInt = Rutina(1);               /* ENG: The Rutina function polls the key backlight input (when parking lights are on/off) and the transmitter's "learning mode" state, adding 100 and/or 30 to the key code. This tells the receiver to apply voltage to the "backlight" output on the head unit side, or that the transmitter is in "learning" mode. The same applies to other buttons or the encoder.
                                         UKR: Функція Rutina опитує вхід підсвітки клавіш (коли увімкнені/вимкнені габарити), а також стан передавача "навчання магнітоли" і додає до коду клавіші число 100 та/або 30. Це вкаже приймачу на те, що потрібно подати напругу на вихід "підсвітка" на стороні магнітоли, або на те, що передавач у режимі "навчання". Те саме і для інших кнопок або енкодера.*/
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


  // --------------- Process button holds
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


  // ---------- Process encoder button click
  if (encoder.isClick()) {
    Serial.println(F("Click on encoder switch"));
    keyInt = Rutina(7);
    toneVal = Tone1;
  }

  if (encoder.isHolded()) {                                    // ENG: If the encoder button was held / UKR: Якщо була затиснута кнопка енкодера
    keyInt = Rutina(8);
    Serial.println(F("Encoder switch holded"));
    toneVal = Tone2;
  }

  if (encoder.isRightH()) {                                    // ENG: If turned right while held, we'll assume the button was pressed and the turn was accidental / UKR: Якщо був поворот вправо із затиснутим енкодером, будемо вважати, що натиснута кнопка енкодера, а поворот випадковий
    keyInt = Rutina(7);
    Serial.println(F("-> Right Holded"));
    toneVal = Tone1;
  }
  if (encoder.isLeftH()) {                                     // ENG: If turned left while held, we'll assume the button was pressed and the turn was accidental / UKR: Якщо був поворот вліво із затиснутим енкодером, будемо вважати, що натиснута кнопка енкодера, а поворот випадковий
    keyInt = Rutina(7);
    Serial.println(F("<- Left Holded"));
    toneVal = Tone1;
  }

  if (encoder.isRight()) {                                     // ENG: If the encoder was turned right / UKR: Якщо був поворот енкодера вправо
    keyInt = Rutina(9);
    Serial.println(F("-> Right"));
    toneVal = Tone1;
  }
  if (encoder.isLeft()) {                                      // ENG: If the encoder was turned left / UKR: Якщо був поворот енкодера вліво
    keyInt = Rutina(10);
    Serial.println(F("<- Left"));
    toneVal = Tone1;
  }



  if (done) {                                                  // ENG: If something was pressed, done == true, so we will transmit / UKR: Якщо щось натискали, done == true, значить будемо передавати в ефір
    if (enableBuzzer) tone(buzzer, toneVal, 100);              // ENG: If the buzzer is active, beep to confirm the press/rotation / UKR: Якщо буззер активований, - бібікнемо, підтверджуючи натискання/обертання енкодера
    Serial.println(keyInt);
    if (!radio.write(&keyInt, sizeof(keyInt))) {               // ENG: Send the code via radio, and if it fails... / UKR: Відправити код по радіо, і якщо не вдалося...
      Serial.println(F("Sending failed"));
      txError++;                                               // ENG: Increment the failed send attempts counter / UKR: Збільшуємо лічильник невдалих спроб відправки даних
      if (enableBuzzer or txError >= retryError) {             // ENG: and beep three short times / UKR: і коротко бібікнемо три рази
        tone(buzzer, 2500, 20);
        delay(50);
        tone(buzzer, 2250, 20);
        delay(50);
        tone(buzzer, 2000, 20);
      }
    } else txError = 0;
    Serial.println();
    done = false;                                             // ENG: Transmitted, so reset the done flag / UKR: В ефір відправили, скидаємо стан прапора done
    timeToSleep = millis();                   // ENG: Update the go-to-sleep timer, waiting 1000ms for the next key press or encoder pulse / UKR: Оновлюємо таймер відходу до сну, відповідно чекаємо 1000мс на натискання наступної клавіші або імпульс енкодера
  }

  if (millis() - timeToSleep >= 1000) {                       // ENG: And if nothing has happened for more than 1000 milliseconds since the last action, then... / UKR: І якщо після останніх дій нічого не відбувалося більше 1000 мілісекунд, то...
    goingToSleep();                                           // ENG: we go to sleep. / UKR: йдемо спати.
    timeToSleep = millis();                                   // ENG: After waking up from an interrupt, we land here and reset the sleep timer. If nothing else happens (we don't press more buttons), the MCU will sleep again when the timer expires. / UKR: Після пробудження по перериванню, потрапляємо сюди і зводимо таймер до сну. Якщо нічого не відбудеться (ми більше не натискаємо кнопки), МК знову засне по закінченню таймера.
  }
}




int Rutina(byte key) {                                     // ENG: This function checks the "backlight" input state and supplements key codes with service codes / UKR: Ця функція перевіряє стан входу "підсвітка", а також доповнює коди клавіш службовими кодами
  static uint8_t lightOn = 0;

  if (digitalRead(light) and !flag_Light) {                // ENG: Read the backlight state / UKR: Читаємо стан підсвітки
    Serial.println(F("Light is ON"));
    flag_Light = true;
    lightOn = 100;
  }
  if (!digitalRead(light) and flag_Light) {                // ENG: Handle backlight turning off / UKR: Обробляємо відключення підсвітки
    Serial.println(F("Light is ON"));
    flag_Light = false;
    lightOn = 0;
  }
  
  done = true;                                             // ENG: Data is ready, raise the flag for transmission / UKR: Дані готові, піднімаємо прапор для передачі в ефір
  
  return key + lightOn + (learning ? 30 : 0);              // ENG: Return the final command code value for transmission / UKR: Повертаємо остаточне значення коду команди для передачі в ефір
}
