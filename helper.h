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
bool pongBack = true;
uint8_t SecretKey[33] = "YELLOW SUBMARINEENIRAMBUS WOLLEY";
uint8_t encBuf[128], hexBuf[256], msgBuf[256];
uint8_t randomStock[256];
uint8_t randomIndex = 0;
float lastBattery = 0.0;
double batteryUpdateDelay;
char deviceName[33];
double myFreq = 863125000;
double mySF = 10;

uint16_t encryptECB(uint8_t*);
void decryptECB(uint8_t*, uint8_t);
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
void setDeviceName(char *);

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
  SerialUSB.print("setPWD: ");
  SerialUSB.println(buff);
  SerialUSB.print("len: ");
  SerialUSB.println(len);
  hexDump((uint8_t *)buff, len);
  if (len == 32) {
    // copy to the SecretKey buffer
    memcpy(SecretKey, buff, 32);
    needEncryption = true;
    hexDump((uint8_t *)SecretKey, 32);
    return;
  }
  if (len == 64) {
    // copy to the SecretKey buffer
    hex2array((uint8_t *)buff, SecretKey, 64);
    needEncryption = true;
    hexDump((uint8_t *)SecretKey, 32);
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
  SerialUSB.println("Before calling encryption. olen = " + String(olen));
  memcpy(msgBuf, encBuf, olen);
  hexDump(msgBuf, olen);

  if (needEncryption) {
    olen = encryptECB((uint8_t*)msgBuf);
    // encBuff = encrypted buffer
    // hexBuff = encBuf, hex encoded
    // olen = len(hexBuf)
  } SerialUSB.println("olen: " + String(olen));
  SerialUSB.print("Sending packet...");
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
  SerialUSB.println(" done!");
  delay(500);
  digitalWrite(LED_BUILTIN, 0);
  LoRa.receive();
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
}

void decryptECB(uint8_t* myBuf, uint8_t olen) {
  SerialUSB.println(" . Decrypting:");
  hexDump(myBuf, olen);
  SerialUSB.println("  - Dehexing myBuf to encBuf:");
  hex2array(myBuf, encBuf, olen);
  uint8_t len = olen / 2;
  hexDump(encBuf, len);
  SerialUSB.println("  - Decrypting encBuf:");
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
}

uint16_t encryptECB(uint8_t* myBuf) {
  // first ascertain length
  uint8_t len = strlen((char*)myBuf);
  uint16_t olen;
  struct AES_ctx ctx;
  // prepare the buffer
  olen = len;
  if (olen != 16) {
    if (olen % 16 > 0) {
      if (olen < 16) olen = 16;
      else olen += 16 - (olen % 16);
    }
  }
  SerialUSB.println("myBuf:");
  hexDump(encBuf, olen);
  SerialUSB.print("[encryptECB]: ");
  SerialUSB.print("olen = " + String(olen));
  SerialUSB.println(", len = " + String(len));
  memset(encBuf, (olen - len), olen);
  memcpy(encBuf, myBuf, len);
  encBuf[len] = 0;
  AES_init_ctx(&ctx, (const uint8_t*)SecretKey);
  uint8_t rounds = olen / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    AES_ECB_encrypt(&ctx, encBuf + steps);
    // void AES_ECB_decrypt(&ctx, encBuf + steps);
    steps += 16;
    // encrypts in place, 16 bytes at a time
  }
  array2hex(encBuf, olen, hexBuf);
  SerialUSB.println("encBuf:");
  hexDump(encBuf, olen);
  SerialUSB.println("hexBuf:");
  hexDump(hexBuf, olen * 2);
  return (olen * 2);
}

void stockUpRandom() {
  fillRandom(randomStock, 256);
  randomIndex = 0;
}

void showHelp() {
  SerialUSB.println("--- HELP ---");
  SerialUSB.println(" Dxxxxxxxxxxx: Set device name");
  SerialUSB.print(" -> right now: "); SerialUSB.println(deviceName);
  SerialUSB.println(" Sxxxxxxxxxxx: send string xxxxxxxxxxx");
  SerialUSB.println(" E           : turn on encryption");
  SerialUSB.println(" e           : turn off encryption");
  SerialUSB.print(" -> right now: "); SerialUSB.println(needEncryption ? "on" : "off");
  SerialUSB.println(" Pxxxxxxx[32]: set password [32 chars]");
  SerialUSB.println(" R           : turn on PONG back [Reply on]");
  SerialUSB.println(" r           : turn off PONG back [reply off]");
  SerialUSB.print(" -> right now: "); SerialUSB.println(pongBack ? "on" : "off");
  SerialUSB.println(" Fxxx.yyy    : Set a new LoRa frequency.");
  SerialUSB.print(" -> right now: "); SerialUSB.println(myFreq / 1e6, 3);
  SerialUSB.println(" Sxx         : Set a new LoRa Spreading Factor.");
  SerialUSB.print(" -> right now: "); SerialUSB.println(mySF);
  SerialUSB.println(" Else        : show this help");
  SerialUSB.println(" p           : send PING packet with counter & frequency");
}

void setPongBack(bool x) {
  pongBack = x;
  SerialUSB.print("PONG back set to ");
  if (x) SerialUSB.println("true");
  else SerialUSB.println("false");
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
    SerialUSB.println("Last Battery: " + String(lastBattery) + " vs current: " + String(battery));
    lastBattery = battery;
  }
}

void setFQ(char* buff) {
  float fq = atof(buff);
  // RAK4260: 862 to 1020 MHz frequency coverage
  // clearFrame();
  if (fq < 862.0 || fq > 1020.0) {
    SerialUSB.println("Requested frequency (" + String(buff) + ") is invalid!");
  } else {
    myFreq = fq * 1e6;
    LoRa.idle();
    LoRa.setFrequency(myFreq);
    delay(100);
    LoRa.receive();
    SerialUSB.println("Frequency set to " + String(fq, 3) + " MHz");
  }
}

void setSF(char* buff) {
  int sf = atoi(buff);
  // SF 7 to 12
  // clearFrame();
  if (sf < 7 || sf > 12) {
    SerialUSB.println("Requested SF (" + String(buff) + ") is invalid!");
  } else {
    mySF = sf;
    LoRa.idle();
    LoRa.setSpreadingFactor(mySF);
    delay(100);
    LoRa.receive();
    SerialUSB.println("SF set to " + String(mySF));
  }
}

void setDeviceName(char *truc) {
  memset(deviceName, 0, 33);
  memcpy(deviceName, truc, strlen(truc));
  SerialUSB.print("Device Name set to: ");
  SerialUSB.println(deviceName);
}

void sendPing() {
  // PING!
  string answer = "PING #";
  answer.append(to_string(pingCounter++));
  answer.append(" from ");
  answer.append(deviceName);
  answer.append(" at ");
  string fk = to_string(myFreq);
  answer.append(fk.substr(0, 3));
  answer.append(".");
  answer.append(fk.substr(3, 3));
  answer.append(" MHz");
  SerialUSB.println("Sending...");
  SerialUSB.println(myFreq);
  SerialUSB.println((char*)answer.c_str());
  SerialUSB.println("PING sent!");
}
