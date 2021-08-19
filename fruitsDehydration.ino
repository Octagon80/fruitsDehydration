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

  
//Определяем, где работает программа: на Ардуине?, тогда true
#define INARDUINO false
#define DEBUG       true
#define USE_DS18B20 true
#define USE_RELE    true
#define USE_DISPLAY true
#define USE_ENCODER true

#if INARDUINO
  
#if USE_DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#if USE_RELE
  #include <GyverRelay.h>
#endif

#if USE_DISPLAY
  #include <GyverTM1637.h>
#endif
  
#include <GyverEncoder.h>





#if USE_DS18B20
  //термодатчик DS18B20
  #define PIN_T_SENSOR 6
  OneWire oneWire(PIN_T_SENSOR);
  DallasTemperature sensors(&oneWire);
#endif  


#if USE_RELE
  #define PIN_HEATER 5
  //Работа с реле, управление нагревателем
  GyverRelay regulator(REVERSE);
#endif
  
#if USE_DISPLAY
  //Дисплей
  #define DISP_CLK 2
  #define DISP_DIO 3
  GyverTM1637 disp(DISP_CLK, DISP_DIO);
#endif

#if USE_ENCODER
 //энкодер
 #define ENC_CLK 7
 #define ENC_DT 8
 #define ENC_SW 93
 Encoder enc1(ENC_CLK, ENC_DT, ENC_SW);  // для работы c кнопкой
#endif 
  
#else
  #include <cstdlib>
  using namespace std;
  #include <stdio.h>
  #include <string.h>
  #include <cmath>
  #include <time.h>
  #include "Serial.h"
  #include "Arduino.h"
  ObjSerial Serial;

  
#endif 

float tempValue = 20;
float targetTempValue = 20;

int encValue = 0;


//частота обновления температуры ms, как следствие частота обновления регулируемого значения для реле
//подобрать опытным путем
unsigned long maxTimeoutUpdateTemper = 500;
//текщее значение счетчика до следующего обновления значения температуры
unsigned long curtimeoutUpdateTemper;


//предопределение
float endcoderGetValue();


