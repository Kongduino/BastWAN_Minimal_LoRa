// Uncomment the next line if uploading to Pavel
//#define Pavel 1
// Uncomment this next line if you want to use a BME680
//#define NEED_BME 1
// Uncomment this next line if you want to use pins 5 & 6 for Gnd/Vcc
// Particularly useful on a breadboard as they are next to SDA/SCL
//#define NEED_SIDE_I2C 1

// Uncomment this next line if you want to use a DHT22
//#define NEED_DHT 1
// Uncomment this next line if you want to use an SSD1306 OLED
#define NEED_SSD1306 1
// Uncomment this next line if you want to use an HDC1080
#define NEED_HDC1080 1
// Uncomment this next line if you want to use an EEPROM
//#define NEED_EEPROM
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <LoRandom.h>
#include "aes.c"
#include "sha2.c"
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
  If you're planning to usee an EEPROM, you need
  to define the buffer lengths in SparkFun_External_EEPROM.h
  Around line 56:
  #elif defined(_VARIANT_ELECTRONICCATS_BASTWAN_)
  #define I2C_BUFFER_LENGTH_RX SERIAL_BUFFER_SIZE
  #define I2C_BUFFER_LENGTH_TX SERIAL_BUFFER_SIZE
*/
#include "ArduinoJson.h"
// Click here to get the library: http://librarymanager/All#ArduinoJson

#ifdef NEED_SSD1306
#include "SSD1306Ascii.h"
// Click here to get the library: http://librarymanager/All#SSD1306Ascii
#include "SSD1306AsciiWire.h"
#define I2C_ADDRESS 0x3C
#define RST_PIN -1
#define OLED_FORMAT &Adafruit128x32
SSD1306AsciiWire oled;
#endif // NEED_SSD1306

#ifdef NEED_HDC1080
#include <ClosedCube_HDC1080.h>
ClosedCube_HDC1080 hdc1080;
#define hdc1080_waitout 30000
double lastReading = 0;
float temp_hum_val[2] = {0};
#define PING_DELAY 300000 // 5 minutes
#endif // NEED_HDC1080

#ifdef NEED_BME
#include "ClosedCube_BME680.h"
ClosedCube_BME680 bme680;
double lastReading = 0;
#define PING_DELAY 300000 // 5 minutes
#endif // NEED_BME

#ifdef NEED_EEPROM
#include "SparkFun_External_EEPROM.h"
// Click here to get the library: http://librarymanager/All#SparkFun_External_EEPROM
ExternalEEPROM myMem;
#endif // NEED_EEPROM

#ifdef NEED_DHT
#include "DHT.h"
#define DHTPIN 9 // what pin we're connected to
#define DHTTYPE DHT22 // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);
#define PING_DELAY 300000 // 5 minutes
double lastReading = 0;
float p[2] = {0};
#endif // NEED_DHT

#include "helper.h"
#include "haversine.h"
#include "SerialCommands.h"

