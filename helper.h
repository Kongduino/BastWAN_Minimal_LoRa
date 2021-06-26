#undef max
#undef min
#include <string>
#include <vector>

uint8_t NEED_DEBUG = 1;

using namespace std;
template class basic_string<char>; // https://github.com/esp8266/Arduino/issues/1136
// Required or the code won't compile!
namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION
//void __throw_bad_alloc() {}
}

/*
  NOTE!
  Add:
  namespace std _GLIBCXX_VISIBILITY(default) {
    _GLIBCXX_BEGIN_NAMESPACE_VERSION
    void __throw_length_error(char const*) {
    }
    void __throw_out_of_range(char const*) {
    }
    void __throw_logic_error(char const*) {
    }
    void __throw_out_of_range_fmt(char const*, ...) {
    }
  }
  to ~/Library/Arduino15/packages/arduino/tools/arm-none-eabi-gcc/4.8.3-2014q1/arm-none-eabi/include/c++/4.8.3/bits/basic_string.h
  Or the code won't compile.
*/

#define REG_OCP 0x0B
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0c
#define REG_OP_MODE 0x01
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_MODEM_CONFIG_3 0x26
#define REG_PA_DAC 0x4D
#define PA_DAC_HIGH 0x87

#define RFM_TCXO (40u)
#define RFM_SWITCH (41u)
#define CBC 0
#define CTR 1
#define ECB 2

uint8_t myMode = ECB;
bool needEncryption = true;
bool needAuthentification = true;
bool pongBack = true;
bool OCP_ON = false, PA_BOOST = true;
uint8_t SecretKey[33] = "YELLOW SUBMARINEENIRAMBUS WOLLEY";
uint8_t encBuf[128], hexBuf[256], msgBuf[256];
uint8_t randomStock[256];
uint8_t randomIndex = 0;
float lastBattery = 0.0;
double batteryUpdateDelay;
char deviceName[33];
uint32_t myFreq = 868125000;
int mySF = 10;
uint8_t myBW = 7;
uint8_t myCR = 5;
double BWs[10] = {
  7.8, 10.4, 15.6, 20.8, 31.25,
  41.7, 62.5, 125.0, 250.0, 500.0
};
uint16_t pingCounter = 0;
uint16_t pingFrequency = 0;
bool needPing = false;
double lastAutoPing = 0;
float homeLatitude = 22.4591126;
float homeLongitude = 114.0003769;
uint8_t TxPower = 20;

// Sets of Freq / SF / BW settings
StaticJsonDocument<256>sets;

uint16_t encryptECB(uint8_t*);
int16_t decryptECB(uint8_t*, uint8_t);
void array2hex(uint8_t *, size_t, uint8_t *, uint8_t);
void hex2array(uint8_t *, uint8_t *, size_t);
void sendPacket(char *);
void setPWD(char *);
void setPongBack(bool);
void stockUpRandom();
void showHelp();
uint8_t getRandomByte();
uint16_t getRamdom16();
void getRandomBytes(uint8_t *buff, uint8_t count);
void getBattery();
void setFQ(char*);
void setSF(char*);
void setTxPower(char* buff);
void setBW(char* buff);
void setCR(char* buff);
void setDeviceName(char *);
void sendJSONPacket();
void savePrefs();
void setAutoPing(char *);
void handleSerial();

void writeRegister(uint8_t reg, uint8_t value) {
  LoRa.writeRegister(reg, value);
}
uint8_t readRegister(uint8_t reg) {
  return LoRa.readRegister(reg);
}

void hex2array(uint8_t *src, uint8_t *dst, size_t sLen) {
  size_t i, n = 0;
  for (i = 0; i < sLen; i += 2) {
    uint8_t x, c;
    c = src[i];
    if (c != '-') {
      if (c > 0x39) c -= 55;
      else c -= 0x30;
      x = c << 4;
      c = src[i + 1];
      if (c > 0x39) c -= 55;
      else c -= 0x30;
      dst[n++] = (x + c);
    }
  }
}

void array2hex(uint8_t *inBuf, size_t sLen, uint8_t *outBuf, uint8_t dashFreq = 0) {
  size_t i, len, n = 0;
  const char * hex = "0123456789ABCDEF";
  for (i = 0; i < sLen; ++i) {
    outBuf[n++] = hex[(inBuf[i] >> 4) & 0xF];
    outBuf[n++] = hex[inBuf[i] & 0xF];
    if (dashFreq > 0 && i != sLen - 1) {
      if ((i + 1) % dashFreq == 0) outBuf[n++] = '-';
    }
  }
  outBuf[n++] = 0;
}

