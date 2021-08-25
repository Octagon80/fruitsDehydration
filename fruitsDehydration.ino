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
#define DEBUG       false
#define USE_DS18B20 false
#define USE_RELE    false
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

#if USE_ENCODER
#include <GyverEncoder.h>
#endif


 
#if USE_DS18B20
  //термодатчик DS18B20
  #define PIN_T_SENSOR 7
  OneWire oneWire(PIN_T_SENSOR);
  DallasTemperature sensors(&oneWire);
  DeviceAddress tempDevAddress;
#endif  


#if USE_RELE
  #define PIN_HEATER 8
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
 #define ENC_CLK 4
 #define ENC_DT  5
 #define ENC_SW  6
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
  #define _empty 0
  ObjSerial Serial;

  
#endif 

//Значение темперутуры при выявленной ошибке работы с датчиком  
#define TEMPVALUE_ERROR      -999
//Значение темперутуры при выявленной потери связи с датчиком температуры
#define TEMPVALUE_NOCONNECT  -888
//Занчение температуры при отсутсвии датчика или невозможности прочитать его внутренний адрес
#define TEMPVALUE_NOTPRESENT -777
  
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
 * Готовность системе к работе по отсутствии режима настройки
 * @type boolean false 
 */
bool readyBySetupMode = false;

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



/**
 * Режимы работы системы
 *  0 - неопределенное состояние
 *  1 - режим управления температурой с отображением текущей температуры
 *  2 - режим настройки температуры
 * режим 1 - по умолчанию, в этот режим система будет переключаться
 * автоматически после тайаута. Т.е. выбор режима более 1,
 * вклчается таймаут, отсчитывается, и режим переключается в 1
 */
#define SYSTEM_MODE_NONE   0
#define SYSTEM_MODE_NORMAL 1
#define SYSTEM_MODE_SETUP  2
byte sytemMode = SYSTEM_MODE_NONE;
unsigned long sytemModeTimeoutMax = 5000;
unsigned long sytemModeTimeoutCur = 0;


//предопределения
float endcoderGetValue();
float getTemp();
byte getSystemMode();
void setSystemMode( byte value );

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
  readyHeater = readyByTempSensors && readyBySetupMode;
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
    //Serial.print( "void setTemp(float  " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  tempValue    = value;
  relaySetCurrentValue( value );
}

/**
 * Получить текущую температуру
 */
float getTemp(){
  #if DEBUG
    //Serial.print( "float getTemp()\r\n" );
  #endif  
  return( (tempSensIsPresent)?tempValue: TEMPVALUE_ERROR );
}


/*
 * Инициализация дисплея отображения
 * текущей температцуры и настройки температуры
 */
void displayInit(){
  #if DEBUG
    Serial.print( "void displayInit()\r\n" );
  #endif  
  #if USE_DISPLAY
    #if DEBUG
      Serial.println( "Inicializaciya displeya\r\n" );
    #endif  
    #if INARDUINO
      disp.clear();
      disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
     // disp.clear();
      //disp.displayByte(_H, _E, _L, _L);
    #endif  
  #endif   
}


float dispayValue = 0;
float dispayValueOld = 0;
/**
 * Отобразить на экране значение температуры или настройки
 * @param bool   setupMode true-отображение значение настройки, иначе - температуры
 */