/*
  Welcome to role-assigned values: each machine will have a specific role,
  and code will be compiled and run depending on who it is for.

  Pavel:
   - Outdoors (WHEN IT'S NOT RAINING) device.
   - BME680 inside
   OR
   - DHT22 inside
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
  // this has to happen first, if the I2C bus is powered by 5/6
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - Set up I2C");
  }
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(5, LOW); // Keyboard Featherwing I2C GND
  digitalWrite(6, HIGH); // Keyboard Featherwing I2C VCC
  // And obviously we can't display on the OLED yet...
#endif // NEED_SIDE_I2C

  Wire.begin(SDA, SCL);
  Wire.setClock(100000);
#ifdef NEED_SSD1306
  SerialUSB.println("Setting up OLED");
  // Initialising the UI will init the display too.
  oled.begin(OLED_FORMAT, I2C_ADDRESS);
  oled.setFont(System5x7);
#if INCLUDE_SCROLLING == 0
#error INCLUDE_SCROLLING must be non-zero. Edit SSD1306Ascii.h
#endif // INCLUDE_SCROLLING
  // Set auto scrolling at end of window.
  oled.setScrollMode(SCROLL_MODE_AUTO);
  oled.println("BastWAN Minimal LoRa");
#endif // NEED_SSD1306

#ifdef NEED_EEPROM
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - Start EEPROM");
  }
#ifdef NEED_SSD1306
  oled.println(" . Start EEPROM");
#endif // NEED_SSD1306
  if (myMem.begin() == false) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("   No memory detected. Freezing.");
    }
#ifdef NEED_SSD1306
    oled.println("No memory detected.");
    oled.println("Freezing...");
#endif // NEED_SSD1306
    while (1)
      ;
  }
  uint32_t myLen = myMem.length(), index = 0;
  if (NEED_DEBUG == 1) {
    SerialUSB.println("Memory detected!");
    SerialUSB.print("Mem size in bytes: ");
    SerialUSB.println(myLen);
#ifdef NEED_SSD1306
    oled.println("Memory detected!");
    oled.println("Size in bytes: ");
    oled.println(myLen);
#endif // NEED_SSD1306
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
#ifdef NEED_SSD1306
    oled.println("JSON prefs fail.");
#endif // NEED_SSD1306
    savePrefs();
  }
  myFreq = doc["myFreq"];
  mySF = doc["mySF"] = mySF;
  myBW = doc["myBW"];
  myCR = doc["myCR"];
  const char *x = doc["deviceName"];
  memcpy(deviceName, x, 33);
  if (NEED_DEBUG == 1) {
    SerialUSB.print("FQ: "); SerialUSB.println(myFreq / 1e6);
    SerialUSB.print("SF: "); SerialUSB.println(mySF);
    SerialUSB.print("BW: "); SerialUSB.println(myBW);
    SerialUSB.print("Device Name: "); SerialUSB.println(deviceName);
  }
#ifdef NEED_SSD1306
  oled.println("JSON prefs fail.");
  oled.print("SF: "); oled.println(SF);
  oled.print("BW: "); oled.println(myBW);
#endif // NEED_SSD1306
#endif // NEED_EEPROM

#ifdef NEED_BME
  // ---- BME STUFF ----
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" - ClosedCube BME680 ([T]emperature, [P]ressure, [H]umidity)");
  }
#ifdef NEED_SSD1306
  oled.println("ClosedCube BME680");
#endif // NEED_SSD1306
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
#endif // NEED_BME

#ifdef NEED_SSD1306
  oled.println("LoRa Setup");
#endif // NEED_SSD1306
  pinMode(RFM_TCXO, OUTPUT);
  pinMode(RFM_SWITCH, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  LoRa.setPins(SS, RFM_RST, RFM_DIO0);
  if (!LoRa.begin(myFreq)) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Starting LoRa failed!\nNow that's disappointing...");
    }
#ifdef NEED_SSD1306
    oled.println("LoRa init failed!");
    oled.println("Freezing...");
#endif // NEED_SSD1306
    while (1);
  }

#ifdef NEED_SSD1306
  oled.println(" . Random");
#endif // NEED_SSD1306
  stockUpRandom();
  // first fill a 256-byte array with random bytes
#ifdef NEED_SSD1306
  oled.println(" . Set SF");
#endif // NEED_SSD1306
  LoRa.setSpreadingFactor(mySF);
#ifdef NEED_SSD1306
  oled.println(" . Set BW");
#endif // NEED_SSD1306
  LoRa.setSignalBandwidth(BWs[myBW] * 1e3);
#ifdef NEED_SSD1306
  oled.println(" . Set C/R");
#endif // NEED_SSD1306
  LoRa.setCodingRate4(myCR);
#ifdef NEED_SSD1306
  oled.println(" . Set Preamble");
#endif // NEED_SSD1306
  LoRa.setPreambleLength(8);
#ifdef NEED_SSD1306
  oled.println(" . Set Tx Power");
#endif // NEED_SSD1306
  LoRa.setTxPower(TxPower, PA_OUTPUT_PA_BOOST_PIN);
  digitalWrite(RFM_SWITCH, HIGH);
#ifdef NEED_SSD1306
  oled.println(" . Set PA_BOOST");
#endif // NEED_SSD1306
  if (PA_BOOST) LoRa.setTxPower(TxPower, PA_OUTPUT_PA_BOOST_PIN);
  else LoRa.setTxPower(TxPower, 0); // NOT RECOMMENDED!
#ifdef NEED_SSD1306
  oled.println(" . Set PA_CONFIG");
#endif // NEED_SSD1306
  LoRa.writeRegister(REG_PA_CONFIG, 0b11111111); // That's for the transceiver
  // 0B 1111 1111
  // 1    PA_BOOST pin. Maximum power of +20 dBm
  // 111  MaxPower 10.8+0.6*MaxPower [dBm] = 15
  // 1111 OutputPower Pout=17-(15-OutputPower) if PaSelect = 1 --> 17
#ifdef NEED_SSD1306
  oled.println(" . Set PA_DAC");
#endif // NEED_SSD1306
  LoRa.writeRegister(REG_PA_DAC, PA_DAC_HIGH); // That's for the transceiver
  // 0B 1000 0111
  // 00000 RESERVED
  // 111 +20dBm on PA_BOOST when OutputPower=1111
  //  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
#ifdef NEED_SSD1306
  oled.println(" . Set REG_OCP");
#endif // NEED_SSD1306
  if (OCP_ON) LoRa.writeRegister(REG_OCP, 0b00111111); // OCP Max 240
  else LoRa.writeRegister(REG_OCP, 0b00011111); // NO OCP
  // 0b 0010 0011
  // 001 G1 = highest gain
  // 00  Default LNA current
  // 0   Reserved
  // 11  Boost on, 150% LNA current
  LoRa.receive();
#ifdef NEED_SSD1306
  oled.println(" . Set REG_LNA");
#endif // NEED_SSD1306
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE

#ifdef Pavel
  setDeviceName("Pavel");
#else
  setDeviceName("Alfred");
#endif // Pavel
#ifdef NEED_SSD1306
  oled.println("Device name:"); oled.println(deviceName);
#endif // NEED_SSD1306

#ifdef NEED_DHT
#ifdef NEED_SSD1306
  oled.println("DHT");
#endif // NEED_SSD1306
  dht.begin();
#endif // NEED_DHT

#ifdef NEED_HDC1080
#ifdef NEED_SSD1306
  oled.println("HDC1080");
#endif // NEED_SSD1306
  hdc1080.begin(0x40); // I2C address
  SerialUSB.print("Manufacturer ID=0x");
  SerialUSB.println(hdc1080.readManufacturerId(), HEX); // 0x5449 ID of Texas Instruments
  SerialUSB.print("Device ID=0x");
  SerialUSB.println(hdc1080.readDeviceId(), HEX); // 0x1050 ID of the device
  Serial.print("Device Serial Number=");
  HDC1080_SerialNumber sernum = hdc1080.readSerialNumber();
  char format[12];
  sprintf(format, "%02X-%04X-%04X", sernum.serialFirst, sernum.serialMid, sernum.serialLast);
  Serial.println(format);
  HDC1080_Registers reg = hdc1080.readRegister();
  SerialUSB.print("Battery: 0x");
  SerialUSB.println(reg.BatteryStatus, HEX);
  SerialUSB.print("Heater: 0x");
  SerialUSB.println(reg.Heater, HEX);
  SerialUSB.print("HumidityMeasurementResolution: 0x");
  SerialUSB.println(reg.HumidityMeasurementResolution, HEX);
  SerialUSB.print("TemperatureMeasurementResolution: 0x");
  SerialUSB.println(reg.TemperatureMeasurementResolution, HEX);
#endif // NEED_HDC1080

#ifdef NEED_SSD1306
  oled.println("Sets");
#endif // NEED_SSD1306
  DeserializationError error = deserializeJson(sets, "{\"freq\":[868,868.125,868.125],\"sf\":[12,9,9],\"bw\":[9,8,6]}");
  if (error) {
#ifdef NEED_SSD1306
    oled.println("ndeserializeJson failed");
#endif // NEED_SSD1306
    SerialUSB.println(F("\ndeserializeJson() in Sets failed!"));
    hexDump(msgBuf, 256);
  } else {
    setsFQ = sets["freq"];
    setsSF = sets["sf"];
    setsBW = sets["bw"];
    uint8_t i, j = setsFQ.size();
    SerialUSB.println("\n\n" + String(j) + " Sets:");
    for (i = 0; i < j; i++) {
      float F = setsFQ[i];
      int S = setsSF[i];
      int B = setsBW[i];
      sprintf((char*)msgBuf, " . Freq: %3.3f MHz, SF %d, BW %d: %3.2f", F, S, B, BWs[B]);
      SerialUSB.println((char*)msgBuf);
#ifdef NEED_SSD1306
      oled.print("Freq["); oled.print(i); oled.print("]: "); oled.println(String(F, 3) + " MHz");
      oled.print("SF["); oled.print(i); oled.print("]: "); oled.println(S);
      oled.print("BW["); oled.print(i); oled.print("]: "); oled.print(B); oled.print(" ie "); oled.println(BWs[B]);
#endif // NEED_SSD1306
    }
  }
  SerialUSB.println("Setup done...");
#ifdef NEED_BME
  displayBME680();
  lastReading = millis();
#endif // NEED_BME
#ifdef NEED_DHT
  displayDHT();
  lastReading = millis();
#endif // NEED_DHT
#ifdef NEED_HDC1080
  displayHDC1080();
  lastReading = millis();
#endif // NEED_HDC1080
}

void loop() {
  double t0 = millis();
#ifdef NEED_BME
  if (t0 - lastReading >= PING_DELAY) {
    displayBME680();
    lastReading = millis();
  }
#endif // NEED_BME
#ifdef NEED_DHT
  if (t0 - lastReading >= PING_DELAY) {
    displayDHT();
    lastReading = millis();
  }
#endif // NEED_DHT
#ifdef NEED_HDC1080
  if (t0 - lastReading >= PING_DELAY) {
    displayHDC1080();
    lastReading = millis();
  }
#endif // NEED_HDC1080

  // Uncomment if you have a battery plugged in.
  //  if (millis() - batteryUpdateDelay > 10000) {
  //    getBattery();
  //    batteryUpdateDelay = millis();
  //  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
#ifdef NEED_SSD1306
    oled.print("Incoming! ");
#endif // NEED_SSD1306
    memset(msgBuf, 0xFF, 256);
    int ix = 0;
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      delay(10);
      msgBuf[ix++] = c;
    } msgBuf[ix] = 0;
    int rssi = LoRa.packetRssi();
#ifdef NEED_SSD1306
    oled.print("RSSI: ");
    oled.println(rssi);
#endif // NEED_SSD1306
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Received packet: ");
      hexDump(msgBuf, ix);
    }
    if (needEncryption) {
      if (NEED_DEBUG == 1) {
        SerialUSB.println("\n . Decrypting...");
      }
      packetSize = decryptECB(msgBuf, ix);
      if (packetSize > -1) {
        memset(msgBuf, 0, 256);
        memcpy(msgBuf, encBuf, packetSize);
      } else {
        SerialUSB.println("Error while decrypting");
        return;
      }
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, msgBuf);
    if (error) {
      if (NEED_DEBUG == 1) {
        SerialUSB.print(F("deserializeJson() failed: "));
        SerialUSB.println(error.f_str());
      }
#ifdef NEED_SSD1306
      oled.print("deserializeJson failed");
#endif // NEED_SSD1306
      return;
    }

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
#ifdef NEED_SSD1306
    oled.print("from ");
    oled.print(from);
    oled.print(": ");
    oled.println(cmd);
#endif // NEED_SSD1306
    if (strcmp(cmd, "msg") == 0) {
      const char *msg = doc["msg"];
#ifdef NEED_SSD1306
      oled.println(msg);
#endif // NEED_SSD1306
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
#ifdef NEED_SSD1306
      oled.print("PONG back! ");
#endif // NEED_SSD1306
      // we cannot pong back right away – the message would be lost
      // if there are other devices on the same network
      uint16_t dl = getRamdom16() % 2800 + 3300;
      if (NEED_DEBUG == 1) {
        SerialUSB.println("Delaying " + String(dl) + " millis...");
      }
      delay(dl);
      sendPong((char*)myID, rssi);
      LoRa.receive();
    } else if (strcmp(cmd, "pong") == 0) {
      int rcvRSSI = doc["rcvRSSI"];
      if (NEED_DEBUG == 1) {
        SerialUSB.print("rcvRSSI: ");
        SerialUSB.println(rcvRSSI);
      }
#ifdef NEED_SSD1306
      oled.print("rcvRSSI: ");
      oled.println(rcvRSSI);
#endif // NEED_SSD1306
    } else if (strcmp(cmd, "freq") == 0) {
      // Do we have a frequency change request?
      if (strcmp(from, "BastMobile") != 0) return;
      // Not for you, brah
      mydata = doc["freq"];
      if (mydata.isNull()) {
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['freq']) is null!");
      } else {
        uint32_t fq = mydata.as<float>() * 1e6;
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['freq']) = " + String(fq, 3));
        if (fq < 862e6 || fq > 1020e6) {
          if (NEED_DEBUG == 1) {
            SerialUSB.println("Requested frequency (" + String(fq) + ") is invalid!");
          }
        } else {
          myFreq = fq;
          LoRa.idle();
          LoRa.setFrequency(myFreq);
          delay(100);
          LoRa.receive();
          if (NEED_DEBUG == 1) {
            SerialUSB.println("Frequency set to " + String(myFreq / 1e6, 3) + " MHz");
          }
#ifdef NEED_SSD1306
          oled.print("New freq: ");
          oled.println(String(myFreq / 1e6, 3) + " MHz");
#endif // NEED_SSD1306
          savePrefs();
        }
      }
    } else if (strcmp(cmd, "bw") == 0) {
      // Do we have a bandwidth change request?
      /*
        Note on SF / BW pairs:
        Unless you are sending very small packets, all pairs might not work.
        Here is a table based on empirical results of pairs that work.
         BW|SF|Y/N
         --|--|---
           |9 | Y
         6 |10| N
           |11| N
           |12| N
         --|--|---
           |9 | Y
         7 |10| Y
           |11| N
           |12| N
         --|--|---
           |9 | Y
         8 |10| Y
           |11| Y
           |12| N
         --|--|---
           |9 | Y
         9 |10| Y
           |11| Y
           |12| Y
      */
      if (strcmp(from, "BastMobile") != 0) return;
      // Not for you, brah
      mydata = doc["bw"];
      if (mydata.isNull()) {
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['bw']) is null!");
      } else {
        int bw = mydata.as<int>();
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['bw']) = " + String(bw));
        if (bw < 0 || bw > 9) {
          if (NEED_DEBUG == 1) {
            SerialUSB.println("Requested bandwidth (" + String(bw) + ") is invalid!");
          }
        } else {
          myBW = bw;
          LoRa.idle();
          LoRa.setSignalBandwidth(BWs[myBW] * 1e3);
          delay(100);
          LoRa.receive();
          if (NEED_DEBUG == 1) {
            SerialUSB.println("Bandwidth set to " + String(BWs[myBW], 3) + " KHz");
          }
#ifdef NEED_SSD1306
          oled.print("New BW: ");
          oled.println(String(BWs[myBW], 3) + " KHz");
#endif // NEED_SSD1306
          savePrefs();
        }
      }
    } else if (strcmp(cmd, "sf") == 0) {
      // Do we have a spreading factor change request?
      if (strcmp(from, "BastMobile") != 0) return;
      // Not for you, brah
      mydata = doc["sf"];
      if (mydata.isNull()) {
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['sf']) is null!");
      } else {
        int sf = mydata.as<int>();
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['sf']) = " + String(sf));
        if (sf < 7 || sf > 12) {
          if (NEED_DEBUG == 1) {
            SerialUSB.println("Requested SF (" + String(sf) + ") is invalid!");
          }
        } else {
          mySF = sf;
          LoRa.idle();
          LoRa.setSpreadingFactor(mySF);
          delay(100);
          LoRa.receive();
          if (NEED_DEBUG == 1) {
            SerialUSB.println("SF set to " + String(sf));
          }
#ifdef NEED_SSD1306
          oled.print("New SF: ");
          oled.println(mySF);
#endif // NEED_SSD1306
          savePrefs();
        }
      }
    } else if (strcmp(cmd, "switch") == 0) {
      // Do we have a spreading factor change request?
      if (strcmp(from, "BastMobile") != 0) return;
      // Not for you, brah
      mydata = doc["set"];
      if (mydata.isNull()) {
        if (NEED_DEBUG == 1) SerialUSB.println("mydata (doc['set']) is null!");
      } else {
        int setNum = mydata.as<int>();
        if (NEED_DEBUG == 1) SerialUSB.println("Switching to set #" + String(setNum));
        float F = setsFQ[0];
        int S = setsSF[0];
        int B = setsBW[0];
        if (NEED_DEBUG == 1) {
          sprintf((char*)msgBuf, " . Freq: %3.3f MHz, SF %d, BW %d: %3.2f", F, S, B, BWs[B]);
          SerialUSB.println((char*)msgBuf);
        }
        myFreq = F * 1e6;
        mySF = S;
        myBW = B;
        LoRa.idle();
        LoRa.setFrequency(myFreq);
        delay(10);
        LoRa.setSpreadingFactor(mySF);
        delay(10);
        LoRa.setSignalBandwidth(BWs[myBW] * 1e3);
        delay(10);
        LoRa.receive();
        sendPong((char*)myID, rssi);
#ifdef NEED_SSD1306
        oled.print("New freq: ");
        oled.println(String(myFreq / 1e6, 3) + " MHz");
        oled.print("New SF: ");
        oled.println(mySF);
        oled.print("New BW: ");
        oled.println(String(BWs[myBW], 3) + " KHz");
#endif // NEED_SSD1306
      }
    }
  }
  if (SerialUSB.available()) {
    // When the BastMobile is connected via USB to a computer,
    // you can make changes to settings via Serial,
    // like in BastWAN_Minimal_LoRa
    handleSerial();
  }
  if (needPing) {
    double t0 = millis();
    if (t0 - lastAutoPing > pingFrequency) {
#ifdef NEED_SSD1306
      oled.println("Auto PING!");
#endif // NEED_SSD1306
      sendPing();
      lastAutoPing = millis();
    }
  }
}

