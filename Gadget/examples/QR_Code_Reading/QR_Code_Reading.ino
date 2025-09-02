//  Juniper Robotics QR Code Reading

// QR Code reading based on the following:
/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-qr-code-reader-scanner-arduino/
*********/

#include <Arduino.h>
#include <ESP32QRCodeReader.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h";
#include <DifferentialSteering.h>
#include "dashboard.h"

/* --- USER ADJUSTED PARAMETERS START --- */

// Set your access point network credentials
const char* ssid = "Gadget_SSID";               // Change "Gadget###" to match label on Gadget

// Speed at which Gadget will rotate
byte rotate_speed       =   35;                 // Range: (30 to 50)    

// How long in milliseconds to rotate (1 second = 1000 milliseconds)
int turn_duration =         2000;               // Range: (10 to 5000)

/* --- USER ADJUSTED PARAMETERS END --- */
 
ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

// Mutex thread sync variables
volatile bool qrAvailable = false;
char qrPayload[256];
SemaphoreHandle_t qrMutex;


DifferentialSteering DiffSteer;
int fPivYLimit = 32;
int idle = 0;

// Define vars to hold time for turn counter
unsigned long start_turn_time = 0;
unsigned long current_turn_time = 0;

camera_fb_t * fb = NULL;



#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

using namespace websockets;
WebsocketsServer WSserver;
AsyncWebServer webserver(80);



void differential_steer(int XValue, int YValue) {
  // Outside no action limit joystick
  if (!((XValue > -5) && (XValue < 5) && (YValue > -5) && (YValue < 5)))
  {
    DiffSteer.computeMotors(XValue, YValue);
    int leftMotor = DiffSteer.computedLeftMotor();
    int rightMotor = DiffSteer.computedRightMotor();

    // map motor outputs to your desired range
    leftMotor = map(leftMotor, -127, 127, 3276, 6554); // 0-180
    rightMotor = map(rightMotor, -127, 127, 6554, 3276); // 0-180 reversed
    ledcWrite(2, rightMotor); // channel, value
    ledcWrite(4, leftMotor); // channel, value
  } else
  {
    ledcWrite(2, idle);
    ledcWrite(4, idle); // channel, value
  }
}

void onQrCodeTask(void *pvParameters) {
  struct QRCodeData qrCodeData;

 while (true) {
    if (reader.receiveQrCode(&qrCodeData, 100)) {
      if (qrCodeData.valid) {
        Serial.print("Detected QR Code ");
        Serial.println((const char *)qrCodeData.payload);
        if (xSemaphoreTake(qrMutex, portMAX_DELAY)) {
          strncpy(qrPayload, (const char *)qrCodeData.payload, sizeof(qrPayload) - 1);
          qrPayload[sizeof(qrPayload) - 1] = '\0'; // ensure null-termination
          qrAvailable = true;
          xSemaphoreGive(qrMutex);
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


void setup() {

  Serial.begin(115200);

  // Wi-Fi connection
  Serial.print("Creating WiFi AP for ");
  Serial.println(ssid);
  WiFi.softAP(ssid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  Serial.println("");
  Serial.println("WiFi connected");


  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  webserver.begin();
  WSserver.listen(82);


  DiffSteer.begin(fPivYLimit);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // Ai-Thinker: pins 2 and 12
  ledcSetup(2, 50, 16); //channel, freq, resolution
  ledcAttachPin(12, 2); // pin, channel
  ledcSetup(4, 50, 16); //channel, freq, resolution
  ledcAttachPin(2, 4); // pin, channel

 

  reader.setup();
  Serial.println("Setup QRCode Reader");

  reader.beginOnCore(1);
  Serial.println("Begin on Core 1");

  qrMutex = xSemaphoreCreateMutex();
  xTaskCreate(onQrCodeTask, "onQrCode", 4 * 1024, NULL, 4, NULL);

  // Check that user defined values stay within bounds
  if (rotate_speed > 50)
    {
      rotate_speed = 50;
    }
  else if (rotate_speed < 30)
    {
      rotate_speed = 30;
    }
  else if (rotate_speed < 0)
    {
      rotate_speed = 0;
    }

  if (turn_duration > 5000)
    {
      turn_duration = 5000;
    }
  else if (turn_duration < 10)
    {
      turn_duration = 10;
    }
  else if (turn_duration < 0)
    {
      turn_duration = 0;
    }


}


void loop() {
  // Set back to zero to help with lag/loss of connection
  differential_steer(0, 0);
  
  auto client = WSserver.accept();
  while (client.available()) {
    client.poll();
  
    if (qrAvailable) {
      if (xSemaphoreTake(qrMutex, portMAX_DELAY)) {
        Serial.println(qrPayload);
        // Send the current QR Code detected to the UI
        client.send("qr:" + String(qrPayload));
        qrAvailable = false;
        
        if (String(qrPayload) == "Left"){
          start_turn_time = millis();
          current_turn_time = millis();
          while ((current_turn_time - start_turn_time) < turn_duration){ 
            differential_steer(-rotate_speed, 0);
            // Serial.println("Turning Left");
            client.send("op:Turning Left");
            current_turn_time = millis();
          }
        }
        else if (String(qrPayload) == "Right"){
          start_turn_time = millis();
          current_turn_time = millis();
          while ((current_turn_time - start_turn_time) < turn_duration){ 
            differential_steer(rotate_speed, 0);
            // Serial.println("Turning Right");
            client.send("op:Turning Right");
            current_turn_time = millis();
          }

        }
        else{
          Serial.println("Invalid Operation");
          client.send("op:Invalid QR Code Detected");
          differential_steer(0, 0);
        }
        xSemaphoreGive(qrMutex);
      }
    }
    else{
      client.send("op:Awaiting QR Code Instructions");
      differential_steer(0, 0);
    }


    client.send("text_0: Configured Parameters ");
    client.send("text_1:" + String(turn_duration));
    client.send("text_2:" + String(rotate_speed));
  }
}