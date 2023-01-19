/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.
   A connect hander associated with the server starts a background task that performs notification
   every couple of seconds.
*/
#include <Arduino.h>
#include <string>
#include <stdlib.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "c48e6067-5295-48d3-8d5c-0395f61792b1"
#define CHARACTERISTIC_UUID "c48e6067-5295-48d3-8d5c-0395f61792b2"

// rotary encoder
long int rotValue=0, swValue=0;
float rotations = 0;
uint8_t state=0;
float lengthCm;
uint32_t lengthMm = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      rotValue=0, swValue=0;

    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



#define ROTARY_PINA 32
#define ROTARY_PINB 25
#define ROTARY_PINSW 21

#define ONE_ROTATION 74

portMUX_TYPE gpioMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR isrAB() {
   uint8_t s = state & 3;

  portENTER_CRITICAL_ISR(&gpioMux);
    if (digitalRead(ROTARY_PINA)) s |= 4;
    if (digitalRead(ROTARY_PINB)) s |= 8;
    switch (s) {
      case 0: case 5: case 10: case 15:
        break;
      case 1: case 7: case 8: case 14:
        rotValue++; break;
      case 2: case 4: case 11: case 13:
        rotValue--; break;
      case 3: case 12:
        rotValue += 2; break;
      default:
        rotValue -= 2; break;
    }
    state = (s >> 2);
   portEXIT_CRITICAL_ISR(&gpioMux);
 
}


void IRAM_ATTR isrSWAll() {

 portENTER_CRITICAL_ISR(&gpioMux);
 swValue++;
 portEXIT_CRITICAL_ISR(&gpioMux);

}


void setup() {
  pinMode(ROTARY_PINA, INPUT_PULLUP);
  pinMode(ROTARY_PINB, INPUT_PULLUP);
  pinMode(ROTARY_PINSW, INPUT_PULLUP);

  attachInterrupt(ROTARY_PINA, isrAB, CHANGE);
  attachInterrupt(ROTARY_PINB, isrAB, CHANGE);
  attachInterrupt(ROTARY_PINSW, isrSWAll, CHANGE);

  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("MyESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        rotations = (float) rotValue / ONE_ROTATION;
        lengthCm = fabs(rotations * 2 * 3.14 * 2.5);
        lengthMm = lengthCm * 10;
        Serial.println(lengthMm);
        pCharacteristic->setValue(lengthMm);
        pCharacteristic->notify();
        delay(100); // bluetooth stack will go into congestion, if too many packets are sent
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}