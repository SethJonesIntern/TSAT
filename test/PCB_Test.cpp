//T-SAT Air 

//libraries
#include <Arducam_Mega.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//RF and Servo
#include <RH_RF95.h>
#include <ESP32Servo.h>

//BMP
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>

//Oled Display
#define OLED_SCL 22 
#define OLED_SDA 21 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

//Alt sensor 
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BMP3XX bmp;

// Camera SPI pins (VSPI)
#define CAM2_CS 27
#define CAM1_CS 25 
#define SCK 18
#define MISO 19
#define MOSI 23

// SD Card SPI pins (HSPI)
#define SD_CS 15
#define SCK_SD 14
#define MISO_SD 12
#define MOSI_SD 13

//RF and Servo 
#define RF95_CS   5
#define RF95_INT  2 //GOOO
#define RF95_RST  4

#define SERVO_PIN   32  // choose a free PWM-capable pin (NOT SPI SCK)

#define SPI_CLOCK_FREQ 8000000
#define PIC_BUFFER_SIZE 4096 
#define SERIAL_BAUD 74880

//Globals

//for the oled
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//for RF
RH_RF95 rf95(RF95_CS, RF95_INT);
Servo servo1;

// Create separate SPI instances
SPIClass spiSD(HSPI);  // HSPI for SD card
// VSPI (default SPI) for camera

Arducam_Mega myCAM1(CAM1_CS);
Arducam_Mega myCAM2(CAM2_CS);

uint8_t image_buf[PIC_BUFFER_SIZE];
int pic_num = 0;
int display_line = 0;

//Initializing functions 
void write_pic(Arducam_Mega &cam, File &dest);
void printBoth(String msg);
void printDisplay(String msg);

void setup(){
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  Serial.println("\n=== Starting ===");

  //***************** Init SPI Buses ******************************
  Serial.println("Setting up SPI buses...");
  
  // Initialize VSPI for camera and RF (default SPI)
  SPI.begin(SCK, MISO, MOSI, -1);
  pinMode(CAM1_CS, OUTPUT);
  digitalWrite(CAM1_CS, HIGH);
  pinMode(CAM2_CS, OUTPUT);
  digitalWrite(CAM2_CS, HIGH);
  pinMode(RF95_CS, OUTPUT);
  digitalWrite(RF95_CS, HIGH);
  delay(100);

  // Initialize 2nd SPI bus (HSPI) for SD card
  spiSD.begin(SCK_SD, MISO_SD, MOSI_SD, SD_CS);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(100);

  Serial.println("SPI buses ready");

  //Initalize RF module before two cams 
  Serial.println("Initializing LoRa RF module...");

  // Reset radio
  pinMode(RF95_RST, OUTPUT);
  digitalWrite(RF95_RST, LOW);
  delay(10);
  digitalWrite(RF95_RST, HIGH);
  delay(10);

  // Initialize radio
  if (!rf95.init()) {
    Serial.println("RF INIT FAIL - continuing without radio");
    // continue mission without comms
  } else {
    Serial.println("RF module initialized");
    
    if (!rf95.setFrequency(915.0)) {
      Serial.println("RF frequency set FAIL");
    } else {
      Serial.println("RF frequency: 915 MHz");
    }
    
    rf95.setTxPower(13, false);
    Serial.println("RF module ready");
  }

  //Initialzie Cameras

  // Cam 1 init (VSPI)
  Serial.println("Initializing camera 1...");
  SPI.setFrequency(SPI_CLOCK_FREQ);
  uint8_t r1= myCAM1.begin();
  if (r1 != CAM_ERR_SUCCESS) {
    Serial.print("CAMERA 1 INIT FAIL: ");
    Serial.println(r1);
    while (1) delay(1000);
  }
  Serial.println("Camera 1 initialized");
  //Cam 2 Init
  Serial.println("Initializing camera 2...");
  uint8_t r2 = myCAM2.begin();
  if (r2 != CAM_ERR_SUCCESS) {
    Serial.print("CAMERA 2 INIT FAIL: ");
    Serial.println(r2);
    while (1) delay(1000);
  }
  Serial.println("Camera 2 initialized");

  // Init SD card on HSPI
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD INIT FAIL");
    while (1) delay(1000);
  }
  Serial.println("SD card initialized");
  // Re-configure camera SPI after SD init
  SPI.setFrequency(SPI_CLOCK_FREQ);
  SD.mkdir("/images");
  Serial.println("SD Ready");

  //************************* Init I2C LAST ******************************

  Serial.println("Initializing I2C bus...");
  delay(500); // Stabilizing delay

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeout(100);
  Wire.setClock(100000);
  delay(100);

  // Initialize BMP sensor FIRST (more critical than OLED)
  Serial.println("Initializing BMP388 sensor...");
  bool bmp_ok = false;
  if (bmp.begin_I2C(0x77)) {
    bmp_ok = true;
    Serial.println("BMP388 found at 0x77");
  } else if (bmp.begin_I2C(0x76)) {
    bmp_ok = true;
    Serial.println("BMP388 found at 0x76");
  } else {
    Serial.println("ERROR: BMP388 not found - CHECK WIRING");
    while (1) delay(1000);
  }

  if (bmp_ok) {
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_7);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    bmp.performReading(); // Discard first reading
    delay(100);
    bmp.performReading(); // Start using readings after this
    Serial.println("BMP388 ready");
  }

  // Initialize OLED LAST (least critical)
  Serial.println("Initializing OLED...");
  bool oled_ok = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      oled_ok = true;
      Serial.println("OLED initialized");
      break;
    }
    Serial.print("OLED init attempt ");
    Serial.print(attempt + 1);
    Serial.println(" failed, retrying...");
    delay(200);
  }

  if (!oled_ok) {
    Serial.println("OLED init failed - continuing without display");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("System Ready!");
    display.println("RF: OK");
    display.println("Cams: OK");
    display.println("SD: OK");
    display.println("BMP: OK");
    display.display();
  }

  Serial.println("\n=== ALL SYSTEMS INITIALIZED ===\n");

}

