# BastWAN_Minimal_LoRa

A project demonstrating how to do P2P encrypted communication using LoRa on the BastWAN.

It is using the LoRandom library by ours truly and the LoRa library by Sandeep Mistry. You could replace this library with whatever flavor you prefer, just pay attention to which pins are in use, and the functions used to read from and write to SPI. This is important because you need to provide 2 functions:

```c++
void writeRegister(uint8_t reg, uint8_t value) {
  LoRa.writeRegister(reg, value);
  // --> this is the function from LoRa.h
}
uint8_t readRegister(uint8_t reg) {
  return LoRa.readRegister(reg);
  // --> this is the function from LoRa.h
}
```

These 2 functions are used by `LoRandom.h`. Note that technically you shouldn't need `LoRandom.h`, because it has been integrated into LoRa.h – but I have kept working on it and it may have deviated a bit from the original. As it is now, I am including it, to stay on the safe side. Plus, if you want to use another LoRa-related library, YOU WILL NEED IT.

This example shows how to deal with random numbers. The code first builds a "stock" of 256 random `uint8_t`. Two functions are provided (although I only use one for now):

```c++
uint8_t getRandomByte() {
  uint8_t r = randomStock[randomIndex++];
  // reset random stock automatically if needed
  if (randomIndex > 252) stockUpRandom();
  return r;
}

void getRandomBytes(uint8_t *buff, uint8_t count) {
  uint8_t r;
  for (uint8_t i = 0; i < count; i++) {
    buff[i] = randomStock[randomIndex++];
    // reset random stock automatically if needed
    if (randomIndex > 252) stockUpRandom();
  }
}
```

The first one gives you one randome byte, as expected. The second stores `count` bytes into a buffer. The "stock" of random bytes is reset if it runs low. Here the threshold is set at 252, but realistically the test should / can be `if (randomIndex == 255) stockUpRandom();`.

The AES encryption / decryption code is the original [Rijndael implementation](http://efgh.com/software/rijndael.htm), and is provided in the sketch. There are better implementations, but it works, and is simple enough to read. Although, with crypto, simple is always debatable!

![Test](LoRaTest.png)

There are a few commands to be used in the Serial Monitor (or another Terminal):

![Help](Help.png)

There is an optional PONG back function – which I use to do distance tests. I have a BastWAN on a Keyboard FeatherWing, and I send regularly small messages. If the BastWAN at home receives the message, and the pongback option is on, it will send back a message with its device name and the RSSI. Good enough when you are doing a quick test.

Problems start to crop up when you have several devices listening and ponging back – then some PONGs may be lost, and even if not, only the last one will be shown, as the previous ones will be erased. I plan to remedy to this by creating a message queue, which will store the messages, and in the incoming window, will show a list from which you can choose which message to display. TODO!

The hex-encoded string below the pongback is the original pongback message, displayed for debug purposes. It'll go the way of the dodo soon.

![PongBack](PongBack.jpg)