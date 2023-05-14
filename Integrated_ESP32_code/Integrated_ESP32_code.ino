/*
    Integrated code for Team 16's RootBound ECE Capstone Project
    This contains the code to be run on the ESP32 "In-pot" device which gathers data from 
    a force sensative resistor (FSR) and an Adafruit STEMMA soil sensor
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "Adafruit_seesaw.h"
//#include <gpio.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define FORCE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MOIST_CHARACTERISTIC_UUID "f2c30819-4734-4e9f-9185-8130eed7a1a7"
#define TEMPERATURE_CHARACTERISTIC_UUID "d0fa78d1-b479-456b-8158-74a943672883"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */

enum modes {
  fullOp = 0,
  sleepCycle,
  serialData,
};

static const enum modes deviceMode = fullOp;

RTC_DATA_ATTR int bootCount = 0;
Adafruit_seesaw ss;
bool seesawActive = false;

BLECharacteristic *forceCharacteristic;
BLECharacteristic *moistureCharacteristic;
BLECharacteristic *temperatureCharacteristic;

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

float getForceReading(){
  float voltageValue= 0.0;
  unsigned long startTime = 0;
  unsigned long stopTime = 0;

  while (digitalRead(26) != LOW) {}
  while(digitalRead(26) == LOW) {}
  
  startTime = micros(); //26 went high, so set start time

  while(digitalRead(26) != LOW) {}
  while(digitalRead(26) == LOW) {}
  while(digitalRead(26) != LOW) {}
  while(digitalRead(26) == LOW) {}

  stopTime = micros(); //set stop time because low detected, this is 2 cycles
  unsigned long elapsedTime = stopTime - startTime;
  elapsedTime = elapsedTime * 0.5; //there are two cycles so divide by 2

  return 1000000.0 / elapsedTime;
}

void disablePins(){
  //Turning off GPIO to reduce sleep power consumption
  gpio_reset_pin(GPIO_NUM_0);
  gpio_reset_pin(GPIO_NUM_2);
  gpio_reset_pin(GPIO_NUM_4);
  gpio_reset_pin(GPIO_NUM_12);
  gpio_reset_pin(GPIO_NUM_13);
  gpio_reset_pin(GPIO_NUM_14);
  gpio_reset_pin(GPIO_NUM_15);
  gpio_reset_pin(GPIO_NUM_25);
  gpio_reset_pin(GPIO_NUM_26);
  gpio_reset_pin(GPIO_NUM_27);
  gpio_reset_pin(GPIO_NUM_32);
  gpio_reset_pin(GPIO_NUM_33);
  gpio_reset_pin(GPIO_NUM_34);
  gpio_reset_pin(GPIO_NUM_35);
  gpio_reset_pin(GPIO_NUM_36);
  gpio_reset_pin(GPIO_NUM_37);
  gpio_reset_pin(GPIO_NUM_38);
  gpio_reset_pin(GPIO_NUM_39);
}

void enterDeepSleep(){
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup to Deep Sleep for " + String(TIME_TO_SLEEP) +
  " Seconds");

  //sleep(TIME_TO_SLEEP);
  Serial.println("Entering Deep Sleep");
  Serial.flush(); 

  disablePins();
  esp_deep_sleep_start();
}

void initSoilSensor(){
 if (!ss.begin(0x36)) {
    Serial.println("ERROR! seesaw not found");
    seesawActive = false;
  } else {
    Serial.print("seesaw started! version: ");
    Serial.println(ss.getVersion(), HEX);
    seesawActive = true;
  }
}

void setup() {
  Serial.begin(115200);
  bootCount++;
  Serial.print("Boot Number: ");
  Serial.println(bootCount);
  print_wakeup_reason();
  Serial.println("Initilizing BLE");

  BLEDevice::init("RootBound In-pot Device");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  forceCharacteristic = pService->createCharacteristic(
                                         FORCE_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE_NR
                                       );
  temperatureCharacteristic = pService->createCharacteristic(
                                         TEMPERATURE_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE_NR
                                       );
  moistureCharacteristic = pService->createCharacteristic(
                                         MOIST_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE_NR
                                       );

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("BLE Characteristics defined");

  switch(deviceMode){
    case sleepCycle:
      break;
    default:
      pinMode(26, INPUT); //set GPIO 26 to input as the FSR circuits reads an analog square wave
      initSoilSensor();
      break;
  }
}

void loop() {
  //Switch based on the mode needs to do while awake
  switch(deviceMode){
    case sleepCycle:
      Serial.println("Waiting");
      sleep(5);
      break;
    default:
      float frequency = getForceReading();
      forceCharacteristic->setValue(String(frequency).c_str());
      Serial.println("Force reading: " + String(frequency));

      if(seesawActive){
        float tempC = ss.getTemp();
        temperatureCharacteristic->setValue(String(tempC).c_str());
        Serial.print("Temperature: "); Serial.print(tempC); Serial.println("*C");

        uint16_t capread = ss.touchRead(0);
        moistureCharacteristic->setValue(String(capread).c_str());
        Serial.print("Capacitive: "); Serial.println(capread);
      }
      break;      
  }
  
  //Switch based on how the mode needs to cycle, either loop after a sleep period or enterDeepSleep() and start back from setup()
  switch(deviceMode){
    case serialData:
      sleep(1);
      break;
    case fullOp:
      Serial.println("Waiting for Base Sation to clear characteristics...");
      if(seesawActive){
        while(atoi(moistureCharacteristic->getValue().c_str()) != 0 || atoi(forceCharacteristic->getValue().c_str()) != 0){
          delay(1);
        }
      } else {
        while(atoi(forceCharacteristic->getValue().c_str()) != 0){
          delay(1);
        }
      }
      sleep(1);
      enterDeepSleep();
      break;
    default:
      sleep(10);
      enterDeepSleep();
      break;
  }
}