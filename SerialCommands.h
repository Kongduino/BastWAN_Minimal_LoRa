
void handleSerial() {
  memset(msgBuf, 0, 256);
  int ix = 0;
  while (SerialUSB.available() && ix < 255) {
    char c = SerialUSB.read();
    delay(10);
    if (c > 31) msgBuf[ix++] = c;
  } msgBuf[ix] = 0;
  char c = msgBuf[0]; // Command
  if (c == '/') {
    c = msgBuf[1];
    char c1 = msgBuf[2];
    if (c == '>') {
      char buff[256];
      strcpy(buff, (char*)msgBuf + 2);
      prepareJSONPacket(buff);
      sendJSONPacket();
      return;
    } else if (c == 'D' && c1 == 'N') {
      setDeviceName((char*)msgBuf + 3);
      return;
    } else if (c == 'E' || c == 'e') {
      if (c1 = '1') needEncryption = true;
      if (c1 = '0') needEncryption = false;
      return;
    } else if (strcmp((char*)msgBuf, "/HX1") == 0) {
      needHexification = true;
      char buff[64];
      SerialUSB.println("needHexification set to: ON");
      return;
    } else if (strcmp((char*)msgBuf, "/HX0") == 0) {
      needHexification = false;
      SerialUSB.println("needHexification set to: OFF");
      return;
    } else if (c == 'R' || c == 'r') {
      if (c1 = '1') setPongBack(true);
      if (c1 = '0') setPongBack(false);
      return;
    } else if (c == 'F' && c1 == 'Q') {
      setFQ((char*)msgBuf + 3);
      return;
    } else if (c == 'S' && c1 == 'F') {
      setSF((char*)msgBuf + 3);
      return;
    } else if (c == 'B' && c1 == 'W') {
      setBW((char*)msgBuf + 3);
      return;
    } else if (c == 'C' && c1 == 'R') {
      setCR((char*)msgBuf + 3);
      return;
    } else if (c == 'T' && c1 == '0') {
      setTxPower((char*)msgBuf + 3);
      return;
    } else if (c == 'p' || c == 'P') {
      if (c1 < 32) {
        sendPing();
        return;
      } else if (c1 == 'w' || c1 == 'W') {
        setPWD((char*)msgBuf + 2);
        return;
      } else if (c1 == 'b' || c1 == 'B') {
        if (msgBuf[3] == '1') {
          SerialUSB.println("--> PA_BOOST on");
          LoRa.setTxPower(TxPower, 1);
        } else if (msgBuf[3] == '0') {
          SerialUSB.println("--> PA_BOOST off, RFO pin");
          LoRa.setTxPower(TxPower, 0);
        }
        return;
      }
    } else if (c == 'O' && c1 == 'C') {
      if (msgBuf[3] == '1') {
        // OCP ON
        OCP_ON = true;
        LoRa.idle();
        LoRa.writeRegister(REG_OCP, 0b00111111); // MAX OCP
        LoRa.receive();
        SerialUSB.println("--> OCP Trim on, Max OCP");
        return;
      } else if (msgBuf[3] == '0') {
        // OCP Trim OFF
        OCP_ON = false;
        LoRa.idle();
        LoRa.writeRegister(REG_OCP, 0b00011111); // NO OCP
        LoRa.receive();
        SerialUSB.println("--> OCP Trim off");
        return;
      }
    }
    //      if (c == 'L') {
    //        // Backlight on
    //        LoRa.idle();
    //        ts.writeRegister8(GPIO_ALT_FUNCT, 0b00001000); // Set GPIO-3 to alternate function
    //        ts.writeRegister8(GPIO_SET_PIN, 0b00001000);
    //        SerialUSB.println("--> Backlight ON");
    //        return;
    //      }
    //      if (c == 'l') {
    //        // Backlight off
    //        LoRa.idle();
    //        ts.writeRegister8(GPIO_ALT_FUNCT, 0b00001000); // Set GPIO-3 to alternate function
    //        ts.writeRegister8(GPIO_CLR_PIN, 0b00001000);
    //        ts.writeRegister8(GPIO_ALT_FUNCT, 0b00000000); // Set GPIO-3 to alternate function
    //        ts.writeRegister8(GPIO_CLR_PIN, 0b00001000);
    //        SerialUSB.println("--> Backlight OFF");
    //        return;
    //      }
  } else {
    //SerialUSB.println((char*)msgBuf);
    showHelp();
  }
}

void showHelp() {
  char buff[256];
  SerialUSB.println(" +==================+================================+");
  sprintf(buff, " |%-18s|%32s|\n", "     Command", "Explanation            ");
  SerialUSB.print(buff);
  SerialUSB.println(" +==================+================================+");
  sprintf(buff, " |%-18s|%32s|\n", "/DN<max 32 chars>", "Set device name");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " -> right now", deviceName);
  SerialUSB.print(buff);
  SerialUSB.println(" +==================+================================+");
  sprintf(buff, " |%-18s|%32s|\n", "/>xxxxxxxxxxx", "send string xxxxxxxxxxx");
  SerialUSB.print(buff);
  SerialUSB.println(" +==================+================================+");
  SerialUSB.println(" |                      OPTIONS                      |");
  SerialUSB.println(" +==================+================================+");
  sprintf(buff, " |%-18s|%32s|\n", " /E1 or /e1", "turn on encryption");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /E0 or /e0", "turn off encryption");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "  -> right now", needEncryption ? "on" : "off");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /HX1", "turn on hexification");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /HX0", "turn off hexification");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "  -> right now", needHexification ? "on" : "off");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /PW<32 chars>", "set password [32 chars]");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /pw", "[exactly 32] (Uses AES256)");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /R1 or /r1", "turn on PONG back [Reply on]");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /R0 or /r0", "turn off PONG back [Reply off]");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "  -> right now", pongBack ? "on" : "off");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /FQ<float>", "Set a new LoRa frequency");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " Frequency:", "Between 862 and 1020 MHz (HF)");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%28.3f MHz|\n", "  -> right now", (myFreq / 1e6));
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /SF[7-12]", "Set a new LoRa Spreading Factor");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32u|\n", "  -> right now", mySF);
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /BW[0-9]", "Set a new LoRa Bandwidth");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "", "From 0: 7.8 KHz to 9: 500 KHz");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32.3f|\n", "  -> right now", BWs[myBW]);
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /CR[5-8]", "Set a new Coding Rate");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32u|\n", "  -> right now", myCR);
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /TX[7-17]", "Set a new Tx Power");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32u|\n", "  -> right now", TxPower);
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /OC1 or /oc1", "turn on OCP Trim");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /OC0 or /oc0", "turn off OCP Trim");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "  -> right now", OCP_ON ? "on" : "off");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /PB1 or /pb1", "turn on PA_BOOST");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", " /pb0 or /pb0", "turn off PA_BOOST");
  SerialUSB.print(buff);
  sprintf(buff, " |%-18s|%32s|\n", "  -> right now", PA_BOOST ? "on" : "off");
  SerialUSB.print(buff);
  SerialUSB.println(" +---------------------------------------------------+");
  sprintf(buff, " |%-18s|%32s|\n", " /P or /p", "Send PING packet");
  SerialUSB.print(buff);
  SerialUSB.println(" +==================+================================+");
  SerialUSB.println(" | Anything else    | show this help message.        |");
  SerialUSB.println(" +==================+================================+");
}
