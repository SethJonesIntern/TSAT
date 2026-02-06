//Skeleton code for GROUND STATION
#include <Arduino.h>
#include <RH_RF95.h>
#include <ESP32Servo.h>




#include <SPI.h>
#include <SD.h>


// Transmissions
#define RF95_CS 5
#define RF95_INT 21
#define RF95_RST 4

#define BUTTON_PIN 2 // local safety/arm button on the satellite//

RH_RF95 rf95(RF95_CS, RF95_INT);
int buttonValue = 0;
int lastButtonValue = 0;


uint8_t detachMsg[] = "Detach";

//initialize Transmitter, Reciever, and SD
void setup(){

}



void loop(){

}

//transmit burnwire message
void sendBurnwire(){
    Serial.println("Sending 'Detach'...");
    rf95.send(detachMsg, sizeof(detachMsg));   // send "Detach"
    rf95.waitPacketSent();
    Serial.println("Message sent: 'Detach'");

    // optional debounce
    delay(200);

}


//write altitude to SD
void logAltitude(float altitude) {
    File file = SD.open("/altitude.txt", FILE_APPEND);

    if (file) {
        file.print(millis());
        file.print(",");
        file.println(altitude);
        file.close();
    }
}