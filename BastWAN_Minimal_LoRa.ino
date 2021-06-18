// Uncomment the next line if uploading to Pavel
//#define Pavel 1
// Uncomment this next line if you want to use a BME680
//#define NEED_BME 1
// Uncomment this next line if you want to use a DHT22
//#define NEED_DHT

#include <SPI.h>
#include <LoRa.h>
#include <LoRandom.h>
#include "aes.c"
#include "sha2.c"
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

#ifdef NEED_BME
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

#ifdef NEED_DHT
#include "DHT.h"
#define DHTPIN 9 // what pin we're connected to
#define DHTTYPE DHT22 // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);
#define DHT_PING_DELAY 300000 // 5 minutes
double lastReading = DHT_PING_DELAY * -1;
float temp_hum_val[2] = {0};
#endif

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
      SerialUSB.println("   No memory detected. Freezing.");
    }
    while (1)
      ;
  }
  uint32_t myLen = myMem.length(), index = 0;
  if (NEED_DEBUG == 1) {
    SerialUSB.println("Memory detected!");
    SerialUSB.print("Mem size in bytes: ");
    SerialUSB.println(myLen);
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
  myCR = doc["myCR"];
  const char *x = doc["deviceName"];
  memcpy(deviceName, x, 33);
  if (NEED_DEBUG == 1) {
    SerialUSB.print("FQ: "); SerialUSB.println(myFreq / 1e6);
    SerialUSB.print("SF: "); SerialUSB.println(mySF);
    SerialUSB.print("BW: "); SerialUSB.println(myBW);
    SerialUSB.print("Device Name: "); SerialUSB.println(deviceName);
  }
#endif

#ifdef NEED_BME
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
  LoRa.setSpreadingFactor(mySF);
  LoRa.setSignalBandwidth(BWs[myBW] * 1e3);
  LoRa.setCodingRate4(myCR);
  LoRa.setPreambleLength(8);
  LoRa.setTxPower(TxPower, PA_OUTPUT_PA_BOOST_PIN);
  digitalWrite(RFM_SWITCH, HIGH);
  if (PA_BOOST) LoRa.setTxPower(TxPower, PA_OUTPUT_PA_BOOST_PIN);
  else LoRa.setTxPower(TxPower, 0); // NOT RECOMMENDED!
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
  if (OCP_ON) LoRa.writeRegister(REG_OCP, 0b00111111); // OCP Max 240
  else LoRa.writeRegister(REG_OCP, 0b00011111); // NO OCP
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
  setDeviceName("Pablo");
#endif
#ifdef NEED_DHT
  dht.begin();
#endif
}

void loop() {
  double t0 = millis();
#ifdef NEED_BME
  if (t0 - lastReading >= BME_PING_DELAY) {
    displayBME680();
    lastReading = millis();
  }
#endif
#ifdef NEED_DHT
  if (t0 - lastReading >= DHT_PING_DELAY) {
    displayDHT();
    lastReading = millis();
  }
#endif

  // Uncomment if you have a battery plugged in.
  //  if (millis() - batteryUpdateDelay > 10000) {
  //    getBattery();
  //    batteryUpdateDelay = millis();
  //  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    memset(msgBuf, 0xFF, 256);
    int ix = 0;
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      delay(10);
      msgBuf[ix++] = c;
    } msgBuf[ix] = 0;
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
          savePrefs();
        }
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
      sendPing();
      lastAutoPing = millis();
    }
  }
}

#ifdef NEED_BME
void displayBME680() {
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
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  if (!dht.readTempAndHumidity(temp_hum_val)) {
    SerialUSB.print("Humidity: ");
    SerialUSB.print(temp_hum_val[0]);
    SerialUSB.print(" % \t");
    SerialUSB.print("Temperature: ");
    SerialUSB.print(temp_hum_val[1]);
    SerialUSB.println(" *C");
  } else {
    SerialUSB.println("Failed to get temperature and humidity value.");
  }
}
#endif
