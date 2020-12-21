#include <SPI.h>
#include <LoRa.h>
#include "aes.c"
#include "helper.h"

void setup() {
  SerialUSB.begin(9600);
  delay(2000);
  SerialUSB.println("\n\nBastWAN at your service!");
  delay(1000);
  pinMode(RFM_TCXO, OUTPUT);
  pinMode(RFM_SWITCH, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LoRa.setPins(SS, RFM_RST, RFM_DIO0);
  if (!LoRa.begin(868125000)) {
    SerialUSB.println("Starting LoRa failed!\nNow that's disappointing...");
    while (1);
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(250e3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  digitalWrite(RFM_SWITCH, HIGH);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    SerialUSB.print("Received packet: `");
    char buff[256];
    memset(buff, 0, 256);
    int ix = 0;
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      delay(10);
      //if (c > 31)
      buff[ix++] = c;
      // filter out non-printable chars (like 0x1A)
    }
    SerialUSB.print(buff);
    SerialUSB.print("` with RSSI ");
    SerialUSB.println(LoRa.packetRssi());
    if (needEncryption) {
      SerialUSB.println(" . Decrypting...");
      decryptECB((uint8_t*)buff, strlen(buff));
      SerialUSB.println((char*)encBuf);
    }
  }
  if (SerialUSB.available()) {
    char buff[256];
    memset(buff, 0, 256);
    int ix = 0;
    while (SerialUSB.available()) {
      char c = SerialUSB.read();
      delay(10);
      buff[ix++] = c;
    }
    char c = buff[0]; // Command
    if (c == 'S') sendPacket(buff + 1);
    else if (c == 'E') needEncryption = true;
    else if (c == 'e') needEncryption = false;
    else if (c == 'P') setPWD(buff + 1);
    else {
      SerialUSB.println(buff);
      showHelp();
    }
  }
}
