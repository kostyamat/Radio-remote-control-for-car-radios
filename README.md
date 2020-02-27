# Car-china-radio-remote-control
https://www.youtube.com/watch?v=470r13HLdKc

Система состоит из двух устройств: передатчик и приемник. Схемы обоих устройств представлены в файле Schematic_TX_RX-buttons.png

Для сборки используются готовые платы, аналоги Arduino Nano, на микроконтроллерах Atmega328/Atmega168 или их китайском клоне LGT8F328P (кстати, очень не плохой чип). Так же используются модули NRF24L01.

Передатчик обеспечивает передачу 10 дискретных команд, в том числе регулировку звука с помощью рототативного энкодера от, пришедшей в негодность, компьютерной мышки.
Так же, есть специальный режим - "обучение магнитолы".

Приемник, если реализована соответствующая часть принципиальной схемы, поддерживает датчик температуры DS18B20, и соответственно 
может управлять внешним вентилятором охлаждения для магнитолы.



# Передатчик:

Используются три дискретные тактовые кнопки,  + кнопка размещенная под энкодером от мыши и сам энкодер, для регулировки громкости.

Кратковременное нажатие на любую из кнопок, или вращение энкодера, формирует кодовую посылку, передающуюся на приемник. 

Удержание кнопки более 300мс формирует модифицированную кодовую посылку, соответствующую альтернативной команде. Таким образом, кнопки могут формировать до 8-ми независимых команд. Две дополнительные команды (предусмотренны как Volume+ и Volume-) формирует вращение энкодера.

Одновременное вращение и нажатие кнопки энкодера трактуется передатчиком как нажатие кнопки энкодера, и не формирует альтернативные команды. Так сделано во избежание ошибок во время вождения, так как кнопку энкодера логично привязывать к команде Mute.

Передатчик не отправляет код отмены команды (отжатия кнопок), поэтому логика обработки команд реализована в приемнике. Так сделано для того, чтобы избежать задержек в loop программы передатчика. Любое быстрое последовательные нажатия кнопок фактически будут обработаны передатчиком моментально, и последовательно переданы приемнику. Приемник же, в свою очередь, обработает такую последовательность команд корректно, последовательно сменив состояния пинов с соответствующими, пришедшим командам, резисторами.

В схеме передатчика предусмотрен буззер, для звукового подтверждения нажатий кнопок, или вращения энкодера. Поддерживается не требующее доступа к плате передатчика, управление состоянием звукового подтверждения - включено\выключено.
Для переключения режимов - включен\выключен, нужно на обесточенном передатчике зажать кнопку "Центр" (указана на схеме как SW3) и подать на передатчик питание (надеюсь у вас передатчик питается через замок зажигания). Если буззер был включен, он выключится, и наоборот. Состояние сохраняется в ЕЕПРОМ, и будет использовано при последующих запусках автомобиля.

Для включения режима "Обучение магнитолы" нужно обесточить передатчик (еще раз, - надеюсь передатчик у Вас подключен через замок зажигания автомобиля), и зажав кнопки "Вверх" (SW2) и "Вниз" (SW4), подать на него питание. Кодовая посылка будет модифицирована и приемник будет понимать, что нужно перейти в режим "Обучение магнитолы". В этом режиме приемник будет передавать каждую команду магнитоле на протяжении около 3-х секунд. Этого времени достаточно, чтобы магнитола четко определила подключенный резистор и прописала его значение себе в память. Выход из этого режима - кратковременно обесточить передатчик (отключить замок зажигания), или нажать на передатчике кнопку RESET.

Может быть так, что у вас в руле уже установлены некие кнопки управления (как у меня в KIA Magentis II), и на них, при включении габаритных огней, приходит сигнал подсветки +12 вольт. В таком случае есть смысл реализовать те части схемы приемника и передатчика, которые отвечают за включение подсветки на кнопках и на магнитоле. Это обеспечит вам подсветку кнопок своей конструкции, а так же передать этот сигнал приемнику, который в свою очередь передаст его на вход ILLUMINATION магнитолы .





# Приемник:

