
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
 * Библиотек DS18B20 от Gyver
 * https://github.com/GyverLibs/microDS
 * 18B20/archive/refs/heads/main.zip
 * Эта библотек позволяет подключать несколько датчиков на РАЗНЫЕ пины.
 * Что дает плюс при замене вышедшего датчика. Просто заменил и не нужно узнавать его адрес
 * 
 * Датчик температуры DS18B20
 * Подключение к PIN7    контроль температуры потока воздуха от нагревателя
 * Возможно добавлю датчики для доп. контроля потока воздуха. Лучше перестраховаться с конролем, чем огонь
 * 
 * Твердотельное реле SSR-10 D
 * Подключение к PIN8
 * 
 * Твердотельное реле управляет нагревателем.
 * Вентилятор должен вращаться всегда
 * 
 * 
  */

  
//Определяем, где работает программа: на Ардуине?, тогда true
#define INARDUINO   true
/**
 * Уровень отладочной информации
 * 0 - нет ничего
 * 1 - только ошибки
 * 2 - плюс предупреждения, плюс отчет работы системы
 * 3 - плюс простая отладочная информация
 * 4 - плюс подробдная отладочная инфорация
 */
#define DEBUG       1
#define USE_DS18B20 true
#define USE_RELE    true
#define USE_DISPLAY true
#define USE_ENCODER true

#if INARDUINO
  
#if USE_DS18B20
  ///#include <OneWire.h>
  #include <microDS18B20.h>
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
  #define PIN_T_SENSOR1  7
  #define PIN_T_SENSOR2  9

  #define DS_TEMP_TYPE     int  // целочисленный тип данных для температуры
  #define DS_CHECK_CRC     true // включить проверку подлинности принятых данных
  #define DS_CRC_USE_TABLE true // использовать готовую таблицу контрольной суммы - значительно быстрее, +256 байт flash

  MicroDS18B20<PIN_T_SENSOR1> sensor1;
  //MicroDS18B20<PIN_T_SENSOR2> sensor2;
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
//#define TEMPVALUE_NOCONNECT  -888
//Занчение температуры при отсутсвии датчика или невозможности прочитать его внутренний адрес
//#define TEMPVALUE_NOTPRESENT -777
  
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
float tempTargetValue = 40;


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
  readyByTempSensors = getTemp()>5 && getTemp()<70 ;
  
  //готовность (разрешение) к включению нагревателя
  readyHeater = readyByTempSensors && readyBySetupMode;
}

