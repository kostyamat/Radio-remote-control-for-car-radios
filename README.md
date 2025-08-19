# Wireless Steering Wheel Control Extender for Car Radios

## 🇺🇦 [Ukrainian Version](#ukrainian-version)

[![Watch the video](https://img.youtube.com/vi/470r13HLdKc/maxresdefault.jpg)](https://youtu.be/470r13HLdKc)

A wireless extender project for steering wheel buttons, designed for car head units that support resistive button recognition (KEY1, KEY2 interfaces).

---

## Overview

The system consists of two devices: a **Transmitter** and a **Receiver**. Both devices are built on ready-made boards similar to Arduino Nano (Atmega328/Atmega168 or their clone LGT8F328P) and use NRF24L01 radio modules for communication.

### Key Features
* **10 independent commands:** 8 commands from 4 buttons (short/long press) + 2 commands from the encoder.
* **Volume control:** Uses a rotary encoder (e.g., from an old computer mouse).
* **"Head Unit Learning" Mode:** A special mode for easy button setup in the head unit's menu.
* **Audible feedback:** A built-in buzzer in the transmitter to indicate actions.
* **Thermostat function:** The receiver can measure temperature using a DS18B20 sensor and control a cooling fan for the head unit.
* **Backlight control:** Ability to synchronize the button backlight with the car's parking lights.
* **Energy efficiency:** The transmitter uses `PowerDown` sleep mode to minimize power consumption, allowing for a battery-powered implementation.

---

## Schematic
The general schematic for the transmitter and receiver.

![Schematic TX RX buttons](https://github.com/kostyamat/Radio-remote-control-for-car-radios/blob/master/Schematic_TX_RX-buttons.png?raw=true)

---

## Detailed Component Description

### 🔹 Transmitter

The transmitter uses three tactile buttons, an encoder button, and the encoder itself.

* **Controls:**
    * **Short press (< 300ms):** Sends the primary command.
    * **Long press (> 300ms):** Sends the alternative command. Thus, 4 buttons can generate 8 different commands.
    * **Encoder rotation:** Generates `Volume+` and `Volume-` commands. Simultaneous pressing and rotating of the encoder is ignored to prevent accidental commands (pressing, e.g., for `Mute`, has priority).

* **Logic:**
    The transmitter instantly sends a command packet upon any action (press, rotation) and does not wait for the button to be "released." This ensures a high response speed. All logic for command duration is handled by the receiver.

#### Special Transmitter Modes

* **Audible Feedback Control (Buzzer):**
    To enable or disable the buzzer, follow these steps:
    1.  Power off the transmitter.
    2.  Press and hold the **"Center" (SW3)** button.
    3.  Power on the transmitter.
    The buzzer state (on/off) will be saved to EEPROM.

* **"Head Unit Learning" Mode:**
    This mode increases the signal duration for the head unit, allowing it to reliably recognize and save the resistance value for each button.
    1.  Power off the transmitter.
    2.  Press and hold the **"Up" (SW2)** and **"Down" (SW4)** buttons simultaneously.
    3.  Power on the transmitter.
    The receiver will enter learning mode. To exit, simply power cycle the transmitter or press its `RESET` button.

### 🔹 Receiver

The receiver gets commands from the transmitter, identifies them, and connects the corresponding resistors to the output (KEY1 or KEY2).

* **Logic:**
    * On a command from a button, the resistor is connected for **500 ms** (`keyHold`).
    * On a command from the encoder, it is connected for **100 ms** (`hold`).
    * In **"Learning" mode**, the duration is increased to **3000 ms** for buttons and **2600 ms** for the encoder.
    These values can be changed in the receiver's sketch.

* **Feedback:**
    After receiving a command, the receiver sends an acknowledgment to the transmitter. If the transmitter does not receive it, it will retry 15 times. If all attempts fail, the buzzer (if enabled) will beep three times.

* **Thermostat Function:**
    The receiver can control a cooling fan for the head unit.
    * **Sensor:** DS18B20.
    * **Control Element:** Darlington transistor TIP120 (or any other suitable transistor/relay).
    * **Sketch settings:**
        * `const int8_t maxTemp = 45;` — fan turn-on temperature (default 45°C).
        * `const int8_t destTemp = 30;` — fan turn-off temperature (default 30°C).
        * `#define FAN_ON true` — control logic (`true` for high-side switching, `false` for low-side).
        * `const uint8_t tempSensPeriod = 30;` — sensor polling period in seconds. It is **not recommended** to set this below 5 seconds due to potential delays (the `OneWire` library uses `delay()`).

---

## Assembly and Setup Specifics

### Component Recommendations

* **Microcontrollers:** The project is compatible with Arduino Nano based on **Atmega328/168** and their Chinese clones **LGT8F328P**.
    ![LGT8F328P](https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/LGT8F328P.png?raw=true)
    *The plugin to support LGT8F328P in the Arduino IDE can be downloaded [here](https://github.com/dbuezas/lgt8fx).*

* **NRF24L01 Radio Modules:** Purchase modules from reputable sellers. Cheap counterfeits may not work correctly with sleep modes.

* **Encoder:** Encoders from computer mice can have different pinouts. The ground pin (C) is usually connected to the common ground on the mouse's PCB. The other two pins (A/B) are signal pins. If the encoder behaves strangely:
    1.  Check the pinout.
    2.  In the transmitter sketch, change `c1.setType(TYPE1)` to `TYPE2`.
    3.  Add ceramic capacitors (10-100 nF) between the signal pins (A/B) and ground.

* **Buzzer:** A passive piezo element is recommended. An active buzzer will work, but the sound may be distorted.

### Battery-Powered Transmitter Option

The transmitter can run on a battery (2.2-3.6V) by utilizing sleep mode. For this, it is recommended to:
1.  Use an **Arduino Pro Mini 3.3V 8MHz** board.
2.  Remove the voltage regulator and power LED from the board to reduce power consumption.
3.  Power the board and the NRF24L01 module directly from a lithium battery (e.g., CR2030).

**Approximate schematic:**
![Schematic battery TX](https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/Schematic_battery_TX.png?raw=true)

### ⚠️ Important Power Supply Notes

* **DO NOT** connect power from the car's electrical system (>12V) directly to the `VIN` pin of the Arduino. Use an external voltage regulator, such as an **LM7805**.
* **DO NOT** power the NRF24L01 module from the `3.3V` output of the Arduino. The module's current draw exceeds the capability of the onboard regulator. Use a separate **3.3V** regulator, such as an **AMS1117-3.3**.
* The best power source for the receiver is the remote amp turn-on wire from the head unit. The voltage on it appears after the head unit has fully booted and is typically around 9V, which reduces heat on the LM7805 regulator.

---

## Support the Author
If you found this project useful, you can show your appreciation:
[<img src="https://www.paypalobjects.com/digitalassets/c/website/logo/full-text/pp_fc_hl.svg" width="100">](https://www.paypal.me/kostyamat)

---
---

<a name="ukrainian-version"></a>
# Радіо-подовжувач кнопок керма для автомобільних магнітол

[![Демонстрація роботи](https://img.youtube.com/vi/470r13HLdKc/maxresdefault.jpg)](https://youtu.be/470r13HLdKc)

Проект радіо-подовжувача для кнопок на кермі, призначений для автомобільних магнітол, які підтримують розпізнавання резистивних кнопок (інтерфейси KEY1, KEY2).

---

## Загальний опис

Система складається з двох пристроїв: **передавача** та **приймача**. Обидва пристрої побудовані на базі готових плат, аналогічних Arduino Nano (Atmega328/Atmega168 або їх клон LGT8F328P), і використовують радіо-модулі NRF24L01 для зв'язку.

### Основні можливості
* **10 незалежних команд:** 8 команд з 4 кнопок (коротке/довге натискання) + 2 команди від енкодера.
* **Регулювання гучності:** Використовується ротативний енкодер (наприклад, від старої комп'ютерної миші).
* **Режим "Навчання магнітоли":** Спеціальний режим для зручного налаштування кнопок у меню магнітоли.
* **Звукове підтвердження:** Вбудований буззер у передавачі для індикації натискань.
* **Термостат:** Приймач може вимірювати температуру за допомогою датчика DS18B20 та керувати вентилятором охолодження магнітоли.
* **Керування підсвіткою:** Можливість синхронізації підсвітки кнопок з габаритними вогнями автомобіля.
* **Енергоефективність:** Передавач використовує режим сну `PowerDown` для мінімізації споживання енергії, що дозволяє реалізувати живлення від батареї.

---

## Схема
Загальна принципова схема передавача та приймача.

![Schematic TX RX buttons](https://github.com/kostyamat/Radio-remote-control-for-car-radios/blob/master/Schematic_TX_RX-buttons.png?raw=true)

---

## Детальний опис компонентів

### 🔹 Передавач (Transmitter)

Передавач використовує три тактові кнопки, кнопку енкодера та сам енкодер.

* **Керування:**
    * **Коротке натискання (< 300мс):** Надсилає основну команду.
    * **Довге утримання (> 300мс):** Надсилає альтернативну команду. Таким чином, 4 кнопки можуть генерувати 8 різних команд.
    * **Обертання енкодера:** Генерує команди `Volume+` та `Volume-`. Одночасне натискання та обертання енкодера ігнорується, щоб уникнути помилкових спрацювань (пріоритет має натискання, наприклад, для команди `Mute`).

* **Логіка роботи:**
    Передавач миттєво відправляє кодову посилку при кожній дії (натискання, обертання) і не чекає "відпускання" кнопки. Це забезпечує високу швидкість реакції. Вся логіка обробки тривалості команди реалізована на стороні приймача.

#### Спеціальні режими передавача

* **Керування звуковим підтвердженням (Буззер):**
    Щоб увімкнути або вимкнути буззер, виконайте наступні дії:
    1.  Вимкніть живлення передавача.
    2.  Затисніть кнопку **"Центр" (SW3)**.
    3.  Подайте живлення на передавач.
    Стан буззера (увімкнено/вимкнено) збережеться в EEPROM.

* **Режим "Навчання магнітоли":**
    Цей режим збільшує тривалість сигналу для магнітоли, щоб вона могла надійно розпізнати та зберегти значення опору для кожної кнопки.
    1.  Вимкніть живлення передавача.
    2.  Одночасно затисніть кнопки **"Вгору" (SW2)** та **"Вниз" (SW4)**.
    3.  Подайте живлення на передавач.
    Приймач перейде в режим навчання. Для виходу з режиму достатньо короткочасно вимкнути живлення або натиснути кнопку `RESET` на передавачі.

### 🔹 Приймач (Receiver)

Приймач отримує команди від передавача, ідентифікує їх та підключає до виходу (KEY1 або KEY2) резистори відповідного номіналу.

* **Логіка роботи:**
    * При отриманні команди від кнопки, резистор підключається на **500 мс** (`keyHold`).
    * При отриманні команди від енкодера — на **100 мс** (`hold`).
    * У режимі **"Навчання"** тривалість збільшується до **3000 мс** для кнопок та **2600 мс** для енкодера.
    Ці значення можна змінити у скетчі приймача.

* **Зворотний зв'язок:**
    Після отримання команди приймач надсилає передавачу підтвердження. Якщо передавач не отримує підтвердження, він робить 15 спроб повторної відправки. Якщо всі спроби невдалі, буззер (якщо увімкнений) подасть три короткі сигнали.

* **Функція термостата:**
    Приймач може керувати вентилятором охолодження магнітоли.
    * **Датчик:** DS18B20.
    * **Керуючий елемент:** Транзистор Дарлінгтона TIP120 (або будь-який інший відповідний транзистор/реле).
    * **Налаштування у скетчі:**
        * `const int8_t maxTemp = 45;` — температура увімкнення вентилятора (за замовчуванням 45°C).
        * `const int8_t destTemp = 30;` — температура вимкнення вентилятора (за замовчуванням 30°C).
        * `#define FAN_ON true` — логіка керування (`true` для подачі "+", `false` для подачі "-").
        * `const uint8_t tempSensPeriod = 30;` — період опитування датчика в секундах. **Не рекомендується** встановлювати значення менше 5 секунд через можливі затримки в роботі (бібліотека `OneWire` використовує `delay()`).

---

## Специфіка збірки та налаштування

### Рекомендації щодо компонентів

* **Мікроконтролери:** Проект сумісний з Arduino Nano на базі **Atmega328/168** та їх китайськими клонами **LGT8F328P**.
    ![LGT8F328P](https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/LGT8F328P.png?raw=true)
    *Плагін для підтримки LGT8F328P в Arduino IDE можна завантажити [тут](https://github.com/dbuezas/lgt8fx).*

* **Радіо-модулі NRF24L01:** Купуйте модулі у перевірених продавців. Дешеві підробки можуть некоректно працювати з режимами сну.

* **Енкодер:** Енкодери від комп'ютерних мишей можуть мати різне розпіновку. "Земляний" вивід (C) зазвичай підключений до загальної маси на платі миші. Два інші виводи (A/B) є сигнальними. Якщо енкодер працює некоректно:
    1.  Перевірте розпіновку.
    2.  У скетчі передавача змініть `c1.setType(TYPE1)` на `TYPE2`.
    3.  Додайте керамічні конденсатори (10-100 нФ) між сигнальними виводами (A/B) та масою.

* **Буззер:** Рекомендується використовувати пасивний п'єзоелемент. Активний буззер буде працювати, але звук може бути спотвореним.

### Варіант живлення передавача від батареї

Передавач може працювати від батареї (2.2-3.6 В) завдяки використанню режиму сну. Для цього рекомендується:
1.  Використовувати плату **Arduino Pro Mini 3.3V 8MHz**.
2.  Видалити з плати стабілізатор напруги та світлодіод індикації живлення для зменшення енергоспоживання.
3.  Живити плату та модуль NRF24L01 безпосередньо від літієвої батареї (наприклад, CR2030).

**Приблизна схема:**
![Schematic battery TX](https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/Schematic_battery_TX.png?raw=true)

### ⚠️ Важливі зауваження щодо живлення

* **НЕ** підключайте живлення від бортової мережі автомобіля (>12В) безпосередньо до піну `VIN` на платі Arduino. Використовуйте зовнішній стабілізатор напруги, наприклад, **LM7805**.
* **НЕ** живіть модуль NRF24L01 від виходу `3.3V` на платі Arduino. Струм споживання модуля перевищує можливості вбудованого стабілізатора. Використовуйте окремий стабілізатор на **3.3В**, наприклад, **AMS1117-3.3**.
* Найкраще джерело живлення для приймача — вихід керування зовнішнім підсилювачем на магнітолі (Remote). Напруга на ньому з'являється після повного завантаження магнітоли і зазвичай становить близько 9В, що зменшує нагрів стабілізатора LM7805.

---

## Підтримати автора
Якщо цей проект був вам корисний, ви можете подякувати автору:
[<img src="https://www.paypalobjects.com/digitalassets/c/website/logo/full-text/pp_fc_hl.svg" width="100">](https://www.paypal.me/kostyamat)

