/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include "BLEDevice.h"
#include <WiFi.h>
#include <WIFIClient.h>
#include <HTTPClient.h>

//____________________________________________________________________
// Hey! Change Me!
// 1. Enter your 2.4 ghz wifi network name in place of ‘NetworkName’ below
// 2. Enter your password for your network in place of ‘NetworkPassword’ below
//______________________________________________________________________

const char* ssid = "NetworkName";
const char* password = "NetworkPassword";

//____________________________________________________________________
// Hey! Change Me!
// This part enables you to perform the same action to both turn on a device and turn 
// off the device
// 
// 1. Enter the names of the devices you would like to turn on or off instead of light or fan in // lightstatus and fanstatus.
//______________________________________________________________________
String lightstatus = "off";
String fanstatus = "off";

//BLE Server name (the other ESP32 name running the server sketch)
#define bleServerName "WeatherMonitor"

/* UUID's of the service, characteristic that we want to read*/
// BLE Service
static BLEUUID bmeServiceUUID("180f");

// BLE Characteristics
// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID("2A19");

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress *pServerAddress;
 
//Characteristicd that we want to read
static BLERemoteCharacteristic* humidityCharacteristic;

//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};





//Variables to store temperature and humidity
char* humidityChar;

//Flags to check whether new temperature and humidity readings are available
boolean newHumidity = false;

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remove BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");
 
  //Assign callback functions for the Characteristics
 
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  return true;
}

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) { //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};
 
//When the BLE Server sends a new temperature reading with the notify property
 int sendRequest(String link) {
    HTTPClient http;
     http.begin(link.c_str());
      // Send HTTP GET request
      int httpResponseCode = http.GET();
     Serial.print(httpResponseCode);
      // Free resources
      return httpResponseCode;    // http.end();    
}

//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  humidityChar = (char*)pData;
  newHumidity = true;
 String myString = String(humidityChar);

 myString = myString.charAt(0);
Serial.println(myString);


//____________________________________________________________________
// Hey! Change Me!
// First: I hope you can code a little bit
// 
// Below you can see that myString is received from the arduino wand. If myString equals 0, 
// then the first gesture you trained in Google’s Tiny Motion Trainer was triggered. If myString
// equals 1, then the second motion you trained was triggered and so on.

// Within those if statements, you can see another if statement, which is the status of the device, // which we created in the last ‘change me’. This will keep track of whether or not the device is
 // on or off.
//
// You need to change this code to work for the gestures you have created, because this could
// be so dynamic, you are on your own. ______________________________________________________________________


 if(myString == "0"){
  if(lightstatus == "off"){
    Serial.println("the light was off");
    lightstatus = "on";
    while(sendRequest("WaitForStepFive") == -1) {
      delay(2);
      Serial.println(" Failed, trying again");
    }
   
  } else if(lightstatus == "on") {
    Serial.println("the light was on");
    lightstatus = "off";
   while(sendRequest("WaitForStepFive") == -1) {
      delay(2);
      Serial.println(" Failed, trying again");
    }
  }   
}

 if(myString == "1"){
  if(fanstatus == "off"){
    Serial.println("the fan was off");
    fanstatus = "on";
    while(sendRequest("WaitForStepFive") == -1) {
      delay(2);
      Serial.println(" Failed, trying again");
    }
   
  } else if(fanstatus == "on") {
    Serial.println(" the fan was on");
    fanstatus = "off";
   while(sendRequest("WaitForStepFive") == -1) {
      delay(2);
      Serial.println(" Failed, trying again");
    } 
  } 
       
}
}

//function that prints the latest sensor readings in the OLED display

void setup() {

 
  //Start serial communication
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");





WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

 
  //Init BLE device
  BLEDevice::init("");
 
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      //Activate the Notify property of each Characteristic
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
    }
    doConnect = false;
  }
  if (newHumidity){
    newHumidity = false;
  }
  delay(1000); // Delay a second between loops.
}

