#include <Arduino.h>

//  Pin 
#define SENSOR1_TRIG   7
#define SENSOR1_ECHO   6
#define SENSOR2_TRIG   5
#define SENSOR2_ECHO   4

#define MOTOR1_RELAY   8   // Shoe Dust Cleaner Motor
#define MOTOR2_RELAY   9   // Shoe Polisher Motor
#define VALVE_RELAY    10  // Fluid dispenser solenoid

// Constants
#define DETECTION_LIMIT   10.0   // cm
#define DETECTION_MIN     1.0    // cm
#define SENSOR_TIMEOUT    20000  // ms

//  Relay logic
#define RELAY_ACTIVE_LOW

#ifndef RELAY_ACTIVE_LOW
  #define ON_SIGNAL  HIGH
  #define OFF_SIGNAL LOW
#else
  #define ON_SIGNAL  LOW
  #define OFF_SIGNAL HIGH
#endif

//  System state
bool isDusting = false;
int availableTokens = 0;
unsigned long valveStartTimestamp = 0;

//  Polisher FSM states
enum PolishMode { STANDBY, DISPENSING, BUFFING };
PolishMode polishStep = STANDBY;

//  Ultrasonic distance reader
float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, SENSOR_TIMEOUT);
  float distance = duration * 0.0343 / 2; // Convert to cm
  return distance;
}

//  Dust cleaner handler
void processDustCleaner(float reading) {
  switch (isDusting) {
    case false:
      if (reading > DETECTION_MIN && reading <= DETECTION_LIMIT) {
        digitalWrite(MOTOR1_RELAY, ON_SIGNAL);
        isDusting = true;
        availableTokens++;
        Serial.print("Dust Cleaner Started | Tokens: ");
        Serial.println(availableTokens);
      }
      break;

    case true:
      if (!(reading > DETECTION_MIN && reading <= DETECTION_LIMIT)) {
        digitalWrite(MOTOR1_RELAY, OFF_SIGNAL);
        isDusting = false;
        Serial.println("Dust Cleaner Stopped");
      }
      break;
  }
}

//  Polishing handler
void processPolisher(float polishRead) {
  switch (polishStep) {
    case STANDBY:
      if (polishRead > DETECTION_MIN && polishRead <= DETECTION_LIMIT) {
        if (availableTokens > 0) {
          digitalWrite(VALVE_RELAY, ON_SIGNAL);
          valveStartTimestamp = millis();
          polishStep = DISPENSING;
          availableTokens--;
          Serial.print("Fluid Dispensing... Tokens left: ");
          Serial.println(availableTokens);
        } else {
          Serial.println("No tokens available for polishing.");
        }
      }
      break;

    case DISPENSING:
      if (millis() - valveStartTimestamp >= 5000) {
        digitalWrite(VALVE_RELAY, OFF_SIGNAL);
        digitalWrite(MOTOR2_RELAY, ON_SIGNAL);
        polishStep = BUFFING;
        Serial.println("Dispense Complete → Buffing Started");
      }
      break;

    case BUFFING:
      if (!(polishRead > DETECTION_MIN && polishRead <= DETECTION_LIMIT)) {
        digitalWrite(MOTOR2_RELAY, OFF_SIGNAL);
        polishStep = STANDBY;
        Serial.println("Buffing Stopped");
      }
      break;
  }
}

//  Setup function
void setup() {
  Serial.begin(9600);

  pinMode(SENSOR1_TRIG, OUTPUT);
  pinMode(SENSOR1_ECHO, INPUT);
  pinMode(SENSOR2_TRIG, OUTPUT);
  pinMode(SENSOR2_ECHO, INPUT);

  pinMode(MOTOR1_RELAY, OUTPUT);
  pinMode(MOTOR2_RELAY, OUTPUT);
  pinMode(VALVE_RELAY, OUTPUT);

  digitalWrite(MOTOR1_RELAY, OFF_SIGNAL);
  digitalWrite(MOTOR2_RELAY, OFF_SIGNAL);
  digitalWrite(VALVE_RELAY, OFF_SIGNAL);
}

// Loop function
void loop() {
  float dustDistance = readDistance(SENSOR1_TRIG, SENSOR1_ECHO);
  float polishDistance = readDistance(SENSOR2_TRIG, SENSOR2_ECHO);

  processDustCleaner(dustDistance);
  processPolisher(polishDistance);

  delay(100); // Polling interval
}
