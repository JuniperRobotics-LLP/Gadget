//  Juniper Robotics Face Tracking

#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <DifferentialSteering.h>
#include "dashboard.h"
#include "camera_index.h"
#include "fb_gfx.h"
#include "fd_forward.h"

/* --- USER ADJUSTED PARAMETERS START --- */

// Set your access point network credentials
const char* ssid = "Gadget_SSID"; //change "Gadget###" to match label on Gadget

/*  How far the face must be away from the center of the image to cause Gadget to move (in X coordinates).
    This value will be applied both to the left and right of the image so a value of 50 means that Gadget will not move
    in a window of 100 X coordinates from the center of the image
*/
int dead_zone = 30;    // Maximum value is : 50. Whole positive values only

// Speed at which Gadget will rotate (may need to adjust depending on surface)
int rotate_speed = 35;       // Maximum value is : 50. Whole positive values only

/* --- USER ADJUSTED PARAMETERS END --- */


DifferentialSteering DiffSteer;
int fPivYLimit = 32;
int idle = 0;

// Flag for the first time ran once connected to webui to send over params
bool first_run = true;


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


struct FaceCoords {
  int x;
  int y;
  bool detected;
};

camera_fb_t * fb = NULL;
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// QVGA resolution 320x240
const int sensor_width = 320;
const int sensor_height = 240;


#define BOUNDING_BOX_COLOR  0x0033FF00
#define FACE_TRACK_DELAY_MS 150

using namespace websockets;
WebsocketsServer WSserver;
AsyncWebServer webserver(80);

static inline mtmn_config_t app_mtmn_config()
{
  mtmn_config_t mtmn_config = {0};
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;
  return mtmn_config;
}
mtmn_config_t mtmn_config = app_mtmn_config();

unsigned long last_face_detect = millis();



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
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;


if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error: Camera initialization failed!");
    return;
}

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

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

  // Check that user defined values stay within bounds
  rotate_speed = constrain(rotate_speed, 0, 50);
  dead_zone = constrain(dead_zone, 0, 50);

  //Set var to track first run to only send params once
  first_run = true;

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
    
    // Print's motor values being sent
    Serial.print("\nCommanding Left Motor: ");
    Serial.print(leftMotor);
    Serial.print(" Commanding Right Motor: ");
    Serial.println(rightMotor);

    // Only run the motor command for 50ms to not overshoot
    delay(50);
    ledcWrite(2, idle); // channel, value
    ledcWrite(4, idle); // channel, value

  } 
  // Command 0 to each motor
  else
  {
    ledcWrite(2, idle); // channel, value
    ledcWrite(4, idle); // channel, value
  }
}



static FaceCoords create_face_box(dl_matrix3du_t *image_matrix, box_array_t *boxes)
{
  FaceCoords coords = {0, 0, false}; // default to no face detected

  int steerValue = rotate_speed;
  int driveValue = 0;

  int x, y, w, h, i, half_width, half_height;
  fb_data_t fb;
  fb.width = image_matrix->w;
  fb.height = image_matrix->h;
  fb.data = image_matrix->item;
  fb.bytes_per_pixel = 3;
  fb.format = FB_BGR888;
  
  if (boxes->len > 0) {
    int x0 = (int)boxes->box[0].box_p[0];
    int y0 = (int)boxes->box[0].box_p[1];
    int x1 = (int)boxes->box[0].box_p[2];
    int y1 = (int)boxes->box[0].box_p[3];

    int face_center_horizontal = (x0 + x1) / 2;
    int face_center_vertical   = (y0 + y1) / 2;

    int w = x1 - x0 + 1;
    int h = y1 - y0 + 1;
   

    // draw rectangle (same as before)
    fb_gfx_drawFastHLine(&fb, x0, y0, w, BOUNDING_BOX_COLOR);
    fb_gfx_drawFastHLine(&fb, x0, y0 + h - 1, w, BOUNDING_BOX_COLOR);
    fb_gfx_drawFastVLine(&fb, x0, y0, h, BOUNDING_BOX_COLOR);
    fb_gfx_drawFastVLine(&fb, x0 + w - 1, y0, h, BOUNDING_BOX_COLOR);

    coords.x = face_center_horizontal;
    coords.y = face_center_vertical;
    coords.detected = true;
   
    // Follow Logic for steering
    if (face_center_horizontal < (sensor_width/2 - dead_zone)){
      // Tell Gadget to Rotate Left
      differential_steer(-steerValue, driveValue);
    }
    else if (face_center_horizontal > (sensor_width/2 + dead_zone)){
      // Tell Gadget to Rotate Right
      differential_steer(steerValue, driveValue);
    }
    else {
      //Tell Gadget to stop rotating
      differential_steer(0, 0);
    }

    return coords;
  }
}


void loop() {
  // Set back to zero to help with lag/loss of connection
  differential_steer(0, 0);

  auto client = WSserver.accept();

  // Reset first_run on new client connection or refresh
  if (client.available()) {
    first_run = true;
  }
 
  dl_matrix3du_t *image_matrix = NULL;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;

  while (client.available()) {
    unsigned long now = millis();
    if (now - last_face_detect >= FACE_TRACK_DELAY_MS)
    {
      client.poll();
      fb = esp_camera_fb_get();
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
      image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  
      fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);

      box_array_t *net_boxes = NULL;
      net_boxes = face_detect(image_matrix, &mtmn_config);

      FaceCoords face_coords = {0,0,false};

      if (net_boxes) {
        face_coords=create_face_box(image_matrix, net_boxes);
        // Face Detected
        free(net_boxes->score);
        free(net_boxes->box);
        free(net_boxes->landmark);
        free(net_boxes);
      }
      else {
        // No Face Detected so stop rotating
        differential_steer(0, 0);
      }
      
      // Send coordinates to the webpage/serial if face detected
      if(face_coords.detected){
        char coord_msg[50];
        sprintf(coord_msg, "coords:%d,%d", face_coords.x, face_coords.y);
        Serial.println("Face Detected");
        Serial.println("-----");
        client.send(coord_msg);
      } else {
        client.send("coords:none");
        Serial.println("No Face Detected");
        Serial.println("-----");
      }

      // Send params to UI only on first run loop since they don't change
      if (first_run)
      {
        client.send("text_0: Configured Parameters ");
        client.send("text_1:" + String(sensor_width/2 - dead_zone) + "-" + String(sensor_width/2 + dead_zone)) ;
        client.send("text_2:" + String(rotate_speed));
        first_run = false;
      }

  

      fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len);
      client.sendBinary((const char *)_jpg_buf, _jpg_buf_len);
  
      esp_camera_fb_return(fb);
      fb = NULL;
      dl_matrix3du_free(image_matrix);
      free(_jpg_buf);
      _jpg_buf = NULL;
      unsigned long last_face_detect = millis();

    }
  }
}
