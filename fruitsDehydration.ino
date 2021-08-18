/**
 * Автоматизация сушилки для фруктов и овощей Supra-DFS-201
 * 
 * В меню IDE Инструменты->Плата выбрать ArduinoNano
 * Библиотека реле
 * https://alexgyver.ru/gyverrelay/
 * https://github.com/GyverLibs/GyverRelay/archive/refs/heads/main.zip 
 * 
 * Библиотека дисплея 
 * GyverTM1637 https://alexgyver.ru/tm1637_display/
 * https://github.com/GyverLibs/GyverTM1637/archive/refs/heads/main.zip
 * 
 * Библиотека энкодера
 * https://alexgyver.ru/encoder/
 * https://github.com/GyverLibs/GyverEncoder/archive/refs/heads/main.zip
 * 
 * Через меню IDE "Менеджер библиотек" найти и установить библотеки
 *   - OneWire
 *   - DallasTemperature
 * 
 * Датчик температуры DS18B20
 * Подключение к PIN6
 * 
 * Твердотельное реле SSR-10 D
 * Подключение к PIN5
 * 
 * Твердотельное реле управляет нагревателем.
 * Вентилятор должен вращаться всегда
 * 
 * 
  */


#include <OneWire.h>
#include <DallasTemperature.h>
#include <GyverRelay.h>
#include <GyverTM1637.h>
#include <GyverEncoder.h>

//Датчик температуры
#define PIN_T_SENSOR 6
//Реле, управляющее нагревателем
#define PIN_HEATER 5

//дисплей
#define DISP_CLK 2
#define DISP_DIO 3

//энкодер
#define ENC_CLK 7
#define ENC_DT 8
#define ENC_SW 93


//термодатчик DS18B20
OneWire oneWire(PIN_T_SENSOR);
DallasTemperature sensors(&oneWire);


//Работа с реле, управление нагревателем
GyverRelay regulator(REVERSE);

//Дисплей
GyverTM1637 disp(DISP_CLK, DISP_DIO);

//энкодер
Encoder enc1(ENC_CLK, ENC_DT, ENC_SW);  // для работы c кнопкой


float tempValue = 20;
float targetTempValue = 20;

int encValue = 0;

unsigned long windowStartTime;


void relayInit(){
  pinMode(PIN_HEATER, OUTPUT);             // пин реле
  digitalWrite(PIN_HEATER, LOW);
  regulator.setpoint    = getTargetTemp();// установка температуры, к которой стремиться система управления
  regulator.hysteresis  = 5;              // ширина гистерезиса
  regulator.k           = 0.5;            // коэффициент обратной связи (подбирается по факту)
  //regulator.dT        = 500;            // установить время итерации для getResultTimer
}


/**
 * Установить для решулировки текущее значение величины, которая регулируется
 * (управление стемиться input сделать == setpoint )
 * @param float value текущее значение управлемой системы
 */
void relaySetCurrentValue(float value ){
  regulator.input    = value;

}

void relaySetTargetValue( float value ){
  regulator.setpoint = value;    // установка температуры, к которой стремиться система управления
}



/**
 * Установить значение целевой температуры, к которой
 * стремитья система управления
 * @param float value значение
 */
void setTargetTemp(float value){
  targetTempValue    = value;
  relaySetTargetValue(  value );
}

/**
 * Получить значение целевой температуры, к которой
 * стремитья система управления
 */
float getTargetTemp(){
  return( targetTempValue );
}


/**
 * Установить текущую температуру
 * @param float value значение
 */
void setTemp(float value){
  tempValue    = value;
  relaySetCurrentValue( value );
}

/**
 * Получить текущую температуру
 */
float getTemp(){
  return( tempValue );
}


/*
 * Инициализация дисплея отображения
 * текущей температцуры и настройки температуры
 */
void displayInit(){
  disp.clear();
  disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
  disp.clear();
  disp.displayByte(_H, _E, _L, _L);
}


float dispayValue = 0;
float dispayValueOld = 0;
/**
 * Отобразить на экране значение температуры или настройки
 * @param bool   setupMode true-отображение значение настройки, иначе - температуры
 */
void displayShow(bool setupMode ){
  uint8_t Digits[] = {0x00,0x00,0x00,0x00};
  /*
   * В режиме настройки отображать значение от энкодера,
   * а в обычном режиме - значение темпераутры
  */
   if( setupMode ) dispayValue =  endcoderGetValue();
  else  dispayValue =  getTemp();
  
  //даем команду модулю на перерисовку значения только тогда,
  //когда значение изменилось, иначе пусть продолжается
  //отображаться ранее записанное
  if( dispayValue != dispayValueOld ){
    disp.clear();
    Digits[0] = (uint8_t)(dispayValue / 1000) % 10; // раскидываем 4-значное число на цифры
    Digits[1] = (uint8_t)(dispayValue / 100) % 10;
    Digits[2] = (uint8_t)(dispayValue / 10) % 10;
    Digits[3] = (uint8_t)(dispayValue) % 10;

    disp.displayByte( Digits );
    dispayValue = dispayValueOld;
  }
  
}


/*
 * Инициализация энкодера для настройки температуры
 */
void endcoderInit(){
  enc1.setType(TYPE2);    // тип энкодера TYPE1 одношаговый, TYPE2 двухшаговый. Если ваш энкодер работает странно, смените тип  
}

/*
 * Получить текущее значение настраиваемой темпераутры
 */
float endcoderGetValue(){
  return(encValue);  
}


/*
 * Обработчик энкодера, который обрабатывает события железяки
 */
void encoderHandler(){
  int encValueOld = encValue;
  
  enc1.tick();

  if (enc1.isRight()) encValue++;       // если был поворот направо, увеличиваем на 1
  if (enc1.isLeft()) encValue--;      // если был поворот налево, уменьшаем на 1
  
  if (enc1.isRightH()) encValue += 5;   // если было удержание + поворот направо, увеличиваем на 5
  if (enc1.isLeftH()) encValue -= 5;  // если было удержание + поворот налево, уменьшаем на 5  


   //ограничение значений разумными пределами
   if( encValue<15) encValue = 15;
   if( encValue>70) encValue = 70;

  if (enc1.isTurn()) {       // если был совершён поворот (индикатор поворота в любую сторону)
    setTargetTemp( encValue );
    Serial.println(encValue);  // выводим значение при повороте
  }

 if( encValue != encValueOld ) displayShow( true );   
}



/**************************************************
 * 
 */
void setup()
{
  Serial.begin(115200);
  Serial.println("Starting");
  relayInit();
  displayInit();
  endcoderInit();
  
 // windowStartTime = millis();

}


/**************************************************
 * 
 */
void loop(){

  displayShow( false );
  encoderHandler();
  
  sensors.requestTemperatures();
  tempValue = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(tempValue);


  regulator.input = tempValue;   // сообщаем регулятору текущую температуру
  // getResult возвращает значение для управляющего устройства
  digitalWrite(PIN_HEATER, regulator.getResult());  // отправляем на реле (ОС работает по своему таймеру)

  
/*
  if (millis() - windowStartTime > WindowSize)
  {
    //time to shift the Relay Window
    windowStartTime += WindowSize;
  }
  if (Output < millis() - windowStartTime) 
    digitalWrite(PIN_HEATER, HIGH);
  else 
    digitalWrite(PIN_HEATER, LOW);
*/

}
