#include <Arducam_Mega.h>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define CAM1_PIN 00
#define CAM2_PIN 00
#define SD_PIN 00



Arducam_Mega cam1(CAM1_PIN);
Arducam_Mega cam2(CAM2_PIN);


int initCam(Arducam_Mega &cam){


    digitalWrite(CAM1_PIN, HIGH);
    digitalWrite(CAM2_PIN, HIGH);
    digitalWrite(SD_PIN, HIGH);



    if(!cam.begin()){
        //error starting the camera
    }
    cam.f
    cam.setImageQuality()
}



void setup(){

}

void loop(){

}