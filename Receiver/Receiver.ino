/*
* Arduino Wireless Communication Tutorial
*       Example 1 - Receiver Code
*                
* by Dejan Nedelkovski, www.HowToMechatronics.com
* 
* Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
*/
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
RF24 radio(7, 8); // CE, CSN
const byte address[][6] = {"00001","00002"};
//char text[64] = "";
unsigned long starttime;

void setup() {
  Serial.begin(115200);
  radio.begin();
  //radio.openWritingPipe(address[1]);    //00002
  radio.openReadingPipe(0, address[0]); //00001
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(0,false);
//  radio.enableAckPayload();
//  radio.setRetries(0,0);
  radio.startListening();
}
void loop() {
  if (radio.available()) {
    //char text[64] = "";
    int text;
    radio.read(&text, sizeof(text));
    Serial.println(text);
//    unsigned long endtime = micros();
//    unsigned long totaltime = endtime - starttime;
//    serial.println(totaltime);
//    starttime = micros();
  }
}