void loop() {

  display.clearDisplay();
  display.setCursor(0, 0);
  display_line = 0;

  //1. Check RF
  if (rf95.available()) {
  Serial.println("[RF] MESSAGE DETECTED!");

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.recv(buf, &len)) {
    buf[len] = '\0';
    Serial.print("Received: ");
    Serial.println((char*)buf);
    Serial.print("RSSI: ");
    Serial.println(rf95.lastRssi());

    if (strcmp((char*)buf, "Detach") == 0) {
      Serial.println("DETACH command confirmed!");

      // SEND REPLY IMMEDIATELY
      const char reply[] = "Detached";
      rf95.send((uint8_t*)reply, strlen(reply));
      rf95.waitPacketSent();
      Serial.println("ACK sent!");

      // THEN do the servo action
      printBoth("DETACHING!");
      display.display();
      triggerDetach();

      Serial.println("Detach complete");
      }
    }
  }

  //2. BMP
  if (bmp.performReading()) { 
    float atmospheric = bmp.pressure;
    float altitude = calculateAltitude(atmospheric);
    
    // Update display buffer (don't display yet)
    display.clearDisplay();
    display.setCursor(0, 0);
    display_line = 0;
    printBoth("Alt: " + String(altitude, 2) + "m");
    printBoth("Pic: " + String(pic_num));
  } else {
    Serial.println("Altitude read failed"); 
  }

  //3. Cameras
  Serial.println("\n=== Starting synchronized capture ===");
  unsigned long captureStart = millis();
  
   // Trigger both cameras as quickly as possible
  printBoth("Triggering camera 1...");
  myCAM1.takePicture(CAM_IMAGE_MODE_FHD, CAM_IMAGE_PIX_FMT_JPG);
  
  printBoth("Triggering camera 2...");
  myCAM2.takePicture(CAM_IMAGE_MODE_FHD, CAM_IMAGE_PIX_FMT_JPG);
  
  unsigned long captureEnd = millis();
  Serial.print("Both cameras triggered in ");
  Serial.print(captureEnd - captureStart);
  Serial.println(" ms");
  
  // Wait for both cameras to finish capturing
  delay(150);
  
  // Read and save from camera 1
  printBoth("\n--- Saving from Camera 1 ---");
  char fp1[32];
  sprintf(fp1, "/images/cam1_pic%d.jpg", pic_num);
  printBoth("Saving CAM1 pic " + String(pic_num - 1));
  
  File file1 = SD.open(fp1, FILE_WRITE);
  if (!file1) {
    printBoth("CAM1 FILE OPEN FAIL");
  } else {
    write_pic(myCAM1, file1);
    
    File check1 = SD.open(fp1);
    if (check1) {
      String msg1 = "Saved: " + String(check1.size()) + " bytes";
      printBoth(msg1);
      check1.close();
    }
  }

  // Read and save from camera 2
  printBoth("\n--- Saving from Camera 2 ---");
  char fp2[32];
  sprintf(fp2, "/images/cam2_pic%d.jpg", pic_num);

  printBoth("Saving CAM2 pic " + String(pic_num - 1));
  
  File file2 = SD.open(fp2, FILE_WRITE);
  if (!file2) {
    printBoth("CAM2 FILE OPEN FAIL");
  } else {
    write_pic(myCAM2, file2);
    
    File check2 = SD.open(fp2);
    if (check2) {
      String msg2 = "Saved: " + String(check2.size()) + " bytes";
      printBoth(msg2);
      printBoth("Saved: ");
      check2.close();
    }
  }
  
  pic_num++;
  
  printBoth("\n=== Capture complete ===");

  delay(3000);

}

