#include <SPI.h>
#include <LoRa.h>
#include <LoRandom.h>
#include "aes.c"
#include <ArduinoJson.h>
#include "helper.h"

void setup() {
  SerialUSB.begin(11520);
  delay(2000);
  SerialUSB.println("\n\nBastWAN at your service!");
  delay(1000);
  pinMode(RFM_TCXO, OUTPUT);
  pinMode(RFM_SWITCH, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LoRa.setPins(SS, RFM_RST, RFM_DIO0);
  if (!LoRa.begin(myFreq)) {
    SerialUSB.println("Starting LoRa failed!\nNow that's disappointing...");
    while (1);
  }
  stockUpRandom();
  // first fill a 256-byte array with random bytes
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(250e3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  digitalWrite(RFM_SWITCH, HIGH);
  LoRa.writeRegister(REG_PA_CONFIG, 0b11111111); // That's for the transceiver
  // 0B 1111 1111
  // 1    PA_BOOST pin. Maximum power of +20 dBm
  // 111  MaxPower 10.8+0.6*MaxPower [dBm] = 15
  // 1111 OutputPower Pout=17-(15-OutputPower) if PaSelect = 1 --> 17
  LoRa.writeRegister(REG_PA_DAC, PA_DAC_HIGH); // That's for the transceiver
  // 0B 1000 0111
  // 00000 RESERVED
  // 111   +20dBm on PA_BOOST when OutputPower=1111
  //  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
  LoRa.writeRegister(REG_OCP, 0b00111111); // MAX OCP
  // 0b 0010 0011
  // 001 G1 = highest gain
  // 00  Default LNA current
  // 0   Reserved
  // 11  Boost on, 150% LNA current
  LoRa.receive();
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
  setDeviceName("Device #03");
}

void loop() {
  // Uncomment if you have a battery plugged in.
  //  if (millis() - batteryUpdateDelay > 10000) {
  //    getBattery();
  //    batteryUpdateDelay = millis();
  //  }
  int packetSize = LoRa.parsePacket();
  //  if (packetSize) receiveJSONPacket();
  if (packetSize) {
    memset(msgBuf, 0, 256);
    int ix = 0;
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      delay(10);
      //if (c > 31)
      msgBuf[ix++] = c;
      // filter out non-printable chars (like 0x1A)
    } msgBuf[ix] = 0;
    SerialUSB.print("Received packet: ");
    if (needEncryption) {
      SerialUSB.println(" . Decrypting...");
      decryptECB(msgBuf, strlen((char*)msgBuf));
      memset(msgBuf, 0, 256);
      memcpy(msgBuf, encBuf, strlen((char*)encBuf));
    }
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, msgBuf);
    if (error) {
      SerialUSB.print(F("deserializeJson() failed: "));
      SerialUSB.println(error.f_str());
      return;
    }
    // Print 4-byte ID
    const char *myID = doc["UUID"];
    SerialUSB.print("ID: ");
    SerialUSB.println(myID);

    // Print sender
    const char *from = doc["from"];
    SerialUSB.print("Sender: ");
    SerialUSB.println(from);

    // Print command
    const char *cmd = doc["cmd"];
    SerialUSB.print("Command: ");
    SerialUSB.println(cmd);

    // Do we have a message?
    if (strcmp(cmd, "msg") == 0) {
      const char *msg = doc["msg"];
      SerialUSB.print("Message: ");
      SerialUSB.println(msg);
    }

    SerialUSB.print("RSSI: ");
    int rssi = LoRa.packetRssi();
    SerialUSB.println(rssi);

    if (strcmp(cmd, "ping") == 0 && pongBack) {
      // if it's a PING, and we are set to respond:
      LoRa.idle();
      SerialUSB.println("Pong back:");
      // we cannot pong back right away – the message could be lost
      uint16_t dl = getRamdom16() % 2500 + 800;
      SerialUSB.println("Delaying " + String(dl) + " millis...");
      delay(dl);
      sendPong((char*)myID);
      LoRa.receive();
    }
  }
  if (SerialUSB.available()) {
    memset(msgBuf, 0, 256);
    int ix = 0;
    while (SerialUSB.available()) {
      char c = SerialUSB.read();
      delay(10);
      if (c > 31) msgBuf[ix++] = c;
    } msgBuf[ix] = 0;
    char c = msgBuf[0]; // Command
    if (c == '>') {
      char buff[256];
      strcpy(buff, (char*)msgBuf + 1);
      prepareJSONPacket(buff);
      sendJSONPacket();
    } else if (c == 'D') setDeviceName((char*)msgBuf + 1);
    else if (c == 'E') needEncryption = true;
    else if (c == 'e') needEncryption = false;
    else if (c == 'P') setPWD((char*)msgBuf + 1);
    else if (c == 'R') setPongBack(true);
    else if (c == 'r') setPongBack(false);
    else if (c == 'F') setFQ((char*)msgBuf + 1);
    else if (c == 'S') setSF((char*)msgBuf + 1);
    else if (c == 'B') setBW((char*)msgBuf + 1);
    else if (c == 'p') sendPing();
    else {
      SerialUSB.println((char*)msgBuf);
      showHelp();
    }
  }
}