Получив кодовую посылку от передатчика, приемник идентифицирует команду и подключает резисторы соответствующего номинала, расположенные на пинах микроконтроллера, к массе, второй вывод резистора, фактически подключен к выводу KEY1 или KEY2. 
И в случае если это команда от кнопки или энкодера, фиксируется в таком состоянии на период 500мс, что обеспечивает гарантированное считывание резистора магнитолой. В случае если происходит вращение энкодера, длительность подключения соответствующих резисторов равна 100мс. 
Эти две временные задержки описаны и могут бить изменены в скетче приемника (читайте описание констант hold и keyHold).
В случае если передатчик прислал состояние "Обучение магнитолы", эти временные задержки возрастают на 2500мс. И для энкодера равняются 2600мс на щелчок, а для каждой из кнопок - 3000мс.

Получив команду, приемник отсылает передатчику сигнал подтверждения приема. Если по каким либо причинам передатчик не получил подтверждения приема команды, он повторит попытку 15 раз, и если снова нет, то вы услышите три коротких попискивания, конечно же  в случае если буззер в состоянии "включен".


Приемник, кроме как симулировать нажатие резистивных кнопок, умеет работать и как термостат, управляющий дополнительным вентилятором охлаждения магнитолы. В схеме применен датчик DS18B20, а вентилятор подключен через дарлингтон-транзистор TIP120 (у меня их кучка, прост в использовании, несколько ампер держит даже без радиатора). Транзистор можно заменить любым подходящим, да хоть КТ815, главное чтобы держал достаточный ток, ну и обвязка нужна несколько другая. Так же можно использовать транзисторы или реле-модули обратной проводимости, управляемые минусом, а не плюсом. В этом случае в скетче приемника нужно поменять дефайн #define FAN_ON true на #define FAN_ON false (то же самое и для выхода управления подсветкой, читай комментарии к дефайнам и константам в скетче).
Температуру, при которой включается вентилятор можно сменить константой const int8_t maxTemp - по умолчанию 45С
Температура выключения - const int8_t destTemp - по умолчанию 30. Имейте в виду, куллер не есть холодильник, сколько не дуй, температуру ниже, чем в салоне +5-10C получить не удасться. Если у вас нет кондиционера, лучше ставить нижнее значение не менее 35 градусов. Если  установите слишком низкую - вентилятор будет работать без остановки.

Так же, не стоит ставить температуру включения вентилятора слишком высокой - радиатор магнитолы + доп.радиатор (если есть) + термосопротивление контакта датчика с радиатором + масса корпуса самого датчика = достаточно большая инерционность. Пока на датчике наберет 50С, на чипе усилителя, или процессора, вполне наберет 70-80C. По причине инерционности не рекомендую покупать датчик DS18B20 в исполнении "щуп с кабелем". Лучше купите обыкновенный вариант, в корпусе как у транзистора. Такой датчик: имеет большую площадь контакта с радиатором; может быть приклеен супер-клеем плоской стороной на прямую к радиатору; имеет меньшую собственную массу = меньшую температурную инерционность (быстрее нагревается и остывает).
Период опроса датчика const uint8_t tempSensPeriod, по умолчанию, равен 30 секундам, можно опрашивать датчик и чаще, но нет особого смысла. Можно опрашивать реже, но тоже смысла не вижу. Меняйте на свой вкус, но желательно опрашивать не чаще чем раз в 5секунд. К тому же, частый опрос датчика приводит к его само-разогреву. 

Имейте ввиду, что библиотека OneWire, используемая для подключения датчика температуры, написана с использованием delay(). Поэтому, во время опроса датчика, основной цикл приемника будет немного подвисать. Это может привести к "задумчивости" при выполнении команд с передатчика. Я считаю, что период опроса в 30 секунд - оптимальное значение.



# Специфика сборки и настройки:

Скетч передатчика написан таким образом, что фактически передатчик можно питать от батареи напряжением 2.2-3.6 вольт. В скетче отключен детектор пониженного напряжения питания, а так же используется режим PowerDown, все время пока ничего не происходит. При нажатии любой из кнопок, или при вращении энкодера, микроконтроллер просыпается считывает состояние своих кодов, будит модуль NRF24L01, отправляет команду, и по истечении 1500мс снова засыпает (время до сна можно подправить под свои предпочтения). 1500мс выбрано для корректной работы энкодера, хотя вполне можно уменьшить и до 500мс, мне так показалось надежнее. Вот с модулем NRF24L01 могут быть нюансы, на рынке много отбраковки и подделок (которые, как не странно, работают). На многих зарубежных форумах были сообщения, что отбраковка и клоны могут не спать, или спать очень плохо, либо вообще не просыпаться после сна. Не экономьте на радио-модулях, покупайте у солидных и проверенных продавцов. Я свои на Amazon.es покупал, чуть дороже, но работают отлично.

