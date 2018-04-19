/*
 * Adapted from code by Dejan Nedelkovski, www.HowToMechatronics.com
 * Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
 */
#include "particle-rf24.h"
#include "math.h"
SYSTEM_MODE(MANUAL);

/* Phton Pin Mapping 
 *  
 * Multiplexer pins:
 * BIT_16_PIN --> D0
 * BIT_8_PIN  --> D1
 * BIT_4_PIN  --> D2
 * BIT_2_PIN  --> D3
 * BIT_1_PIN  --> D4
 * OUT_PIN    --> D5
 * 
 * pins for radio:
 * radio (NRF24L01) --> Particle Photon
 * MOSI --> A5
 * MISO --> A4
 * SCK  --> A3
 * SS   --> A2
 * CE   --> D6
 * GND  --> GND
 * VCC  --> 3V3
 * 
 */
#define BIT_16_PIN D0 //MSB
#define BIT_8_PIN  D1
#define BIT_4_PIN  D2
#define BIT_2_PIN  D3
#define BIT_1_PIN  D4  //LSB
#define OUT_PIN    D5
#define X_MAX      9.25
#define Y_MAX      6.25

 /* This is the diode number .
 * The mux index is offset by 5. For example diode #3 --> 8 on mux
 *  | | <-- 5
 * 20 21  22 23      ^ This way up (such that array is at bottom right of board)
 *  12 13  14  15
 * 4  5   6  7
 *   0  1   2  3     ^ y
 * 8  9  10 11       |
 *   16 17  18 19    --> x   origin is center of board, midpoint of 1 and 6
 */

/* dx and dy indexed by diode #, first three positions are not used
 * Diode # is shown above. 
 * dx and dy is distance from x and y axes in millimeters
 * origin is in center of photodiode array.
 */
const int ss          = A2; // SS or CSN for MEMS
const int sck         = A3; // (pin 6)
const int mosi        = A5; // (pin 4)
                            // Gnd (pin 3 and pin 5)
                            // +5V (pin 7)
const int ce          = D6; // to set TX or RX modes for radio


const double dx[] = {-6.75, -1.75, 4.25, 9.25,
                     -9.25, -4.25, 1.75, 6.75,
                     -9.25, -4.25, 1.75, 6.75,
                     -6.75, -1.75, 4.25, 9.25,
                     -6.75, -1.75, 4.25, 9.25,
                     -9.25, -4.25, 1.75, 6.75 };
const double dy[] = {-1.25, -1.25, -1.25, -1.25,
                     1.25, 1.25, 1.25, 1.25,
                     -3.75, -3.75, -3.75, -3.75,
                     3.75, 3.75, 3.75, 3.75,
                     -6.25, -6.25, -6.25, -6.25,
                     6.25, 6.25, 6.25, 6.25};
 
RF24 radio(ce, ss); // CE, CSN
byte addresses[][6] = {"1Node","2Node"};
int    threshold, number_diodes_on;
int8_t data[2] = {0,0};
double dy_mean, dx_mean, dy_sum, dx_sum;

void setup() {
  Serial.begin(9600);
  
  radio.begin();
  radio.setPALevel(RF24_PA_MIN);
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1,addresses[0]);
  //radio.setAutoAck(0,false);
  radio.stopListening();

  pinMode(BIT_16_PIN, OUTPUT);
  pinMode(BIT_8_PIN,  OUTPUT);
  pinMode(BIT_4_PIN,  OUTPUT);
  pinMode(BIT_2_PIN,  OUTPUT);
  pinMode(BIT_1_PIN,  OUTPUT);
  pinMode(OUT_PIN,    INPUT_PULLUP);
}

void loop() {
  dx_sum = 0;
  dy_sum = 0;
  number_diodes_on = 0;

  // iterate through mux
  for (int i=0 ; i < 32 ; i++){
    digitalWrite(BIT_1_PIN,  i & 1     );
    digitalWrite(BIT_2_PIN,  i & (1<<1));
    digitalWrite(BIT_4_PIN,  i & (1<<2));
    digitalWrite(BIT_8_PIN,  i & (1<<3));
    digitalWrite(BIT_16_PIN, i & (1<<4));
    if (digitalRead(OUT_PIN) == HIGH){
      dx_sum += dx[i];
      dy_sum += dy[i];
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
  //Serial.print(data[0]);
  //Serial.print(',');
  //Serial.println(data[1]);
  //delay(100);
  data[0] = round(dx_mean/X_MAX * 127);
  data[1] = round(dy_mean/Y_MAX * 127);
  radio.write(&data, sizeof(data[0])*2);
}