void displayShow(){

  #if DEBUG
    //Serial.println( "void displayShow();" );
  #endif  
  /*
   * В режиме настройки отображать значение от энкодера,
   * а в обычном режиме - значение темпераутры
  */
  if( getSystemMode() == SYSTEM_MODE_SETUP ) dispayValue =  endcoderGetValue();
  else  dispayValue =  getTemp();
  
  #if DEBUG
   // Serial.print( "dispayValue=" );Serial.print( dispayValue );
   // Serial.print( "; dispayValueOld=" );Serial.print(dispayValueOld );
  //  Serial.println( "" );
  #endif    
  //даем команду модулю на перерисовку значения только тогда,
  //когда значение изменилось, иначе пусть продолжается
  //отображаться ранее записанное
  if( (int)dispayValue != (int)dispayValueOld ){ //!!! Обратить внимание! Мы сравниваем float, они м.б. не равны где нибудь в 8 после запятой знаке, а по факту они равны
    
   //замечание, индикатор не может отображать менее -999 и более 9999
      
    #if USE_DISPLAY
      #if INARDUINO
        //скорее всего это лишнее disp.clear();
        //При отсутствии готовности датчика температуре и режима отображения температуры
        //показать код ошибки
        if( !readyByTempSensors && getSystemMode() != SYSTEM_MODE_SETUP ) disp.displayByte( _N,_o,_empty, _F );
        else disp.displayInt( (int)dispayValue);
       #endif  
    #endif 
        
    #if DEBUG
      Serial.print( "display: " );Serial.print(  dispayValue );Serial.println( "\r\n" );
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
   // Serial.print( "float endcoderGetValue() --> " ); Serial.println(encValue);
  #endif  
  return(encValue);  
}

/*
 * Установить текущее значение настраиваемой темпераутры
 */
void endcoderSetValue(float value ){
  #if DEBUG
    Serial.print( "void endcoderSetValue( value ) --> " ); Serial.println(value);
  #endif  
  encValue = value;  
}


/*
 * Обработчик энкодера, который обрабатывает события железяки
 */
void encoderHandler(){
  #if DEBUG
    //Serial.println( "void encoderHandler()\r\n" );
  #endif  
  int encValueOld = endcoderGetValue();

  #if USE_ENCODER
    #if INARDUINO
      enc1.tick();

      if (enc1.isRight()) endcoderSetValue( endcoderGetValue() + 1 ) ;       // если был поворот направо, увеличиваем на 1
      if (enc1.isLeft())  endcoderSetValue( endcoderGetValue() - 1 ) ;      // если был поворот налево, уменьшаем на 1
  
      if (enc1.isRightH()) endcoderSetValue( endcoderGetValue() + 5 ) ;   // если было удержание + поворот направо, увеличиваем на 5
      if (enc1.isLeftH()) endcoderSetValue( endcoderGetValue() - 5 );  // если было удержание + поворот налево, уменьшаем на 5  


       //ограничение значений разумными пределами
       if( endcoderGetValue()<15) endcoderSetValue( 15 );
       if( endcoderGetValue()>70) endcoderSetValue( 70 );

      if (enc1.isTurn()) {       // если был совершён поворот (индикатор поворота в любую сторону)
        setSystemMode( SYSTEM_MODE_SETUP );
        #if DEBUG
          Serial.print("encoder zavershili krutit, znachenie=");Serial.println(endcoderGetValue());  // выводим значение при повороте
        #endif  
        setTargetTemp( endcoderGetValue() );
      }
    #else  
     //требуется иммитация энкодера 
      int sign = rand() % 10;
      int value = rand() % 70;
      if( sign > 8 ){
          setTargetTemp( value );
          setSystemMode( SYSTEM_MODE_SETUP );
          endcoderSetValue( value );
      }    
    #endif  
  #endif  

 if( endcoderGetValue() != encValueOld ) displayShow();   
}

float tempValueOld = 0;

void updateTemperature(){
    float tempValue = getTemp();
  #if DEBUG
    //Serial.println( "void updateTemperature()\r\n" );
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
         tempValue = TEMPVALUE_NOCONNECT;
       }
     }else{
      //Датчие температуры отсутсвует или неправильно подключен
      //значние будет таким, что управление нагревателем д. отключиться из-за ошибочной температуры
       tempValue = TEMPVALUE_NOTPRESENT;
     } 
        
    #else  
      tempValue = rand() % 70;
    #endif
    
    #if DEBUG
      Serial.print("Real temperature: ");
      Serial.println(tempValue);
    #endif  
    
  #else  
  /*
      int sign = rand() % 10;
      int value = rand() % 5;
      if( sign > 4 ) value = -value;
      if( tempValue < 0 || tempValue > 70 ) tempValue = 20;
      tempValue = tempValue + value;

    
    #if DEBUG
      Serial.print("Virtual temperature: ");
      Serial.println(tempValue);
    #endif  
  */  
  #endif   
  if( tempValueOld != tempValue ) setTemp( tempValue );
  tempValueOld = tempValue;
}



void initSystemMode(){
  setSystemMode( SYSTEM_MODE_NORMAL );
}


/**
 * Получить текущий режим системы
 */
byte getSystemMode(){
 return( sytemMode );
}  


/**
 * Установить текущий режим системы.
 * Сброс таймера австосмены режима (при бездействии пользователя)
 */
void setSystemMode( byte value ){
 sytemMode = value;
 sytemModeTimeoutCur = millis();
 if( sytemMode > SYSTEM_MODE_NORMAL ) readyBySetupMode = false;
 else readyBySetupMode = true;
 
}  



/**
 * Обработчик состояние системы.
 * Вызывается циклично
 */
void systemModeHandler(){
  if( sytemMode > SYSTEM_MODE_NORMAL ){
    //таймаут перевода режима системы в обычный с измерением темперетура
      printf("%d - %d = %d \r\n", millis(),sytemModeTimeoutCur, (millis() - sytemModeTimeoutCur)  );
    if (millis() - sytemModeTimeoutCur > sytemModeTimeoutMax) {
      setSystemMode( SYSTEM_MODE_NORMAL ); 
      sytemModeTimeoutCur = millis();
    }    
  }
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
    displayInit();
  initSystemMode();
  relayInit();

  endcoderInit();
  
  
  #if USE_DS18B20
   sensors.begin();
   tempSensIsPresent = sensors.getDS18Count() == 1; //sensors.getDeviceCount == 1;
   
   #if DEBUG
     if( !tempSensIsPresent ){  
       Serial.println("Termodatchik otsutstvuet, rabota nevozmojna. ");
     }  
    #endif  

  
  if (tempSensIsPresent && sensors.getAddress(tempDevAddress, 0)){ 
     tempSensAddressPresent = true;
  }else{
    tempSensAddressPresent = false; 
   #if DEBUG
      Serial.println("Unable to find address for Device 0"); 
    #endif       
  }
  #else      
   tempSensIsPresent = true;
   tempSensAddressPresent = true;

  #endif


   

  curtimeoutUpdateTemper = millis();

}


/**************************************************
 * 
 */
void loop(){
  updateAllowedSystem();
  displayShow();
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

      systemModeHandler();
      

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