void setPWD(char *buff) {
  // buff can be 32 or 64 bytes:
  // 32 bytes = plain text
  // 64 bytes = hex-encoded
  uint8_t len = strlen(buff), i;
  for (i = 0; i < len; i++) {
    if (buff[i] < 32) {
      buff[i] = 0;
      i = len + 1;
    }
  }
  len = strlen(buff);
  if (NEED_DEBUG == 1) {
    SerialUSB.print("setPWD: ");
    SerialUSB.println(buff);
    SerialUSB.print("len: ");
    SerialUSB.println(len);
    hexDump((uint8_t *)buff, len);
  }
  if (len == 32) {
    // copy to the SecretKey buffer
    memcpy(SecretKey, buff, 32);
    needEncryption = true;
    if (NEED_DEBUG == 1) {
      hexDump((uint8_t *)SecretKey, 32);
    }
    return;
  }
  if (len == 64) {
    // copy to the SecretKey buffer
    hex2array((uint8_t *)buff, SecretKey, 64);
    needEncryption = true;
    if (NEED_DEBUG == 1) {
      hexDump((uint8_t *)SecretKey, 32);
    }
    return;
  }
}

void sendPacket(char *buff) {
  LoRa.idle();
  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
  uint16_t olen = strlen(buff);
  memcpy(encBuf + 8, buff, olen);
  // prepend UUID
  // 4 bytes --> 8 bytes
  uint8_t ix = 0;
  getRandomBytes(encBuf, 4);
  //  encBuf[ix++] = getRandomByte();
  //  encBuf[ix++] = getRandomByte();
  //  encBuf[ix++] = getRandomByte();
  //  encBuf[ix++] = getRandomByte();
  array2hex(encBuf, 4, hexBuf);
  memcpy(encBuf, hexBuf, 8);
  olen += 8;
  if (NEED_DEBUG == 1) {
    SerialUSB.println("Before calling encryption. olen = " + String(olen));
  }
  memcpy(msgBuf, encBuf, olen);
  if (NEED_DEBUG == 1) {
    hexDump(msgBuf, olen);
  }
  if (needEncryption) {
    olen = encryptECB((uint8_t*)msgBuf);
    // encBuff = encrypted buffer
    // hexBuff = encBuf, hex encoded
    // olen = len(hexBuf)
  }
  if (NEED_DEBUG == 1) {
    SerialUSB.println("olen: " + String(olen));
    SerialUSB.print("Sending packet...");
  }
  // Now send a packet
  digitalWrite(LED_BUILTIN, 1);
  //digitalWrite(PIN_PA28, LOW);
  digitalWrite(RFM_SWITCH, 0);
  LoRa.beginPacket();
  if (needEncryption) {
    //LoRa.print((char*)hexBuf);
    LoRa.write(hexBuf, olen);
  } else {
    LoRa.write((uint8_t *)buff, olen);
  }
  LoRa.endPacket();
  digitalWrite(RFM_SWITCH, 1);
  //digitalWrite(PIN_PA28, HIGH);
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" done!");
  }
  delay(500);
  digitalWrite(LED_BUILTIN, 0);
  LoRa.receive();
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
}

int16_t decryptECB(uint8_t* myBuf, uint8_t olen) {
  hexDump(myBuf, olen);
  // Test the total len vs requirements:
  // AES: min 16 bytes
  // HMAC if needed: 28 bytes
  uint8_t reqLen = 16 + needAuthentification ? 28 : 0;
  printf("reqLen: %d\n", reqLen);
  if (olen < reqLen) return -1;
  uint8_t len;

  // or just copy over
  memcpy(encBuf, myBuf, olen);
  len = olen;
  if (needAuthentification) {
    // hmac the buffer minus SHA2xx_DIGEST_SIZE
    // compare with the last SHA2xx_DIGEST_SIZE bytes
    // if correct, reduce len by SHA2xx_DIGEST_SIZE
    unsigned char key[20];
    unsigned char digest[SHA224_DIGEST_SIZE];
    unsigned char mac[SHA224_DIGEST_SIZE];
    memset(key, 0x0b, 20);// set up key
    SerialUSB.println("Original HMAC:"); hexDump((unsigned char *)encBuf + len - SHA224_DIGEST_SIZE, SHA224_DIGEST_SIZE);
    hmac_sha224(key, 20, (unsigned char *)encBuf, len - SHA224_DIGEST_SIZE, mac, SHA224_DIGEST_SIZE);
    SerialUSB.println("HMAC:"); hexDump(mac, SHA224_DIGEST_SIZE);
    if (memcmp(mac, encBuf + len - SHA224_DIGEST_SIZE, SHA224_DIGEST_SIZE) == 0) SerialUSB.println(" * test passed");
    else {
      SerialUSB.println(" * test failed");
      // notify we failed
      return -1;
    }
    // deduct SHA224_DIGEST_SIZE from length
    len -= SHA224_DIGEST_SIZE;
  }
  hexDump(encBuf, len);

  // hexDump(encBuf, len);
  // SerialUSB.print("  - Decrypting encBuf with SecretKey: ");
  // SerialUSB.println((char*)SecretKey);
  struct AES_ctx ctx;
  AES_init_ctx(&ctx, SecretKey);
  uint8_t rounds = len / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    //void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
    AES_ECB_decrypt(&ctx, (uint8_t*)encBuf + steps);
    steps += 16;
    // encrypts in place, 16 bytes at a time
  } encBuf[steps] = 0;
  hexDump(encBuf, len);
  return len;
}