void write_pic(Arducam_Mega &cam, File &dest){
  uint8_t prev_byte = 0;
  uint8_t cur_byte = 0;
  bool head_flag = false;
  int read_len = 0;
  int start_offset = 0;
  int bytes_written = 0;

  while (cam.getReceivedLength()) {
    read_len = 0;

    // Fill image_buf in chunks (max 254 bytes at a time per ArduCAM limitation)
    while (read_len < PIC_BUFFER_SIZE && cam.getReceivedLength()) {
      uint32_t chunk_len = min(254, PIC_BUFFER_SIZE - read_len);
      uint32_t actual_read = cam.readBuff(image_buf + read_len, chunk_len);
      
      if (actual_read == 0) {
        break;
      }
      read_len += actual_read;
    }

    start_offset = 0;

    // Process buffer to find JPEG markers
    for (int i = 0; i < read_len; i++) {
      prev_byte = cur_byte;
      cur_byte = image_buf[i];

      // Found JPEG start marker (0xFF 0xD8)
      if (prev_byte == 0xff && cur_byte == 0xd8) {
        Serial.println("Found JPEG start");
        head_flag = true;
        start_offset = i + 1;
        
        bytes_written += dest.write(0xff);
        bytes_written += dest.write(0xd8);
        dest.flush();
      }

      // Found JPEG end marker (0xFF 0xD9)
      if (head_flag && prev_byte == 0xff && cur_byte == 0xd9) {
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
    if (head_flag) {
      int chunk_len = read_len - start_offset;
      int retval = dest.write(image_buf + start_offset, chunk_len);
      
      // Handle write failure by reopening file
      if (retval == 0 && chunk_len > 0) {
        Serial.println("Write failed, reopening file...");
        char fp[40];
        strcpy(fp, dest.path());
        dest.close();
        delay(10);
        dest = SD.open(fp, FILE_APPEND);
        
        if (dest) {
          retval = dest.write(image_buf + start_offset, chunk_len);
          Serial.print("Retry wrote ");
          Serial.println(retval);
        } else {
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

//for bmp 
float calculateAltitude(float atmospheric) {
  atmospheric = atmospheric / 100.0;
  return 44330.0 * (1.0 - pow(atmospheric / SEALEVELPRESSURE_HPA, 0.1903));
}

// function for servo 
void triggerDetach() {
  printBoth("DETACH: moving servo...");
  servo1.write(180);     // detach position
  delay(2000);           // hold for 2s (adjust as needed)
  servo1.write(0);       // back to safe position (optional)
  printBoth("Servo movement complete");
}

// Print to both Serial and OLED, with screen clearing/scrolling
void printBoth(String msg){
  Serial.println(msg);
  printDisplay(msg);
}

// Print to OLED with auto-scrolling
void printDisplay(String msg){
  // If we've reached the bottom of the screen, clear and start over
  if (display_line >= 8){
    display.clearDisplay();
    display.setCursor(0, 0);
    display_line = 0;
  }
  
  display.println(msg);
  display.display();
  display_line++;
}
