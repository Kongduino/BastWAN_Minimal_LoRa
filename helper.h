#define RFM_TCXO (40u)
#define RFM_SWITCH (41u)
#define CBC 0
#define CTR 1
#define ECB 2

uint8_t myMode = ECB;
bool needEncryption = true;
uint8_t SecretKey[33] = "YELLOW SUBMARINEENIRAMBUS WOLLEY";
uint8_t encBuf[128], hexBuf[256];

void hexDump(uint8_t *, uint16_t);
uint16_t encryptECB(uint8_t*);
void decryptECB(uint8_t*, uint8_t);
void array2hex(uint8_t *, size_t, uint8_t *, uint8_t);
void hex2array(uint8_t *, uint8_t *, size_t);
void sendPacket(char *);
void setPWD(char *);

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

void decryptECB(uint8_t* myBuf, uint8_t olen) {
  Serial.println(" . Decrypting:");
  hexDump(myBuf, olen);
  Serial.println("  - Dehexing myBuf to encBuf:");
  hex2array(myBuf, encBuf, olen);
  uint8_t len = olen / 2;
  hexDump(encBuf, len);
  Serial.println("  - Decrypting encBuf:");
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
  uint8_t len = 0;
  uint16_t olen;
  struct AES_ctx ctx;
  // first ascertain length
  while (myBuf[len] > 31) len += 1;
  // prepare the buffer
  myBuf[len] = 0;
  olen = len;
  if (olen != 16) {
    if (olen % 16 > 0) {
      if (olen < 16) olen = 16;
      else olen += 16 - (olen % 16);
    }
  }
  memset(encBuf, (olen - len), olen);
  memcpy(encBuf, myBuf, len);
  AES_init_ctx(&ctx, (const uint8_t*)SecretKey);
  uint8_t rounds = olen / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    AES_ECB_encrypt(&ctx, encBuf + steps);
    // void AES_ECB_decrypt(&ctx, encBuf + steps);
    steps += 16;
    // encrypts in place, 16 bytes at a time
  }
  array2hex(encBuf, olen, hexBuf);
  Serial.println("encBuf:");
  hexDump(encBuf, olen);
  Serial.println("hexBuf:");
  hexDump(hexBuf, olen * 2);
  return (olen * 2);
}

void hexDump(uint8_t *buf, uint16_t len) {
  String s = "|", t = "| |";
  Serial.println(F("  |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f |"));
  Serial.println(F("  +------------------------------------------------+ +----------------+"));
  for (uint16_t i = 0; i < len; i += 16) {
    for (uint8_t j = 0; j < 16; j++) {
      if (i + j >= len) {
        s = s + "   "; t = t + " ";
      } else {
        char c = buf[i + j];
        if (c < 16) s = s + "0";
        s = s + String(c, HEX) + " ";
        if (c < 32 || c > 127) t = t + ".";
        else t = t + (char)c;
      }
    }
    uint8_t index = i / 16;
    Serial.print(index, HEX); Serial.write('.');
    Serial.println(s + t + "|");
    s = "|"; t = "| |";
  }
  Serial.println(F("  +------------------------------------------------+ +----------------+\n"));
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
  Serial.print("setPWD: ");
  Serial.println(buff);
  Serial.print("len: ");
  Serial.println(len);
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
  uint16_t olen = strlen(buff);
  if (needEncryption) {
    olen = encryptECB((uint8_t*)buff);
    // encBuff = encrypted buffer
    // hexBuff = encBuf, hex encoded
    // olen = len(hexBuf)
  } Serial.println("olen: " + String(olen));
  Serial.print("Sending packet...");
  // Now send a packet
  digitalWrite(LED_BUILTIN, 1);
  //digitalWrite(PIN_PA28, LOW);
  digitalWrite(RFM_SWITCH, 0);
  LoRa.beginPacket();
  if (needEncryption) {
    //LoRa.print((char*)hexBuf);
    LoRa.write(hexBuf, olen);
  } else {
    //LoRa.print(buff);
    LoRa.write((uint8_t *)buff, olen);
  }
  LoRa.endPacket();
  digitalWrite(RFM_SWITCH, 1);
  //digitalWrite(PIN_PA28, HIGH);
  Serial.println(" done!");
  delay(500);
  digitalWrite(LED_BUILTIN, 0);
  if (needEncryption) {
    SerialUSB.println((char*)hexBuf);
  } else {
    SerialUSB.println(buff);
  }
}

void showHelp() {
  Serial.println("--- HELP ---");
  Serial.println(" Sxxxxxxxxxxx: send string xxxxxxxxxxx");
  Serial.println(" E           : turn on encryption");
  Serial.println(" e           : turn off encryption");
  Serial.println(" Pxxxxxxx[32]: set password [32 chars]");
  Serial.println(" Else        : show this help");
}
