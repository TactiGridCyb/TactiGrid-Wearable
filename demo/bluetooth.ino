#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "LilyGoLib.h"


int scanTime = 5;
BLEScan *pBLEScan;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

void setup() {
    Serial.begin(115200);
    watch.begin(&Serial);

    Serial.println("Scanning...");

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); // Uses more battery, but faster
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99); // Window of scanning inside the interval itself, can adjust to save more battery between scans

    BLEScanResults foundDevices = pBLEScan->start(scanTime);
    Serial.print("Devices found: ");
    Serial.println(foundDevices.getCount());
    Serial.println("Scan done!");
    pBLEScan->clearResults();
}

void loop() {
    
}