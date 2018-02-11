///*
//* Arduino Wireless Communication Tutorial
//*     Example 1 - Transmitter Code
//*                
//* by Dejan Nedelkovski, www.HowToMechatronics.com
//* 
//* Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
//*/
//#include <SPI.h>
//#include "nRF24L01.h"
//#include "RF24.h"
//RF24 radio(7, 8); // CE, CSN
//const byte address[][6] = {"00001","00002"};
////const char text[];
//
//  
//void setup() {
//  Serial.begin(9600);
//  radio.begin();
//  radio.openWritingPipe(address[0]);    //00001
//  //radio.openReadingPipe(1,address[1]);  //00002
//  radio.setPALevel(RF24_PA_MIN);
//  radio.setAutoAck(0,false);
////  radio.enableAckPayload();
////  radio.setRetries(0,0);
//  radio.stopListening();
//}
//
//
//void loop() {
//  int i = 0;
//  unsigned long StartTime = micros();
//  while(i < 100){
//    radio.write(&i, sizeof(i));
//    i++;
//  }
//  unsigned long EndTime = micros(); //micros has an overhead of about 8-12 microseconds
//  unsigned long TotalTime = EndTime - StartTime;
//  Serial.println(TotalTime);
//  delay(100);
//}

/*
* Arduino Wireless Communication Tutorial
*     Example 1 - Transmitter Code
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
//const char text[];
int threshold;
char data;

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.openWritingPipe(address[0]);    //00001
  //radio.openReadingPipe(1,address[1]);  //00002
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(0,false);
//  radio.enableAckPayload();
//  radio.setRetries(0,0);
  radio.stopListening();

  pinMode(A0,INPUT_PULLUP);
  pinMode(A1,INPUT_PULLUP);
  pinMode(A2,INPUT_PULLUP);
  pinMode(A3,INPUT_PULLUP);
  pinMode(A4,INPUT_PULLUP);
  pinMode(A5,INPUT_PULLUP);
  threshold = analogRead(A1) - 20;
}

void loop() {
  data = '\0';
  data |= (analogRead(A1) < threshold); //0 -Target
  data |= (analogRead(A2) < threshold) << 1; //10 - DOWN (our left)
  data |= (analogRead(A3) < threshold) << 2; //100 - UP (our right)
  data |= (analogRead(A4) < threshold) << 3; //1000 - RIGHT (our down)
  data |= (analogRead(A5) < threshold) << 4; //10000 - LEFT(our up)
  radio.write(&data, sizeof(data));
  Serial.println(data,BIN);
}