#ifdef NEED_BME
void displayBME680() {
#ifdef NEED_SSD1306
  oled.println("displayBME680");
#endif // NEED_SSD1306
  if (NEED_DEBUG == 1) {
    SerialUSB.println("BME680");
  }
  ClosedCube_BME680_Status status = bme680.readStatus();
  if (status.newDataFlag) {
    double temp = bme680.readTemperature();
    double pres = bme680.readPressure();
    double hum = bme680.readHumidity();
    // save the values in the same global array, so that they can be sent in packets
    temp_hum_val[0] = (float)hum;
    temp_hum_val[1] = (float)temp;
#ifdef NEED_SSD1306
    displayHT();
#endif // NEED_SSD1306
    if (NEED_DEBUG == 1) {
      sprintf((char*)msgBuf, "result: T = % f C, RH = % f % %, P = % d hPa\n", temp, hum, pres);
      SerialUSB.println((char*)msgBuf);
    }
    lastReading = millis();
  }
}
#endif

#ifdef NEED_DHT
void displayDHT() {
#ifdef NEED_SSD1306
  oled.println("displayDHT");
#endif // NEED_SSD1306
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  if (dht.readTempAndHumidity(temp_hum_val)) {
    SerialUSB.print("Humidity: ");
    SerialUSB.print(temp_hum_val[0]);
    SerialUSB.print(" % \t");
    SerialUSB.print("Temperature: ");
    SerialUSB.print(temp_hum_val[1]);
    SerialUSB.println(" *C");
#ifdef NEED_SSD1306
    displayHT();
#endif // NEED_SSD1306
  } else {
    SerialUSB.println("Failed to get temperature and humidity value.");
  }
}
#endif // NEED_DHT

#ifdef NEED_HDC1080
void displayHDC1080() {
  char buff[48];
  temp_hum_val[0] = hdc1080.readHumidity();
  temp_hum_val[1] = hdc1080.readTemperature();
  sprintf(buff, "Temp: %2.2f C, Humidity: %2.2f%%\n", temp_hum_val[1], temp_hum_val[0]);
  Serial.print(buff);
#ifdef NEED_SSD1306
  displayHT();
#endif // NEED_SSD1306
}
#endif // NEED_HDC1080

#ifdef NEED_SSD1306
void displayHT() {
  oled.print("H: ");
  oled.print(temp_hum_val[0]);
  oled.print("% T: ");
  oled.print(temp_hum_val[1]);
  oled.println(" *C");
}
#endif // NEED_SSD1306
