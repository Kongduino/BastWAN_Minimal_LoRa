#include <SPI.h>
#include <LoRa.h>
#include <LoRandom.h>
#include "aes.c"
#include "SparkFun_External_EEPROM.h"
// Click here to get the library: http://librarymanager/All#SparkFun_External_EEPROM
/*
  NOTE!
  Add:
  namespace std _GLIBCXX_VISIBILITY(default) {
    _GLIBCXX_BEGIN_NAMESPACE_VERSION
    void __throw_length_error(char const*) {}
    void __throw_bad_alloc() {}
    void __throw_out_of_range(char const*) {}
    void __throw_logic_error(char const*) {}
  }
  to ~/Library/Arduino15/packages/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1/arm-none-eabi/include/c++/4.8.3/bits/basic_string.h

  Add:
   -D_GLIBCXX_USE_C99
  to compiler.c.flags & compiler.cpp.flags.
  Or the code won't compile.
*/

/*
  You need to define the buffer lengths in SparkFun_External_EEPROM.h
  Around line 56:
  #elif defined(_VARIANT_ELECTRONICCATS_BASTWAN_)
  #define I2C_BUFFER_LENGTH_RX SERIAL_BUFFER_SIZE
  #define I2C_BUFFER_LENGTH_TX SERIAL_BUFFER_SIZE
*/
#include "ArduinoJson.h"
// Click here to get the library: http://librarymanager/All#ArduinoJson

// Uncomment the next line if uploading to Pavel
//#define Pavel 1

#ifdef Pavel
#include <Wire.h>
#include "ClosedCube_BME680.h"
ClosedCube_BME680 bme680;
double lastReading = 0;
#define BME_PING_DELAY 300000 // 5 minutes
#define NEED_SIDE_I2C 1
// Since other devices later may also need side I2C,
// let's define it and use it for pins 5/6 Vcc/GND
ExternalEEPROM myMem;
#endif

#include "helper.h"
#include "haversine.h"

/*
  Welcome to role-assigned values: each machine will have a specific role,
  and code will be compiled and run depending on who it is for.

  Pavel:
   - Outdoors (WHEN IT'S NOT RAINING) device.
   - BME680 inside
   - Possibly an OV5208 camera soon
*/

void setup() {
  // ---- HOUSEKEEPING ----
  SerialUSB.begin(115200);
  delay(3000);
  if (NEED_DEBUG == 1) {
    SerialUSB.println("\n\nBastWAN at your service!");
  }
#ifdef NEED_SIDE_I2C
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - Set up I2C");
  }
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(5, LOW); // Keyboard Featherwing I2C GND
  digitalWrite(6, HIGH); // Keyboard Featherwing I2C VCC
  Wire.begin(SDA, SCL);
  Wire.setClock(100000);
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - Start EEPROM");
  }
  if (myMem.begin() == false) {
    if (NEED_DEBUG == 1) {
      Serial.println("   No memory detected. Freezing.");
    }
    while (1)
      ;
  }
  uint32_t myLen = myMem.length(), index = 0;
  if (NEED_DEBUG == 1) {
    Serial.println("Memory detected!");
    Serial.print("Mem size in bytes: ");
    Serial.println(myLen);
  }
  memset(msgBuf, 0, 97);
  myMem.read(0, msgBuf, 32);
  myMem.read(32, msgBuf + 32, 32);
  myMem.read(64, msgBuf + 64, 32);
  // Let's limit the JSON string size to 96 for now.
  if (NEED_DEBUG == 1) {
    hexDump(msgBuf, 96);
  }
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msgBuf);
  if (error) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println(F("\ndeserializeJson() failed!"));
    }
    savePrefs();
  }
  myFreq = doc["myFreq"];
  mySF = doc["mySF"] = mySF;
  myBW = doc["myBW"];
  const char *x = doc["deviceName"];
  memcpy(deviceName, x, 33);
  if (NEED_DEBUG == 1) {
    SerialUSB.print("FQ: "); Serial.println(myFreq / 1e6);
    SerialUSB.print("SF: "); Serial.println(mySF);
    SerialUSB.print("BW: "); Serial.println(myBW);
    Serial.print("Device Name: "); Serial.println(deviceName);
  }
#endif

#ifdef Pavel
  // ---- BME STUFF ----
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - ClosedCube BME680 ([T]emperature, [P]ressure, [H]umidity)");
  }
  bme680.init(0x77); // I2C address: 0x76 or 0x77
  bme680.reset();
  if (NEED_DEBUG == 1) {
    SerialUSB.print("Chip ID=0x");
    SerialUSB.println(bme680.getChipID(), HEX);
  }
  // oversampling: humidity = x1, temperature = x2, pressure = x16
  bme680.setOversampling(BME680_OVERSAMPLING_X1, BME680_OVERSAMPLING_X2, BME680_OVERSAMPLING_X16);
  bme680.setIIRFilter(BME680_FILTER_3);
  bme680.setForcedMode();
#endif
  pinMode(RFM_TCXO, OUTPUT);
  pinMode(RFM_SWITCH, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LoRa.setPins(SS, RFM_RST, RFM_DIO0);
  if (!LoRa.begin(myFreq)) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Starting LoRa failed!\nNow that's disappointing...");
    }
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
  // 111 +20dBm on PA_BOOST when OutputPower=1111
  //  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
  LoRa.writeRegister(REG_OCP, 0b00111111); // MAX OCP
  // 0b 0010 0011
  // 001 G1 = highest gain
  // 00  Default LNA current
  // 0   Reserved
  // 11  Boost on, 150% LNA current
  LoRa.receive();
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
#ifdef Pavel
  setDeviceName("Pavel");
