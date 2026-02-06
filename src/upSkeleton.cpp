// Libraries

// Arducam/ S
#include <Arducam_Mega.h>
#include <SPI.h>
#include <SD.h>

// Barometer
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <Wire.h>

#include <Arduino.h>
#include <RH_RF95.h>
#include <ESP32Servo.h>

// CONSTANTS

// PRESSURE
#define SEALEVELPRESSURE_HPA (1013.25)

// Camera SPI pins of the (VSPI)
#define CAM2_CS 21
#define CAM1_CS 5
#define SCK 18
#define MISO 19
#define MOSI 23

// SD Card SPI pins (HSPI)
#define SD_CS 15
#define SCK_SD 14
#define MISO_SD 12
#define MOSI_SD 13

#define SPI_CLOCK_FREQ 8000000
#define PIC_BUFFER_SIZE 4096
#define SERIAL_BAUD 74880

// Transmissions
#define RF95_CS 5
#define RF95_INT 21
#define RF95_RST 4

#define SERVO_PIN 13 // choose a free PWM-capable pin (NOT SPI SCK)
#define BUTTON_PIN 2 // local safety/arm button on the satellite//

// Reviever declarations
RH_RF95 rf95(RF95_CS, RF95_INT);
Servo servo1;

// Message sent when the button is pressed
uint8_t detachMsg[] = "Detach";

// Camera declarations
//  Create separate SPI instances
SPIClass spiSD(HSPI); // HSPI for SD card
// VSPI (default SPI) for camera

Arducam_Mega myCAM1(CAM1_CS);
Arducam_Mega myCAM2(CAM2_CS);
uint8_t image_buf[PIC_BUFFER_SIZE];
int pic_num = 0;

// barometer
Adafruit_BMP3XX bmp;

// FUNCTION PROTOTYPES
void write_pic(Arducam_Mega &cam, File &dest);
void triggerDetach();
float calculateAltitude(float atmospheric);

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing TSAT...");


    //BAROMETER INIT
    if (!bmp.begin_I2C(0x77))
    {
        Serial.println(F("BMP388 not found at 0x77, trying 0x76...")); // Standard addresses for the I2C  PINS SDA 21 AND SCL 22
        if (!bmp.begin_I2C(0x76))
        {
            Serial.println(F("ERROR: Could not find BMP388 sensor, CHECK WIRING"));
            while (1)
                delay(10);
        }
    }

    bmp.setPressureOversampling(BMP3_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_7);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);

    bmp.performReading(); // discard first one
    delay(100);
    bmp.performReading(); // start using readings after this

    // RECIEVER/TRANSMITTER INIT
    Serial.println("Initializing RX...");

    // Radio reset
    pinMode(RF95_RST, OUTPUT);
    digitalWrite(RF95_RST, HIGH);

    // Local safety button
    pinMode(BUTTON_PIN, INPUT); // or INPUT_PULLUP if wired to GND

    if (!rf95.init())
    {
        Serial.println("LoRa init failed");
        while (1)
            ;
    }

    rf95.setFrequency(915.0);
    rf95.setTxPower(13, false);

    // SERVO INIT
    servo1.attach(SERVO_PIN);
    servo1.write(0); // initial position

    Serial.println("T-Sat RX ready");

    // --------CAMERA INIT-------///


    
    // Serial.begin(SERIAL_BAUD);
    // delay(1000);

    Serial.println("\n=== Starting ARDUCAM ===");

    // Initialize VSPI for camera (its the default SPI)
    SPI.begin(SCK, MISO, MOSI, -1);
    pinMode(CAM1_CS, OUTPUT);
    digitalWrite(CAM1_CS, HIGH);
    pinMode(CAM2_CS, OUTPUT);
    digitalWrite(CAM2_CS, HIGH);
    delay(100);

    // Initialize HSPI for SD card
    spiSD.begin(SCK_SD, MISO_SD, MOSI_SD, SD_CS);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(100);

    // Init camera on VSPI
    Serial.println("Initializing camera 1...");
    SPI.setFrequency(SPI_CLOCK_FREQ);
    uint8_t r1 = myCAM1.begin();
    if (r1 != CAM_ERR_SUCCESS)
    {
        Serial.print("CAMERA 1 INIT FAIL: ");
        Serial.println(r1);
        while (1)
            delay(1000);
    }
    Serial.println("Camera 1 initialized");
    // Cam 2 Init
    Serial.println("Initializing camera 2...");
    uint8_t r2 = myCAM2.begin();
    if (r2 != CAM_ERR_SUCCESS)
    {
        Serial.print("CAMERA 2 INIT FAIL: ");
        Serial.println(r2);
        while (1)
            delay(1000);
    }
    Serial.println("Camera 2 initialized");

    // Init SD card on HSPI
    Serial.println("Initializing SD card...");
    if (!SD.begin(SD_CS, spiSD))
    {
        Serial.println("SD INIT FAIL");
        while (1)
            delay(1000);
    }
    Serial.println("SD card initialized");
    // Re-configure camera SPI after SD init
    SPI.setFrequency(SPI_CLOCK_FREQ);
    SD.mkdir("/images");
    Serial.println("Ready");
    Serial.println("Camera System Ready");
}

