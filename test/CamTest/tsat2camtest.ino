#include <Arducam_Mega.h>
#include <SPI.h>
#include <SD.h>

#define CAM_CS 17
#define SD_CS 5

#define SCK 18
#define MISO 19
#define MOSI 23

Arducam_Mega myCAM(CAM_CS);
uint8_t buffer[1024];
int fileIndex = 0;

void setup() {
  Serial.begin(115200);

  SPI.begin(SCK, MISO, MOSI);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD INIT FAIL");
    while (1);
  }

  // Create folder only once
  SD.mkdir("/images");

  uint8_t r = myCAM.begin();
  if (r != CAM_ERR_SUCCESS) {
    Serial.println("CAMERA INIT FAIL");
    while (1);
  }

  Serial.println("Camera Ready");
}

void loop() {

  Serial.println("Capturing...");
  myCAM.takePicture(CAM_IMAGE_MODE_FHD, CAM_IMAGE_PIX_FMT_JPG);

  // Wait for camera to finish capturing
  delay(50);

  uint32_t totalLen = myCAM.getReceivedLength();
  if (totalLen == 0) {
    Serial.println("ERROR: No image data!");
    return;
  }

  // Generate sequential filename: img000.jpg
  char filename[32];
  sprintf(filename, "/images/img%03d.jpg", fileIndex++);

  File img = SD.open(filename, FILE_WRITE);
  if (!img) {
    Serial.println("FILE OPEN FAIL");
    return;
  }

  // Read image in chunks
  while (totalLen > 0) {
    uint16_t len = min(totalLen, (uint32_t)1024);

    myCAM.readBuff(buffer, len);
    img.write(buffer, len);

    totalLen -= len;
  }

  img.close();
  Serial.print("Saved: ");
  Serial.println(filename);

  delay(3000); // 3-second interval
}