void relayInit(){
  #if DEBUG >= 3
    Serial.print( "void relayInit()\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG >= 3
      Serial.println( "Инициализация GyverRelay\r\n" );
    #endif  
    #if INARDUINO
      pinMode(PIN_HEATER, OUTPUT);             // пин реле
      digitalWrite(PIN_HEATER, LOW);
      regulator.setpoint    = getTargetTemp();// установка температуры, к которой стремиться система управления
      regulator.hysteresis  = 1;//5;              // ширина гистерезиса
      regulator.k           = 1;//0.5;            // коэффициент обратной связи (подбирается по факту)
      //regulator.dT        = 1000;            // установить время итерации для getResultTimer
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
  #if DEBUG >= 3
    Serial.print( "void relaySetCurrentValue(float " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG >= 3
      Serial.print( "GyverRelay input=" );Serial.print( value ); Serial.println( "\r\n" );
    #endif  
    #if INARDUINO
      regulator.input    = value;
    #endif  
  #endif  
}

void relaySetTargetValue( float value ){
  #if DEBUG >= 3
    Serial.print( "void relaySetTargetValue( float  " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  #if USE_RELE
    #if DEBUG >= 3
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
  #if DEBUG >= 3
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
  #if DEBUG >= 3
    Serial.print( "float getTargetTemp()\r\n" );
  #endif  
  return( tempTargetValue );
}


/**
 * Установить текущую температуру
 * @param float value значение
 */
void setTemp(float value){
  #if DEBUG >= 4
    Serial.print( "void setTemp(float  " );Serial.print( value ); Serial.print( ")" ); Serial.println( "\r\n" );
  #endif  
  tempValue    = value;
  relaySetCurrentValue( value );
}

/**
 * Получить текущую температуру
 */
float getTemp(){
  #if DEBUG >= 4
    Serial.print( "float getTemp()\r\n" );
  #endif  
  return( tempValue );
}


/*
 * Инициализация дисплея отображения
 * текущей температцуры и настройки температуры
 */
void displayInit(){
  #if DEBUG >= 3
    Serial.print( "void displayInit()\r\n" );
  #endif  
  #if USE_DISPLAY
    #if DEBUG >= 3
      Serial.println( "Inicializaciya displeya\r\n" );
    #endif  
    #if INARDUINO
      disp.clear();
      disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
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

  #if DEBUG >= 4
    Serial.println( "void displayShow();" );
  #endif  
  /*
   * В режиме настройки отображать значение от энкодера,
   * а в обычном режиме - значение темпераутры
  */
  if( getSystemMode() == SYSTEM_MODE_SETUP ) dispayValue =  endcoderGetValue();
  else  dispayValue =  getTemp();
  
  #if DEBUG >= 4
    Serial.print( "dispayValue=" );Serial.print( dispayValue );
    Serial.print( "; dispayValueOld=" );Serial.print(dispayValueOld );
    Serial.println( "" );
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
        else {
          //disp.displayInt( (int)dispayValue);
          //Надо обозначить режим установки, чтобы были визуальные различия
          //по-простому, хоть и нелогично
          if( getSystemMode() == SYSTEM_MODE_SETUP ) disp.point(1);
          else disp.point(0);
          
          disp.displayInt( (int)dispayValue);  
        }
       #endif  
    #endif 
        
    #if DEBUG >= 3
      Serial.print( "display: " );Serial.print(  dispayValue );Serial.println( "\r\n" );
    #endif  

    dispayValueOld = dispayValue;
  }
  
}


/*
 * Инициализация энкодера для настройки температуры
 */
void endcoderInit(){
  #if DEBUG >= 3
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
  #if DEBUG >= 4
    Serial.print( "float endcoderGetValue() --> " ); Serial.println(encValue);
  #endif  
  return(encValue);  
}

/*
 * Установить текущее значение настраиваемой темпераутры
 */
void endcoderSetValue(float value ){
  #if DEBUG >= 3
    Serial.print( "void endcoderSetValue( value ) --> " ); Serial.println(value);
  #endif  
  encValue = value;  
}


/*
 * Обработчик энкодера, который обрабатывает события железяки
 */
void encoderHandler(){
  #if DEBUG >= 4
    Serial.println( "void encoderHandler()\r\n" );
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
        #if DEBUG >= 3
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
  #if DEBUG >= 4
    Serial.println( "void updateTemperature()\r\n" );
  #endif

  //Замеры темперауры производить только в обычно режиме (не в режиме настройки системы)
  if( getSystemMode() == SYSTEM_MODE_NORMAL ){   
  #if USE_DS18B20
   // запрос температуры  
   sensor1.requestTemp();
   //sensor2.requestTemp();
   // вместо delay используй таймер на millis()
  delay(1000);  
 // tempValue = sensor1.getTemp(); 
//  tempValue2 = sensor2.getTemp(); 
 
   // прпочитали сырое значение
  uint16_t rawVal = sensor1.getRaw();
  if( rawVal == 0){
  //ошибка
   tempValue = TEMPVALUE_ERROR;
  }else{
  tempValue = DS_rawToFloat(rawVal);
  }
    
    #if DEBUG >= 4
      Serial.print("Real temperature: ");
      Serial.println(tempValue);
    #endif  
 
  #endif 
  } 
 //обновляем температуру (и корректируем систему управления) только при фактическом изменении
 //Внимание! сравниваются float'ы   float 1 не всегда равен float 1, т.к. где нибудь в 8-м знаке будет 1
  if( tempValueOld != tempValue ) setTemp( tempValue );
  tempValueOld = tempValue;


   //Отобразить отчет работы системы для внешней отладки регулировки управления нагревателем
  #if DEBUG >= 1
    Serial.print( millis() );
    Serial.print( ";" );
    Serial.print( getTemp() );
    Serial.print( ";" );
    Serial.print( getTargetTemp() );    
    Serial.print( ";" );
    Serial.print( regulator.getResultTimer() );    
    Serial.println( ";" );
  #endif       

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
  #if DEBUG > 0
    Serial.begin(115200);
  #endif  
  #if DEBUG >= 3
    Serial.println("Starting");
  #endif  
    displayInit();
  initSystemMode();
  relayInit();

  endcoderInit();
  
  
  #if USE_DS18B20
   sensor1.setResolution(12);  // разрешение [9-12] бит. По умолч. 12 
   //sensor2.setResolution(12);  // разрешение [9-12] бит. По умолч. 12 
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
   //управление нагревателем по таймеру, если есть разрешение
    if( readyHeater){
        digitalWrite(PIN_HEATER, regulator.getResultTimer() );  // отправляем на реле
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