uint16_t encryptECB(uint8_t* myBuf) {
  // first ascertain length
  uint8_t len = strlen((char*)myBuf);
  uint16_t olen;
  struct AES_ctx ctx;
  olen = len;
  if (olen != 16) {
    if (olen % 16 > 0) {
      if (olen < 16) olen = 16;
      else olen += 16 - (olen % 16);
    }
  }
  memset(encBuf, (olen - len), olen);
  memcpy(encBuf, myBuf, len);
  // SerialUSB.println("myBuf:");
  // hexDump(encBuf, olen);
  encBuf[len] = 0;
  AES_init_ctx(&ctx, (const uint8_t*)SecretKey);
  uint8_t rounds = olen / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    AES_ECB_encrypt(&ctx, encBuf + steps);
    // void AES_ECB_decrypt(&ctx, encBuf + steps);
    steps += 16;
    // encrypts in place, 16 bytes at a time
  }
  SerialUSB.println("encBuf:");
  hexDump(encBuf, olen);

  // Now do we have to add a MAC?
  if (needAuthentification) {
    // hmac the buffer
    // increase len by SHA2xx_DIGEST_SIZE
    unsigned char key[20];
    unsigned char digest[SHA224_DIGEST_SIZE];
    unsigned char mac[SHA224_DIGEST_SIZE];
    memset(key, 0x0b, 20);// set up key
    // authenticate in place
    // at offset olen
    // OF COURSE IT'D BE NICE TO CHECK THAT OLEN+SHA224_DIGEST_SIZE DOESN'T BLOW UP THE BUFFER
    hmac_sha224(key, 20, (unsigned char *)encBuf, olen, (unsigned char*)encBuf + olen, SHA224_DIGEST_SIZE);
    SerialUSB.println("MAC, plain [" + String(olen) + "]:");
    hexDump((unsigned char*)encBuf + olen, SHA224_DIGEST_SIZE);
    olen += SHA224_DIGEST_SIZE;
    SerialUSB.println("encBuf with MAC:");
    hexDump(encBuf, olen);
  }
  return olen;
}

void stockUpRandom() {
  fillRandom(randomStock, 256);
  randomIndex = 0;
}

void setPongBack(bool x) {
  pongBack = x;
  if (NEED_DEBUG == 1) {
    SerialUSB.print("PONG back set to ");
    if (x) SerialUSB.println("true");
    else SerialUSB.println("false");
  }
}

uint8_t getRandomByte() {
  uint8_t r = randomStock[randomIndex++];
  // reset random stock automatically if needed
  if (randomIndex > 254) stockUpRandom();
  return r;
}

uint16_t getRamdom16() {
  uint8_t r0 = randomStock[randomIndex++];
  // reset random stock automatically if needed
  if (randomIndex > 254) stockUpRandom();
  uint8_t r1 = randomStock[randomIndex++];
  // reset random stock automatically if needed
  if (randomIndex > 254) stockUpRandom();
  return r1 * 256 + r0;
}

void getRandomBytes(uint8_t *buff, uint8_t count) {
  uint8_t r;
  for (uint8_t i = 0; i < count; i++) {
    buff[i] = randomStock[randomIndex++];
    // reset random stock automatically if needed
    if (randomIndex > 254) stockUpRandom();
  }
}

