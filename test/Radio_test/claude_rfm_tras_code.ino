// Ground ESP32 (TX)
//#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h>

// SPI Pins
#define SCK  18
#define MISO 19
#define MOSI 23

// RFM AND BUTTON
#define RF95_CS   5
#define RF95_INT  2
#define RF95_RST  4
#define BUTTON_PIN 15

// Config
#define SPI_CLOCK_FREQ 8000000
#define SERIAL_BAUD    74880

RH_RF95 rf95(RF95_CS, RF95_INT);

int buttonValue     = 0;
int lastButtonValue = 0;

uint8_t detachMsg[] = "Detach";

void setup() {
  Serial.begin(SERIAL_BAUD);  // ✅ use defined constant
  Serial.println("Initializing ground...");

  SPI.begin(SCK, MISO, MOSI, RF95_CS);  // ✅ use defined SPI pins

  pinMode(RF95_RST, OUTPUT);
  digitalWrite(RF95_RST, HIGH);
  pinMode(BUTTON_PIN, INPUT);

  if (!rf95.init()) {
    Serial.println("LoRa init failed");
    while (1);
  }

  rf95.setFrequency(915.0);
  Serial.println("Ground Station Ready");
}

void loop() {
  buttonValue = digitalRead(BUTTON_PIN);

  if (buttonValue == HIGH && lastButtonValue == LOW) {
    Serial.println("Sending 'Detach'...");
    rf95.send(detachMsg, sizeof(detachMsg));
    rf95.waitPacketSent();
    Serial.println("Message sent: 'Detach'");
    delay(200);
  }

  lastButtonValue = buttonValue;

  if (rf95.waitAvailableTimeout(50)) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      if (len >= sizeof(buf)) len = sizeof(buf) - 1;
      buf[len] = '\0';
      Serial.print("Reply from T-Sat: ");
      Serial.println((char *)buf);
    } else {
      Serial.println("Receive failed");
    }
  }
}