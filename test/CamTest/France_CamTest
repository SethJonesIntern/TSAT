//Two SPI buses version that works
#include <Arducam_Mega.h>
#include <SPI.h>
#include <SD.h>

// Camera SPI pins (VSPI)
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

// Create separate SPI instances
SPIClass spiSD(HSPI);  // HSPI for SD card
// VSPI (default SPI) for camera

Arducam_Mega myCAM1(CAM1_CS);
Arducam_Mega myCAM2(CAM2_CS);
uint8_t image_buf[PIC_BUFFER_SIZE];
int pic_num = 0;

void write_pic(Arducam_Mega &cam, File &dest);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  Serial.println("\n=== Starting ===");
  
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
  Serial.println("Ready");
}

void loop() {
  Serial.println("\n=== Starting synchronized capture ===");
  unsigned long captureStart = millis();
  
   // Trigger both cameras as quickly as possible
  Serial.println("Triggering camera 1...");
  myCAM1.takePicture(CAM_IMAGE_MODE_FHD, CAM_IMAGE_PIX_FMT_JPG);
  
  Serial.println("Triggering camera 2...");
  myCAM2.takePicture(CAM_IMAGE_MODE_FHD, CAM_IMAGE_PIX_FMT_JPG);
  
  unsigned long captureEnd = millis();
  Serial.print("Both cameras triggered in ");
  Serial.print(captureEnd - captureStart);
  Serial.println(" ms");
  
  // Wait for both cameras to finish capturing
  delay(150);
  
  // Read and save from camera 1
  Serial.println("\n--- Saving from Camera 1 ---");
  char fp1[32];
  sprintf(fp1, "/images/cam1_pic%d.jpg", pic_num);
  
  File file1 = SD.open(fp1, FILE_WRITE);
  if (!file1) {
    Serial.println("CAM1 FILE OPEN FAIL");
  } else {
    write_pic(myCAM1, file1);
    
    File check1 = SD.open(fp1);
    if (check1) {
      Serial.print("Saved: ");
      Serial.print(fp1);
      Serial.print(" (");
      Serial.print(check1.size());
      Serial.println(" bytes)");
      check1.close();
    }
  }
  // Read and save from camera 2
  Serial.println("\n--- Saving from Camera 2 ---");
  char fp2[32];
  sprintf(fp2, "/images/cam2_pic%d.jpg", pic_num);
  
  File file2 = SD.open(fp2, FILE_WRITE);
  if (!file2) {
    Serial.println("CAM2 FILE OPEN FAIL");
  } else {
    write_pic(myCAM2, file2);
    
    File check2 = SD.open(fp2);
    if (check2) {
      Serial.print("Saved: ");
      Serial.print(fp2);
      Serial.print(" (");
      Serial.print(check2.size());
      Serial.println(" bytes)");
      check2.close();
    }
  }
  
  pic_num++;
  
  Serial.println("\n=== Capture complete ===");
  delay(3000);
 }

void write_pic(Arducam_Mega &cam, File &dest) {
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