void getBattery() {
  float battery = analogRead(A0);
  if (battery != lastBattery) {
    // update visually etc.
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Last Battery: " + String(lastBattery) + " vs current: " + String(battery));
    }
    lastBattery = battery;
  }
}

void setFQ(char* buff) {
  uint32_t fq = (uint32_t)(atof(buff) * 1e6);
  // RAK4260: 862 to 1020 MHz frequency coverage
  // clearFrame();
  if (fq < 862e6 || fq > 1020e6) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Requested frequency (" + String(buff) + ") is invalid!");
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

void setSF(char* buff) {
  int sf = atoi(buff);
  // SF 7 to 12
  // clearFrame();
  if (sf < 7 || sf > 12) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Requested SF (" + String(buff) + ") is invalid!");
    }
  } else {
    mySF = sf;
    LoRa.idle();
    LoRa.setSpreadingFactor(mySF);
    delay(100);
    LoRa.receive();
    if (NEED_DEBUG == 1) {
      SerialUSB.println("SF set to " + String(mySF));
    }
    savePrefs();
  }
}

void setTxPower(char* buff) {
  int txp = atoi(buff);
  if (txp < 7 || txp > 23) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Requested TxPower (" + String(txp) + ") is invalid!");
    }
  } else {
    String s = "TxPower set to: " + String(txp);
    TxPower = txp;
    LoRa.idle();
    LoRa.setTxPower(TxPower);
    delay(100);
    LoRa.receive();
    if (NEED_DEBUG == 1) {
      SerialUSB.println("TxPower set to " + String(TxPower));
    }
  }
}

void setCR(char* buff) {
  int cr = atoi(buff);
  // clearFrame();
  if (cr < 5 || cr > 8) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Requested C/R (" + String(cr) + ") is invalid!");
    }
  } else {
    String s = "C/R set to: " + String(cr);
    myCR = cr;
    LoRa.idle();
    LoRa.setCodingRate4(cr);
    delay(100);
    LoRa.receive();
    if (NEED_DEBUG == 1) {
      SerialUSB.println("C/R set to " + String(cr));
    }
    savePrefs();
  }
}

void setBW(char* buff) {
  int bw = atoi(buff);
  /*Signal bandwidth:
    0000  7.8 kHz
    0001  10.4 kHz
    0010  15.6 kHz
    0011  20.8kHz
    0100  31.25 kHz
    0101  41.7 kHz
    0110  62.5 kHz
    0111  125 kHz
    1000  250 kHz
    1001  500 kHz
  */
  // clearFrame();
  if (bw < 0 || bw > 9) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Requested BW (" + String(bw) + ") is invalid!");
    }
  } else {
    String s = "BW set to: " + String(bw);
    myBW = bw;
    LoRa.idle();
    LoRa.setSignalBandwidth(BWs[myBW] * 1e3);
    delay(100);
    LoRa.receive();
    if (NEED_DEBUG == 1) {
      SerialUSB.println("BW set to " + String(BWs[myBW])) + " KHz";
    }
    savePrefs();
  }
}

void setDeviceName(char *truc) {
  memset(deviceName, 0, 33);
  memcpy(deviceName, truc, strlen(truc));
  if (NEED_DEBUG == 1) {
    SerialUSB.print("Device Name set to: ");
    SerialUSB.println(deviceName);
  }
  savePrefs();
}

void prepareJSONPacket(char *buff) {
  StaticJsonDocument<256> doc;
  memset(msgBuf, 0, 256);
  char myID[9];
  getRandomBytes(encBuf, 4);
  array2hex(encBuf, 4, (uint8_t*)myID);
  myID[8] = 0;
  doc["UUID"] = myID;
  doc["cmd"] = "msg";
  doc["msg"] = buff;
  doc["from"] = deviceName;
  serializeJson(doc, (char*)msgBuf, 256);
  sendJSONPacket();
}

