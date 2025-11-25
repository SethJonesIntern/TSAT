#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <Wire.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BMP3XX bmp;


float calculateAltitude(float atmospheric) {
  atmospheric = atmospheric / 100.0;
  return 44330.0 * (1.0 - pow(atmospheric / SEALEVELPRESSURE_HPA, 0.1903));
} //Stolen Valor


void setup(){
    Serial.begin(115200);
    while (!Serial) { ; }                 // optional on some boards
    if (!bmp.begin_I2C(0x77)) {
        Serial.println(F("BMP388 not found at 0x77, trying 0x76..."));  //Standard addresses for the I2C
        if (!bmp.begin_I2C(0x76)) {
            Serial.println(F("ERROR: Could not find BMP388 sensor, CHECK WIRING"));
            while (1) delay(10);
        }
    }

    bmp.setPressureOversampling(BMP3_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_7);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);

    bmp.performReading(); // discard first one
    delay(100);
    bmp.performReading(); // start using readings after this
}

void loop(){
    if (!bmp.performReading()) { Serial.println("Read fail"); delay(200); return; }
        float atmospheric = bmp.pressure;
        Serial.print("Altitude is: ");Serial.println(calculateAltitude(atmospheric),2);
        delay(1000);
}
