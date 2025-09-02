#define LEFT_IR_PIN 4
#define RIGHT_IR_PIN 13

int leftIRSensorValue;
int rightIRSensorValue;
int threshold = 500;

bool prevLeftState = false;   // false = not triggered, true = triggered
bool prevRightState = false;

void setup() {
  // Ai-Thinker PWM setup (leave if needed)
  ledcSetup(2, 50, 16); // channel 2, 50 Hz, 16-bit resolution
  ledcAttachPin(12, 2);
  ledcSetup(4, 50, 16); // channel 4, 50 Hz, 16-bit resolution
  ledcAttachPin(2, 4);

  Serial.begin(115200);

  pinMode(LEFT_IR_PIN, INPUT);
  pinMode(RIGHT_IR_PIN, INPUT);
}

void loop() {
  // Read analog sensor values 
  leftIRSensorValue = analogRead(LEFT_IR_PIN);
  rightIRSensorValue = analogRead(RIGHT_IR_PIN);

  // Determine current trigger state based on threshold
  bool currentLeft = (leftIRSensorValue < threshold);
  bool currentRight = (rightIRSensorValue < threshold);

  // Check if left sensor was just triggered
  if (currentLeft && !prevLeftState) {
    Serial.println("Left IR sensor triggered");
  }

  // Check if right sensor was just triggered
  if (currentRight && !prevRightState) {
    Serial.println("Right IR sensor triggered");
  }

  // Update previous states
  prevLeftState = currentLeft;
  prevRightState = currentRight;

  delay(50);
}
