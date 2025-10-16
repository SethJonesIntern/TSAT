#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_P 00
#define SERVO_INITIAL_POS 00
#define SERVO_DEPLOY_POS 00

Servo myservo;



void setup() {
    myservo.attach(SERVO_P);
    myservo.write(SERVO_INITIAL_POS);       // Attach, go to initial position, wait 3 seconds, go to desired position

    delay(3000);


    myservo.write(SERVO_DEPLOY_POS);
}

void loop() {
  // put your main code here, to run repeatedly:
}