void loop(){
    
}

// AUXILLARY FUNCTIONS

// BAROMETER
float calculateAltitude(float atmospheric)
{
    atmospheric = atmospheric / 100.0;
    return 44330.0 * (1.0 - pow(atmospheric / SEALEVELPRESSURE_HPA, 0.1903));
} // Stolen Valor

// Servo function
void triggerDetach()
{
    Serial.println("DETACH: moving servo...");
    servo1.write(180); // detach position
    delay(2000);       // hold for 2s (adjust as needed)
    servo1.write(0);   // back to safe position (optional)
    Serial.println("Servo movement complete");
}

// CAMERA
void write_pic(Arducam_Mega &cam, File &dest)
{
    uint8_t prev_byte = 0;
    uint8_t cur_byte = 0;
    bool head_flag = false;
    int read_len = 0;
    int start_offset = 0;
    int bytes_written = 0;

    while (cam.getReceivedLength())
    {
        read_len = 0;

        // Fill image_buf in chunks (max 254 bytes at a time per ArduCAM limitation)
        while (read_len < PIC_BUFFER_SIZE && cam.getReceivedLength())
        {
            uint32_t chunk_len = min(254, PIC_BUFFER_SIZE - read_len);
            uint32_t actual_read = cam.readBuff(image_buf + read_len, chunk_len);

            if (actual_read == 0)
            {
                break;
            }
            read_len += actual_read;
        }

        start_offset = 0;

        // Process buffer to find JPEG markers
        for (int i = 0; i < read_len; i++)
        {
            prev_byte = cur_byte;
            cur_byte = image_buf[i];

            // Found JPEG start marker (0xFF 0xD8)
            if (prev_byte == 0xff && cur_byte == 0xd8)
            {
                Serial.println("Found JPEG start");
                head_flag = true;
                start_offset = i + 1;

                bytes_written += dest.write(0xff);
                bytes_written += dest.write(0xd8);
                dest.flush();
            }

            // Found JPEG end marker (0xFF 0xD9)
            if (head_flag && prev_byte == 0xff && cur_byte == 0xd9)
            {
                Serial.println("Found JPEG end");

                // Write remaining data up to and including 0xD9
                int chunk_len = i + 1 - start_offset;
                bytes_written += dest.write(image_buf + start_offset, chunk_len);
                dest.close();

                Serial.print("Total bytes written: ");
                Serial.println(bytes_written);
                return;
            }
        }

        // Write the buffer data if we've found the header
        if (head_flag)
        {
            int chunk_len = read_len - start_offset;
            int retval = dest.write(image_buf + start_offset, chunk_len);

            // Handle write failure by reopening file
            if (retval == 0 && chunk_len > 0)
            {
                Serial.println("Write failed, reopening file...");
                char fp[40];
                strcpy(fp, dest.path());
                dest.close();
                delay(10);
                dest = SD.open(fp, FILE_APPEND);

                if (dest)
                {
                    retval = dest.write(image_buf + start_offset, chunk_len);
                    Serial.print("Retry wrote ");
                    Serial.println(retval);
                }
                else
                {
                    Serial.println("REOPEN FAILED!");
                    return;
                }
            }

            bytes_written += retval;
            dest.flush();
        }
    }

    Serial.println("WARNING: No JPEG end flag found");
    dest.close();
}