Конечно же, при реализации передатчика с питанием от батареи, придется использовать плату Arduino Pro Mini 3.3v 8MHz. С нее придется "сдуть" стабилизатор напряжения и светодиоды (это все тоже кушает заряд батареи, даже если микроконтроллер спит). Запитать это все можно напрямую от литиевой батареи, к примеру от двух паралельных CR2030. Соответственно стабилизаторы, как на схеме, по питанию не нужны, питание радио-модуля нужно запаралелить с питанием Arduino Pro Mini (еще раз напоминаю - нужна версия на 3.3V), объединив его с пинами 3.3V и GND платы ардуино, соответственно. Примерная схема https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/Schematic_battery_TX.png

На рынке присутствуют ардуино-совместимые платы на китайском клоне Atmega328 - LGT8F328P\D. Собственно приемник я паял именно на NANO-совместимой плате, на LGT8F328P, и все отлично работает. Но ввиду специфика кода передатчика, работоспособность, и главное сон, китайских клонов нужно проверять отдельно. (Буду рад фидбеку по этому поводу).
На китайских торговых площадках сейчас много разновидностей NANO-подобных плат на чипах-клонах, во избежание всякого рода проблем при реализации схемы, рекомендую брать платы такие, как на картинке https://github.com/kostyamat/Car-china-radio-remote-control/blob/master/LGT8F328P.png
Хорошо реализованный плагин поддержки этих чипов можно скачать тут https://github.com/dbuezas/lgt8fx

(Персонально я не делал передатчик с питанием от батареи, мне это не было нужно. Да и платы Arduino Pro Mini 3.3V 8MHz у меня попросту нет, а полноценную Arduino NANO "калечить" мне не хочется. Буду рад если кто-то, спаяв вариант с питанием от батареи, отпишется о результатах)

Как показала практика, энкодеры мышки бывают некоторых разновидностей, и распиновка их не очевидна. Для правильного использования, вам нужно внимательно проанализировать плату, и определится с "земляным" выводом (на схеме обозначен как C). Чаще всего, им окажется средний вывод, но, как в моем случаи, им может оказаться любой крайний. Идентифицировать выводы не сложно -  два сигнальных вывода (A/B) всегда уходят к выводам специализированной микросхемы мыши (какой из них A, а какой B - не важно, они в данной схеме взаимозаменяемы), а "земляной" приходит на массу\общий питания.

Если ваш энкодер ведет себя странно то: а) вы неправильно определили распиновку; б) вам нужно поменять параметр c1.setType(TYPE1) на TYPE2, в скетче передатчика; в) возможно вам придется поставить по конденсатору 10-100нФ между выводами A\B и массой.

Буззер для передатчика может быть любым, даже маленьким динамиком. Но все же, рекомендую использовать пассивный пьезоэлемент или пассивный буззер. Стандартный ардуиновский активный буззер тоже будет работать, но звук будет хриплым.

Не пробуйте запитать ардуино от батареи автомобиля на пин VIN, это чревато выходом ее из строя. Теоретически это делать можно, а практически - качество установленных на ней китайских AMS1117-5.0 очень далеко от заявленных параметров. Сэкономив на копеечном LM7805, вы можете потерять целую плату.

Не пробуйте запитать NRF24L01 от выхода +3.3V ардуино. Ток, который потребляет радиомодуль, в несколько раз превышает ток, который способно обеспечить на этом выходе ардуино. Вы либо не получите стабильную работу радимодуля, либо сожжете выход +3.3V ардуино. Питание NRF24L01 через отдельный линейный стабилизатор AMS1117-3.3 - обязательно.

Питание приемника лучше всего получить прямо с магнитолы, с выхода управления внешним усилителем. На этом выходе напряжение около +9V, что уменьшит нагревание стабилизатора питания LM7805, и отделит питание приемника радио-удлиннителя от помех по питанию, от системы зажигания автомобиля. Напряжение на выходе управления внешним усилителем магнитолы появляется после полной загрузки андроид.




# Купить мне пиво можно тут https://www.paypal.me/kostyamat
