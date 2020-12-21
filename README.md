# BastWAN_Minimal_LoRa

A project demonstrating how to do P2P encrypted communication using LoRa on the BastWAN.

It is using the LoRa library by Sandeep Mistry. You could replace this library with whatever flavor ypu prefer, just pay attention to which pins are in use.

The AES encryption / decryption is the original [Rijndael implementation](http://efgh.com/software/rijndael.htm), and is provided in the sketch. 

![Test](LoRaTest.png)

There are a few commands to be used in the Serial Monitor (or another Terminal):

![Help](Help.png)