void relayInit(){
  #if DEBUG
    Serial.print( "void relayInit()\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG
      Serial.println( "Инициализация GyverRelay\r\n" );
    #endif  
    #if INARDUINO
      pinMode(PIN_HEATER, OUTPUT);             // пин реле
      digitalWrite(PIN_HEATER, LOW);
      regulator.setpoint    = getTargetTemp();// установка температуры, к которой стремиться система управления
      regulator.hysteresis  = 5;              // ширина гистерезиса
      regulator.k           = 0.5;            // коэффициент обратной связи (подбирается по факту)
      //regulator.dT        = 500;            // установить время итерации для getResultTimer
    #endif  
 #else
    //Serial.println( "Игнорируем GyverRelay\r\n" );
 #endif   
}


/**
 * Установить для решулировки текущее значение величины, которая регулируется
 * (управление стемиться input сделать == setpoint )
 * @param float value текущее значение управлемой системы
 */
void relaySetCurrentValue(float value ){
  #if DEBUG
    Serial.print( "void relaySetCurrentValue(float " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG
      Serial.print( "GyverRelay input=" );Serial.print( value ); Serial.println( "\r\n" );
    #endif  
    #if INARDUINO
      regulator.input    = value;
    #endif  
  #endif  
}

void relaySetTargetValue( float value ){
  #if DEBUG
    Serial.print( "void relaySetTargetValue( float  " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG
      Serial.print( "GyverRelay setpoint=" );Serial.print( value ); Serial.println( "\r\n" );
    #endif
    #if INARDUINO  
     regulator.setpoint = value;    // установка температуры, к которой стремиться система управления
    #endif 
 #endif 
}



/**
 * Установить значение целевой температуры, к которой
 * стремитья система управления
 * @param float value значение
 */
void setTargetTemp(float value){
  #if DEBUG
    Serial.print( "void setTargetTemp(float " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  targetTempValue    = value;
  relaySetTargetValue(  value );
}

/**
 * Получить значение целевой температуры, к которой
 * стремитья система управления
 */
float getTargetTemp(){
  #if DEBUG
    Serial.print( "float getTargetTemp()\r\n" );
  #endif  
  return( targetTempValue );
}


/**
 * Установить текущую температуру
 * @param float value значение
 */
void setTemp(float value){
  #if DEBUG
    Serial.print( "void setTemp(float  " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  tempValue    = value;
  relaySetCurrentValue( value );
}

/**
 * Получить текущую температуру
 */
float getTemp(){
  #if DEBUG
    Serial.print( "float getTemp()\r\n" );
  #endif  
  return( tempValue );
}


/*
 * Инициализация дисплея отображения
 * текущей температцуры и настройки температуры
 */
void displayInit(){
  #if DEBUG
    Serial.print( "void void displayInit()\r\n" );
  #endif  
  #if USE_DISPLAY
    #if DEBUG
      Serial.println( "Инициализация дисплея\r\n" );
    #endif  
    #if INARDUINO
      disp.clear();
      disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
      disp.clear();
      disp.displayByte(_H, _E, _L, _L);
    #endif  
  #endif   
}


float dispayValue = 0;
float dispayValueOld = 0;
/**
 * Отобразить на экране значение температуры или настройки
 * @param bool   setupMode true-отображение значение настройки, иначе - температуры
 */
void displayShow(bool setupMode ){
  uint8_t Digits[] = {0x00,0x00,0x00,0x00};
  #if DEBUG
    Serial.print( "void displayShow(bool " );Serial.print( setupMode ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
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
    Digits[0] = (uint8_t)(dispayValue / 1000) % 10; // раскидываем 4-значное число на цифры
    Digits[1] = (uint8_t)(dispayValue / 100) % 10;
    Digits[2] = (uint8_t)(dispayValue / 10) % 10;
    Digits[3] = (uint8_t)(dispayValue) % 10;
    #if USE_DISPLAY
      #if INARDUINO
        disp.clear();
        disp.displayByte( Digits );
       #endif  
    #endif  
    #if DEBUG
      Serial.print( "display: " );Serial.print(  Digits[0] ); Serial.print(  Digits[1] );Serial.print(  Digits[2] );Serial.print(  Digits[3] );Serial.println( "\r\n" );
    #endif  
    dispayValueOld = dispayValue;
  }
  
}


/*
 * Инициализация энкодера для настройки температуры
 */
void endcoderInit(){
  #if DEBUG
    Serial.println( "void endcoderInit()\r\n" );
  #endif  
  #if USE_ENCODER
    #if INARDUINO
      enc1.setType(TYPE2);    // тип энкодера TYPE1 одношаговый, TYPE2 двухшаговый. Если ваш энкодер работает странно, смените тип  
    #endif
  #endif  
}

/*
 * Получить текущее значение настраиваемой темпераутры
 */
float endcoderGetValue(){
  #if DEBUG
    Serial.print( "float endcoderGetValue() --> " ); Serial.println(encValue);
  #endif  
  return(encValue);  
}


/*
 * Обработчик энкодера, который обрабатывает события железяки
 */
void encoderHandler(){
  #if DEBUG
    Serial.println( "void encoderHandler()\r\n" );
  #endif  
  int encValueOld = encValue;

  #if USE_ENCODER
    #if INARDUINO
      enc1.tick();

      if (enc1.isRight()) encValue++;       // если был поворот направо, увеличиваем на 1
      if (enc1.isLeft()) encValue--;      // если был поворот налево, уменьшаем на 1
  
      if (enc1.isRightH()) encValue += 5;   // если было удержание + поворот направо, увеличиваем на 5
      if (enc1.isLeftH()) encValue -= 5;  // если было удержание + поворот налево, уменьшаем на 5  


       //ограничение значений разумными пределами
       if( encValue<15) encValue = 15;
       if( encValue>70) encValue = 70;

      if (enc1.isTurn()) {       // если был совершён поворот (индикатор поворота в любую сторону)
        #if DEBUG
          Serial.print("encoder завершли крутить, значение=");Serial.println(encValue);  // выводим значение при повороте
        #endif  
        setTargetTemp( encValue );
      }
    #else  
     setTargetTemp( 30 );
    #endif  
  #endif  

 if( encValue != encValueOld ) displayShow( true );   
}


void updateTemperature(){
  #if DEBUG
    Serial.println( "void updateTemperature()\r\n" );
  #endif    
  #if USE_DS18B20
    #if INARDUINO
      sensors.requestTemperatures();
      tempValue = sensors.getTempCByIndex(0);
    #else  
      tempValue = rand();
    #endif
    #if DEBUG
      Serial.print("Real temperature: ");
      Serial.println(tempValue);
    #endif  
  #else  
    tempValue = random(20,22);
    #if DEBUG
      Serial.print("Virtual temperature: ");
      Serial.println(tempValue);
    #endif  
  #endif    

}


/**************************************************
 * 
 */
void setup()
{
  #if DEBUG
    Serial.begin(115200);
    Serial.println("Starting");
  #endif  
  relayInit();
  displayInit();
  endcoderInit();
  
  curtimeoutUpdateTemper = millis();

}


/**************************************************
 * 
 */
void loop(){

  displayShow( false );
  encoderHandler();
  
  //измерять температуру будем нечасто, как следствие нечастая коррекция значения для управления реле
  if (millis() - curtimeoutUpdateTemper > maxTimeoutUpdateTemper) {
    updateTemperature();  
    curtimeoutUpdateTemper = millis();
  }

  #if INARDUINO
    #if USE_RELE
      digitalWrite(PIN_HEATER, regulator.getResult());  // отправляем на реле (ОС работает по своему таймеру)
    #endif  
  #endif

  
/*

*/

}




#if INARDUINO
#else
int main(int argc, char** argv) {
   srand(time(NULL));
    setup();
    
    while( 1==1 ) loop();
    
    return (EXIT_SUCCESS);
}
#endif
