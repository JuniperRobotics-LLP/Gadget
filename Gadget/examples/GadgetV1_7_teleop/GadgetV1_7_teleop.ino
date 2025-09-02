#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h";
#include <DifferentialSteering.h>
#include "dashboard.h"

DifferentialSteering DiffSteer;
int fPivYLimit = 32;
int idle = 0;

// Set your access point network credentials
const char* ssid = "Gadget_SSID"; //change "Gadget###" to match label on Gadget

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

camera_fb_t * fb = NULL;

using namespace websockets;
WebsocketsServer WSserver;
AsyncWebServer webserver(80);

void setup() {
  DiffSteer.begin(fPivYLimit);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  // Ai-Thinker: pins 2 and 12
  ledcSetup(2, 50, 16); //channel, freq, resolution
  ledcAttachPin(12, 2); // pin, channel
  ledcSetup(4, 50, 16); //channel, freq, resolution
  ledcAttachPin(2, 4); // pin, channel

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_vflip(s, 0); //flip camera image
  s->set_hmirror(s, 0);  //mirror camera image

  // Wi-Fi connection
  Serial.print("Creating WiFi AP for ");
  Serial.println(ssid);
  // Remove the password parameter, if you want the AP (Access Point) to be open
  //  WiFi.softAP(ssid, password);
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
}

void handle_message(WebsocketsMessage msg) {
  int commaIndex = msg.data().indexOf(',');
  int steerValue = msg.data().substring(0, commaIndex).toInt();
  int driveValue = msg.data().substring(commaIndex + 1).toInt();
  Serial.print(steerValue);
  Serial.print(",");
  Serial.println(driveValue);
  steerValue = map(steerValue, -90, 90, -127, 127); // 0-180
  driveValue = map(driveValue, -90, 90, 127, -127); // 0-180 reversed
  differential_steer(steerValue, driveValue);
}

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
void loop() {
  auto client = WSserver.accept();
  client.onMessage(handle_message);
  while (client.available()) {
    client.poll();
    fb = esp_camera_fb_get();
    client.sendBinary((const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    fb = NULL;
  }
}
