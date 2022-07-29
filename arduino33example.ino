
#include <Arduino_LSM9DS1.h>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include "model.h"
//____________________________________________________________________
// Hey! Change Me!
// Go to the file that you downloaded from Google's Tiny Motion Trainer. 
// Find #define MOTION_THRESHOLD 0.2 copy until (including) NUM_SAMPLES. 
//It should be 3 lines
//Past it here
//______________________________________________________________________





//____________________________________________________________________
// Hey! Change Me!
// Go to the file that you downloaded from Google's Tiny Motion Trainer. 
// Find Const char *GESTURES[] = .... and copy until(including) '};'. 
//It should be 3 lines
//Past it here
//______________________________________________________________________



#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

bool isCapturing = false;

// Num samples read from the IMU sensors
// "Full" by default to start in idle
int numSamplesRead = 0;


//==============================================================================
// TensorFlow variables
//==============================================================================

// Global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

// Auto resolve all the TensorFlow Lite for MicroInterpreters ops, for reduced memory-footprint change this to only
// include the op's you need.
tflite::AllOpsResolver tflOpsResolver;

// Setup model
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Create a static memory buffer for TensorFlow Lite for MicroInterpreters, the size may need to
// be adjusted based on the model you are using
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize];


//==============================================================================
// Setup / Loop
//==============================================================================


#include <ArduinoBLE.h>

BLEService weatherService("180F");
BLEStringCharacteristic weatherChar("2A19", BLERead | BLENotify, 30);
String oldSensorLevel = "0,0,0,0,0,0,0";  // last sensor readings
long previousMillis = 0;  // last time the sensor level was read, in ms


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
 




  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("WeatherMonitor");
  BLE.setAdvertisedService(weatherService); // add the service UUID
  weatherService.addCharacteristic(weatherChar); // add the battery level characteristic
  BLE.addService(weatherService); // Add the battery service
  weatherChar.writeValue(oldSensorLevel); // set initial value for this characteristic

  BLE.advertise();
  Saerial.println("Bluetooth® device active, waiting for connections...");




  // Initialize IMU sensors
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // Print out the samples rates of the IMUs
  Serial.print("Accelerometer sample rate: ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  Serial.print("Gyroscope sample rate: ");
  Serial.print(IMU.gyroscopeSampleRate());
  Serial.println(" Hz");

  Serial.println();

  // Get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }

  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);

  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();

  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}


void updateSensorLevel(int R) {
  String sensorLevel = String(R);
  Serial.print("Sensor Level % is now: "); // print sensor readings
  Serial.println(sensorLevel);
  weatherChar.writeValue(sensorLevel);  // and update the weather level characteristic
}

void loop() {
  // Variables to hold IMU data
  float aX, aY, aZ, gX, gY, gZ;

  // Wait for motion above the threshold setting
  while (!isCapturing) {
    BLEDevice central = BLE.central();
    if (central.connected()) {

    } else {
      long currentMillis = millis();
      // if 200ms have passed, check the sensor level:
      if (currentMillis - previousMillis >= 500) {
        previousMillis = currentMillis;


        updateSensorLevel(75);
        Serial.println("");
      }
    }
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {

      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);

      // Sum absolute values
      float average = fabs(aX / 4.0) + fabs(aY / 4.0) + fabs(aZ / 4.0) + fabs(gX / 2000.0) + fabs(gY / 2000.0) + fabs(gZ / 2000.0);
      average /= 6.;

      // Above the threshold?
      if (average >= MOTION_THRESHOLD) {
        isCapturing = true;
        numSamplesRead = 0;
        break;
      }
    }
  }

  while (isCapturing) {

    // Check if both acceleration and gyroscope data is available
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {

      // read the acceleration and gyroscope data
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);

      // Normalize the IMU data between -1 to 1 and store in the model's
      // input tensor. Accelerometer data ranges between -4 and 4,
      // gyroscope data ranges between -2000 and 2000
      tflInputTensor->data.f[numSamplesRead * 6 + 0] = aX / 4.0;
      tflInputTensor->data.f[numSamplesRead * 6 + 1] = aY / 4.0;
      tflInputTensor->data.f[numSamplesRead * 6 + 2] = aZ / 4.0;
      tflInputTensor->data.f[numSamplesRead * 6 + 3] = gX / 2000.0;
      tflInputTensor->data.f[numSamplesRead * 6 + 4] = gY / 2000.0;
      tflInputTensor->data.f[numSamplesRead * 6 + 5] = gZ / 2000.0;

      numSamplesRead++;

      // Do we have the samples we need?
      if (numSamplesRead == NUM_SAMPLES) {

        // Stop capturing
        isCapturing = false;

        // Run inference
        TfLiteStatus invokeStatus = tflInterpreter->Invoke();
        if (invokeStatus != kTfLiteOk) {
          Serial.println("Error: Invoke failed!");
          while (1);
          return;
        }

        // Loop through the output tensor values from the model
        int maxIndex = 0;
        float maxValue = 0;
        for (int i = 0; i < NUM_GESTURES; i++) {
          float _value = tflOutputTensor->data.f[i];
          if (_value > maxValue) {
            maxValue = _value;
            maxIndex = i;
          }

        }



        Serial.print(maxIndex);
        Serial.println();

        updateSensorLevel(maxIndex);


        // Add delay to not double trigger
        delay(CAPTURE_DELAY);
      }
    }
  }
}