void sendJSONPacket() {
  if (NEED_DEBUG == 1) {
    SerialUSB.println("Sending JSON Packet... ");
  }
  LoRa.idle();
  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
  digitalWrite(RFM_SWITCH, LOW);
  uint16_t olen = strlen((char*)msgBuf);
  if (NEED_DEBUG == 1) {
    hexDump(msgBuf, olen);
  }
  if (needEncryption) {
    olen = encryptECB((uint8_t*)msgBuf);
    // encBuff = encrypted buffer
    // hexBuff = encBuf, hex encoded
    // olen = len(hexBuf)
  } // SerialUSB.println("olen: " + String(olen));
  if (NEED_DEBUG == 1) {
    SerialUSB.println("Sending packet...");
  }
  // Now send a packet
  digitalWrite(LED_BUILTIN, 1);
  //digitalWrite(PIN_PA28, LOW);
  digitalWrite(RFM_SWITCH, 0);
  LoRa.beginPacket();
  if (needEncryption) {
    //LoRa.print((char*)hexBuf);
    // hexDump(encBuf, olen);
    LoRa.write(encBuf, olen);
  } else {
    //LoRa.print(buff);
    LoRa.write(msgBuf, olen);
  }
  LoRa.endPacket();
  /*
    RegRssiValue (0x1B)
    Current RSSI value (dBm)
    RSSI[dBm] = -157 + Rssi (using HF output port) or
    RSSI[dBm] = -164 + Rssi (using LF output port)
    Let's see if it has any meaning
  */
  digitalWrite(RFM_SWITCH, HIGH);
  //digitalWrite(PIN_PA28, HIGH);
  if (NEED_DEBUG == 1) {
    SerialUSB.println(" done!");
  }
  delay(500);
  digitalWrite(LED_BUILTIN, 0);
  LoRa.receive();
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
}

void sendPing() {
  // PING!
  StaticJsonDocument<256> doc;
  char myID[9];
  getRandomBytes(encBuf, 4);
  array2hex(encBuf, 4, (uint8_t*)myID);
  myID[8] = 0;
  doc["UUID"] = myID;
  doc["cmd"] = "ping";
  doc["from"] = deviceName;
  char freq[8];
  snprintf( freq, 8, "%f", float(myFreq / 1e6) );
  doc["freq"] = freq;

  // Lat/Long are hard-coded for the moment
  doc["lat"] = homeLatitude;
  doc["long"] = homeLongitude;
#ifdef NEED_DHT || NEED_BMP
  doc["H"] = temp_hum_val[0];
  doc["T"] = temp_hum_val[1];
#endif

  serializeJson(doc, (char*)msgBuf, 256);
  sendJSONPacket();
  if (NEED_DEBUG == 1) {
    SerialUSB.println("PING sent!");
  }
  delay(1000);
}

void sendPong(char *msgID, int rssi) {
  // PONG!
  StaticJsonDocument<256> doc;
  char myID[9];
  getRandomBytes(encBuf, 4);
  array2hex(encBuf, 4, (uint8_t*)myID);
  myID[8] = 0;
  doc["UUID"] = msgID ;
  doc["cmd"] = "pong";
  doc["from"] = deviceName;
  doc["rcvRSSI"] = rssi;
#ifdef NEED_DHT || NEED_BMP
  doc["H"] = temp_hum_val[0];
  doc["T"] = temp_hum_val[1];
#endif
  //  char freq[8];
  //  snprintf( freq, 8, "%f", float(myFreq / 1e6) );
  //  doc["freq"] = freq;
  serializeJson(doc, (char*)msgBuf, 256);
  sendJSONPacket();
  if (NEED_DEBUG == 1) {
    SerialUSB.println("PONG sent!");
  }
  delay(1000);
}

void savePrefs() {
  //  SerialUSB.println("Saving prefs:");
  //  StaticJsonDocument<200> doc;
  //  doc["myFreq"] = myFreq;
  //  doc["mySF"] = mySF;
  //  doc["myBW"] = myBW;
  //  doc["deviceName"] = deviceName;
  //  memset(msgBuf, 0, 97);
  //  serializeJson(doc, (char*)msgBuf, 97);
  //  hexDump(msgBuf, 96);
  //  myMem.write(0, msgBuf, 32);
  //  myMem.write(32, msgBuf + 32, 32);
  //  myMem.write(64, msgBuf + 64, 32);
}

void setAutoPing(char* buff) {
  uint16_t fq = (uint16_t)(atof(buff) * 1e3);
  if (NEED_DEBUG == 1) {
    SerialUSB.print("PING frequency: ");
    SerialUSB.print(buff);
    SerialUSB.print(" --> ");
    SerialUSB.println(fq);
  }
  if (fq == 0) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Turning auto PING off!");
    }
    needPing = false;
    return;
  }
  if (fq < 5000 || fq > 60000) {
    if (NEED_DEBUG == 1) {
      SerialUSB.println("Invalid frequency!");
    }
    return;
  }
  if (NEED_DEBUG == 1) {
    SerialUSB.print("Turning auto PING on, every ");
    SerialUSB.print((uint16_t)fq / 1e3);
    SerialUSB.println(" seconds.");
  }
  pingFrequency = fq;
  needPing = true;
}
