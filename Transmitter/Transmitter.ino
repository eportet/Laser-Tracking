/*
 * Adapted from code by Dejan Nedelkovski, www.HowToMechatronics.com
 * Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
 */
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

#define BIT_16_PIN 13 //MSB
#define BIT_8_PIN  12
#define BIT_4_PIN  11
#define BIT_2_PIN  10
#define BIT_1_PIN  9  //LSB
#define OUT_PIN    6
#define X_MAX      9.25
#define Y_MAX      6.25

/* This is the diode number .
 * The mux index is offset by 5. For example diode #3 --> 8 on mux
 *  | | <-- 5
 * 4  3   6  5      ^ This way up (such that array is at bottom right of board)
 *   8  7  10  9
 * 12 11  14 13
 *   16 15  18 17 
 * 20 19  22 21
 *   24 23  26 25
 */

/* dx and dy indexed by diode #, first three positions are not used
 * Diode # is shown above. 
 * dx and dy is distance from x and y axes in millimeters
 * origin is in center of photodiode array.
 */
const double dx[] = {0, 0, 0, -4.25, -9.25, 6.75, 1.75, 
                             -1.75, -6.75, 9.25, 4.25, 
                             -4.25, -9.25, 6.75, 1.75, 
                             -1.75, -6.75, 9.25, 4.25, 
                             -4.25, -9.25, 6.75, 1.75, 
                             -1.75, -6.75, 9.25, 4.25};
const double dy[] = {0, 0, 0, 6.25, 6.25, 6.25, 6.25,
                             3.75, 3.75, 3.75, 3.75,
                             1.25, 1.25, 1.25, 1.25,
                             -1.25, -1.25, -1.25, -1.25, 
                             -3.75, -3.75, -3.75, -3.75, 
                             -6.25, -6.25, -6.25, -6.25};
 
RF24 radio(7, 8); // CE, CSN
const byte address[][6] = {"00001","00002"};
int    threshold, number_diodes_on;
int8_t data[2] = {0,0};
double dy_mean, dx_mean, dy_sum, dx_sum;

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.openWritingPipe(address[0]); 
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(0,false);
  radio.stopListening();

  pinMode(BIT_16_PIN, OUTPUT);
  pinMode(BIT_8_PIN,  OUTPUT);
  pinMode(BIT_4_PIN,  OUTPUT);
  pinMode(BIT_2_PIN,  OUTPUT);
  pinMode(BIT_1_PIN,  OUTPUT);
  pinMode(OUT_PIN,    INPUT_PULLUP);

  pinMode(A0,INPUT_PULLUP);
  pinMode(A1,INPUT_PULLUP);
  pinMode(A2,INPUT_PULLUP);
  pinMode(A3,INPUT_PULLUP);
  pinMode(A4,INPUT_PULLUP);
  pinMode(A5,INPUT_PULLUP);
  threshold = analogRead(A1) - 20;
}

void loop() {
  dx_sum = 0;
  dy_sum = 0;
  number_diodes_on = 0;

  // iterate through mux
  for (int i=8 ; i < 32 ; i++){
    digitalWrite(BIT_1_PIN,  i & 1     );
    digitalWrite(BIT_2_PIN,  i & (1<<1));
    digitalWrite(BIT_4_PIN,  i & (1<<2));
    digitalWrite(BIT_8_PIN,  i & (1<<3));
    digitalWrite(BIT_16_PIN, i & (1<<4));
    if (digitalRead(OUT_PIN) == HIGH){
      dx_sum += dx[i-5];
      dy_sum += dy[i-5];
      number_diodes_on ++;
    }
  }
  if (number_diodes_on == 0){
    dx_mean = 0;
    dy_mean = 0;
  }
  else{
    dx_mean = dx_sum / number_diodes_on;
    dy_mean = dy_sum / number_diodes_on;
  }
  data[0] = round(dx_mean/X_MAX * 127);
  data[1] = round(dy_mean/Y_MAX * 127);
  radio.write(&data, sizeof(data[0])*2);
}
