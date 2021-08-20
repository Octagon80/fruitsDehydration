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
#define INARDUINO   true
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
  #define PIN_T_SENSOR 0
  OneWire oneWire(PIN_T_SENSOR);
  DallasTemperature sensors(&oneWire);
  DeviceAddress tempDevAddress;
#endif  


#if USE_RELE
  #define PIN_HEATER 6
  //Работа с реле, управление нагревателем
  GyverRelay regulator(REVERSE);
#endif
  
#if USE_DISPLAY
  //Дисплей
  #define DISP_CLK 7
  #define DISP_DIO 8
  GyverTM1637 disp(DISP_CLK, DISP_DIO);
#endif

#if USE_ENCODER
 //энкодер
 #define ENC_CLK 2
 #define ENC_DT  3
 #define ENC_SW  4
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

//Значение темперутуры при выявленной ошибке работы с датчиком  
#define TEMPVALUE_ERROR -999
  
/**
 * Значение текущей температуры
 * Использовать для получение и установки getTemp()/setTemp()
 * @type float
 */  
float tempValue       = 20;
/**
 * Значение целевой температуры, к которой должна стремиться система
 * Использовать для получение и установки getTargetTemp()/setTargetTemp()
 * @type float
 */  
float tempTargetValue = 20;
/**
 * Наличие термодатчика
 */
bool tempSensIsPresent = false;
/**
 * Адрес датчика температуры успешно считан
 */
bool tempSensAddressPresent = false;
/**
 * Готовность системе к работе по термодатчику.
 * @type boolean false Если термодатчик отсутствует, не готов, дает ложные значения
 */
bool readyByTempSensors = false;

/**
 * готовность работы нагревателя
 * @type bool true == можно включать нагреватель
 *            false == нельзя, если включен, выключить
 */
bool readyHeater = false;

int encValue = 0;


//частота обновления температуры ms, как следствие частота обновления регулируемого значения для реле
//подобрать опытным путем
unsigned long maxTimeoutUpdateTemper = 500;
//текщее значение счетчика до следующего обновления значения температуры
unsigned long curtimeoutUpdateTemper;


//предопределение
float endcoderGetValue();


/**
 * Обновить готовность работы системы по термодатчику
 * Ограничения:
 *  работаем летом
 *  до 70 градусов, иначе вырубаем нагреватель
 */
void updateAllowedSystem(){
  //готовность термодатчика
  readyByTempSensors = tempSensIsPresent && tempSensAddressPresent && getTemp()>5 && getTemp()<70 ;
  
  //готовность (разрешение) к включению нагревателя
  readyHeater = readyByTempSensors;
}

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
  tempTargetValue    = value;
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
  return( tempTargetValue );
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
  return( (tempSensIsPresent)?tempValue: TEMPVALUE_ERROR );
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
    
    //лидирующие нули заменяем на пусто
    for(byte i=0; i<4; i++ ){
        if( Digits[i] == 0 ) Digits[i] = _empty;
        //до первого ненулевого значения
        else break;
    }
    #if USE_DISPLAY
      #if INARDUINO
        disp.clear();
        //При отсутствии готовности датчика температуре и режима отображения температуры
        //показать код ошибки
        if( !readyByTempSensors && !setupMode ) disp.displayByte( _E,_r,_r, _empty );
        else disp.displayByte( Digits );
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
      //Если датчик найден и ранее был получен адрес датчика...
     if( tempSensIsPresent && tempSensAddressPresent ){
        //если датчик по-прежнему подключен
       if( sensors.isConnected( tempDevAddress ) ){
           //sensors.requestTemperatures();
           ////sensors.requestTemperaturesByAddress( tempDevAddress );
           //tempValue = sensors.getTempCByIndex(0);
           tempValue = sensors.getTempC( tempDevAddress ); 
       }else{
         //иначе запомнить ошибочное значение температуры
         //на которое контроль готовности системы к работе
         //отреагирует как на запрет работы нагревателя
         tempValue = TEMPVALUE_ERROR;
       }
     } 
        
    #else  
      tempValue = rand() % 70;
    #endif
    
    #if DEBUG
      Serial.print("Real temperature: ");
      Serial.println(tempValue);
    #endif  
    
  #else  
  
    tempValue = rand() % 70;
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
  
  #if USE_DS18B20
   sensors.begin();
   tempSensIsPresent = sensors.getDS18Count() == 1; //sensors.getDeviceCount == 1;
   
   #if DEBUG
     if( !tempSensIsPresent ){  
       Serial.println("Термодатчик отсутствует, работа невозможна. ");
     }  
    #endif  

  tempSensAddressPresent = false; 
  if (tempSensIsPresent && sensors.getAddress(tempDevAddress, 0)){ 
     tempSensAddressPresent = true;
  }else{
   #if DEBUG
      Serial.println("Unable to find address for Device 0"); 
    #endif       
  }
      
  #endif

  curtimeoutUpdateTemper = millis();

}


/**************************************************
 * 
 */
void loop(){
  updateAllowedSystem();
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

#if USE_RELE
   //управление нагревателем по таймеру, если есть разрешение
    if( readyHeater){
        digitalWrite(PIN_HEATER, regulator.getResultTimer());  // отправляем на реле
    }else{
        digitalWrite(PIN_HEATER, LOW );
    }       
#endif  
  #endif
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