#else
  setDeviceName("Simon");
#endif
}

void loop() {
#ifdef Pavel
  double t0 = millis();
  if (t0 - lastReading >= BME_PING_DELAY) displayBME680();
#endif
  // Uncomment if you have a battery plugged in.
  //  if (millis() - batteryUpdateDelay > 10000) {
  //    getBattery();
  //    batteryUpdateDelay = millis();
  //  }
  int packetSize = LoRa.parsePacket();
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
    if (NEED_DEBUG == 1) {
      SerialUSB.print("Received packet: ");
    }
    if (needEncryption) {
      if (NEED_DEBUG == 1) {
        SerialUSB.println("\n . Decrypting...");
      }
      decryptECB(msgBuf, strlen((char*)msgBuf));
      memset(msgBuf, 0, 256);
      memcpy(msgBuf, encBuf, strlen((char*)encBuf));
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, msgBuf);
    if (error) {
      if (NEED_DEBUG == 1) {
        SerialUSB.print(F("deserializeJson() failed: "));
        SerialUSB.println(error.f_str());
      }
      return;
    }
    int rssi = LoRa.packetRssi();

    // DISPLAY HERE JSON PACKET
    // IF NEED_DEBUG IS NOT DEFINED
    if (NEED_DEBUG == 0) {
      doc["rssi"] = rssi;
      serializeJson(doc, Serial);
    }
    // Print 4-byte ID
    const char *myID = doc["UUID"];

    // Print sender
    const char *from = doc["from"];

    // Print command
    const char *cmd = doc["cmd"];

    // Do we have a message?
    if (strcmp(cmd, "msg") == 0) {
      const char *msg = doc["msg"];
    }

    bool hasLatLong = true;
    float tLat, tLong, tDistance;
    JsonVariant mydata = doc["lat"];
    if (mydata.isNull()) {
      // we don't have
      hasLatLong = false;
    } else {
      tLat = mydata.as<float>();
      mydata = doc["long"];
      if (mydata.isNull()) {
        // we don't have
        hasLatLong = false;
      } else {
        tLong = mydata.as<float>();
        // we now have both values AND hasLatLong = true
        // Display distance
        tDistance = haversine(homeLatitude, homeLongitude, tLat, tLong);
      }
    }

    if (NEED_DEBUG == 1) {
      SerialUSB.print("ID: ");
      SerialUSB.println(myID);
      SerialUSB.print("Sender: ");
      SerialUSB.println(from);
      SerialUSB.print("Command: ");
      SerialUSB.println(cmd);
      if (strcmp(cmd, "msg") == 0) {
        const char *msg = doc["msg"];
        SerialUSB.print("Message: ");
        SerialUSB.println(msg);
      }
      if (hasLatLong) {
        SerialUSB.print("Distance: ");
        if (tDistance >= 1000.0) {
          SerialUSB.print(tDistance / 1000.0);
          SerialUSB.println(" km");
        } else {
          SerialUSB.print(tDistance);
          SerialUSB.println(" m");
        }
      }
      SerialUSB.print("RSSI: ");
      SerialUSB.println(rssi);
    }

    if (strcmp(cmd, "ping") == 0 && pongBack) {
      // if it's a PING, and we are set to respond:
      LoRa.idle();
      if (NEED_DEBUG == 1) {
        SerialUSB.println("Pong back:");
      }
      // we cannot pong back right away – the message could be lost
      uint16_t dl = getRamdom16() % 2500 + 800;
      if (NEED_DEBUG == 1) {
        SerialUSB.println("Delaying " + String(dl) + " millis...");
      }
      delay(dl);
      sendPong((char*)myID, rssi);
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
    else if (c == '/') {
      // "special" commands
      c = msgBuf[1]; // Subcommand
#ifdef Pavel
      if (c == 'B') {
        // BME680
        displayBME680();
        return;
      }
#endif
      if (c == 'A') {
        // Auto Ping
        setAutoPing((char*)msgBuf + 2);
        return;
      }
      if (c == 'D') {
        // Debug ON/OFF
        uint8_t d = msgBuf[2];
        if (d == 0) {
          NEED_DEBUG = 1 - NEED_DEBUG;
          return;
        } else if (d == '0') {
          NEED_DEBUG = 0;
          return;
        } else if (d == '1') {
          NEED_DEBUG = 1;
          return;
        }
      }
      if (c == 'H') {
        // Show help regardless of Debug ON/OFF
        showHelp();
        return;
      }
    } else {
      if (NEED_DEBUG == 1) {
        SerialUSB.println((char*)msgBuf);
        showHelp();
        return;
      }
    }
  }
  if (needPing) {
    double t0 = millis();
    if (t0 - lastAutoPing > pingFrequency) {
      sendPing();
      lastAutoPing = millis();
    }
  }
}

#ifdef Pavel
void displayBME680() {
  if (NEED_DEBUG == 1) {
    SerialUSB.println("BME680");
  }
  ClosedCube_BME680_Status status = bme680.readStatus();
  if (status.newDataFlag) {
    double temp = bme680.readTemperature();
    double pres = bme680.readPressure();
    double hum = bme680.readHumidity();
    if (NEED_DEBUG == 1) {
      SerialUSB.print("result: T=");
      SerialUSB.print(temp);
      SerialUSB.print("C, RH=");
      SerialUSB.print(hum);
      SerialUSB.print("%, P=");
      SerialUSB.print(pres);
      SerialUSB.print(" hPa");
      SerialUSB.println();
    }
    lastReading = millis();
  }
}
#endif
