Автоматизация сушилки для фруктов и овощей Supra-DFS-201 на основе Arduino
==========================================================================

Данный проект осуществляет автоматизированное поддержание заданной температуры (управляя нагревателем). В общем сама сушилка работает в полуавтоматическом режиме, другими словами, без контроля процесса человеком не обойтись..



Что нужно сделать для данного проекта.
--------------------------------------
- :black_square_button: Добавить контроль того, что человек забыл о сушке (простой контроль длительности сушки).
- :black_square_button: Добавить контроль отсутствии нагрева.
- :black_square_button: Добавить звукового сигнализатор.
- :black_square_button: ~~Ошибка. Инициализированное значение "температуры поддержания" не устанавливается в инициализированной значение энкодера~~
- :black_square_button: Выбранное энкодером "температуры поддержания" не запоминается, после выключения прибора, придется устанавливать заного (если электричество отключат и вновь включат).
- :black_square_button: Добавить кнопку старт\стоп, чтобы исключить авто включение нагревателя при включении прибора.

Развитие проекта
----------------
- :black_square_button: Добавить управляемый контроль влажности
- :black_square_button: Добавить управляемый контроль времени (продолжительности) сушки.
- :black_square_button: **Программа рецептов.**




Описание по железу
------------------

Для существующей сушилки требуется создать автоматизацию. 
Остается 
 1. корпус
 1. кнопка включения
 1. внешний колпачек крутилки
 1. нагреватель
 1. вентилятор
 ![Вид1](https://raw.githubusercontent.com/Octagon80/fruitsDehydration/main/hardware-overview-1.png)
 ![Вид2](https://raw.githubusercontent.com/Octagon80/fruitsDehydration/main/hardware-overview-2.png)
 
 Добавляется
  - модуль управления Arduino Pro mini
  - индикатор на основе TM1637
  - датчик температуры DS18B20
  - энкодер настройки KY-040
  - твердотельное реле SSR-10D
  
  Схема
  -----
  
![Схема](https://raw.githubusercontent.com/Octagon80/fruitsDehydration/main/Schematic1.png  "Схема")


Аналогичные проекты
-------------------
- HumDry https://github.com/legioncmc/HumDry
- Dryasnack https://github.com/devonmagan/Dryasnack
- arduino-dehydrator https://github.com/hduijn/arduino-dehydrator
- Dehydrator https://github.com/truglodite/Dehydrator

