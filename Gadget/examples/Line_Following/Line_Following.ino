//  Juniper Robotics Line Following

#include <DifferentialSteering.h>

/* --- USER ADJUSTED PARAMETERS START --- */


//  Will continue straight unless the IR delta (Right value - Left Value) is larger than this value. Can help reduce oscillation
byte straight_threshold   =   20;              // Range: (20 to 50)

// Speed at which Gadget will move straight while turning
byte speed_straight_turn  =   15;              // Range: (15 to 25) 

// Speed at which Gadget will turn
byte speed_turn           =   20;              // Range: (20 to 50)

// Speed at which Gadget will move when only driving straight
byte speed_straight       =   33;              // Range: (33 to 50)

// Anything less than this value Gadget will not move and assume it's not on the ground/surface
byte ir_lift_threshold    =   20;              // No defined ranges here but this should be kept small or Gadget won't move on the track
                
/* --- USER ADJUSTED PARAMETERS END --- */


#define LEFT_IR             4               // GPIO for the Left IR Sensor
#define RIGHT_IR            13              // GPIO for the Right IR Sensor

DifferentialSteering DiffSteer;
int fPivYLimit = 32;
int idle = 0;

void setup() {

  DiffSteer.begin(fPivYLimit);
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // Ai-Thinker: pins 2 and 12
  ledcSetup(2, 50, 16); //channel, freq, resolution
  ledcAttachPin(12, 2); // pin, channel
  ledcSetup(4, 50, 16); //channel, freq, resolution
  ledcAttachPin(2, 4); // pin, channel

  Serial.begin(115200);

  // Check that user defined values stay within bounds.
  // straight_threshold
   if (straight_threshold > 50){
    straight_threshold = 50;
  }
  else if (straight_threshold < 20){
    straight_threshold = 20;
  }
  else if (straight_threshold < 0){
    straight_threshold = 0;
  }

  // speed_straight_turn
  if (speed_straight_turn > 25){
    speed_straight_turn = 25;
  }
  else if (speed_straight_turn < 15){
    speed_straight_turn = 15;
  }
  else if (speed_straight_turn < 0){
    speed_straight_turn = 0;
  }

  // speed_turn
  if (speed_turn > 50){
    speed_turn = 50;
  }
  else if (speed_turn < 20){
    speed_turn = 20;
  }
  else if (speed_turn < 0){
    speed_turn = 0;
  }

  // speed_straight
  if (speed_straight > 50){
    speed_straight = 50;
  }
  else if (speed_straight < 33){
    speed_straight = 33;
  }
  else if (speed_straight < 0){
    speed_straight = 0;
  }

    // ir_lift_threshold
  if (ir_lift_threshold < 0){
    ir_lift_threshold = 0;
  }

  Serial.println("User Defined Values: ");
  Serial.print("straight_threshold: ");
  Serial.println(straight_threshold);
  Serial.print("speed_straight_turn: ");
  Serial.println(speed_straight_turn);
  Serial.print("speed_turn: ");
  Serial.println(speed_turn);
  Serial.print("speed_straight: ");
  Serial.println(speed_straight);
  Serial.print("ir_lift_threshold: ");
  Serial.println(ir_lift_threshold);
  


}

void get_IR_sensors() {
  int left_ir, right_ir;
  int delta = 0;
  right_ir = analogRead(RIGHT_IR);
  left_ir = analogRead(LEFT_IR);

  Serial.print("Right IR Value: ");
  Serial.println(right_ir);
  Serial.print("Left IR Value: ");
  Serial.println(left_ir);
  delta = right_ir - left_ir;
  Serial.print("Difference: ");
  Serial.println(delta);
  Serial.println("--");

  // Don't move when picked up
  if (right_ir < ir_lift_threshold && left_ir < ir_lift_threshold){
    differential_steer(0, 0);
    Serial.println("Place on Ground");
  }
  // Don't move if no line is detected
  else if (right_ir > 1000 && left_ir > 1000){
    differential_steer(0, 0);
    Serial.println("No Line Detected");
  }
  // Turn Left
  else if (delta > 0 && delta > straight_threshold ){
    Serial.println("turn_left");
    differential_steer(-speed_turn, speed_straight_turn);
  }
  // Turn Right
  else if (delta < 0 && abs(delta) > straight_threshold ){
     Serial.println("turn_right");
     differential_steer(speed_turn, speed_straight_turn);
  }
  // Drive straight
  else{
    Serial.println("straight");
    differential_steer(0, speed_straight);
  }

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

get_IR_sensors();
delay(10);

}