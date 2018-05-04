SYSTEM_MODE(MANUAL);
#include <SPI.h>
#include "nRF24L01.h"
#include "particle-rf24.h"
#include "math.h"


const int enable_pin  = D7; // (pin 1) pin to enable MEMS output
const int fclk_pin    = D0; // (pin 2) pin to set frequency of Bessel low pass filter
const int ss          = A2; // SS or CSN for MEMS
const int ss_radio    = D5; // SS or CSN for radio
const int sck         = A3; // (pin 6)
const int mosi         = A5; // (pin 4)
                            // Gnd (pin 3 and pin 5)
                            // +5V (pin 7)
const int ce          = D6; // to set TX or RX modes for radio
const int azimuth_pin = D2;
const int pitch_pin   = D3;

/* Radio setup */
RF24 radio(ce, ss_radio); // CE, CSN
byte addresses[][6] = {"1Node","2Node"};
int8_t radio_data[2] = {0,0};

/* MEMS Setup */
int full_reset                  = 2621441; //Initialization to set up DAC
int enable_internal_reference   = 3670017; //Initialization to set up DAC
int enable_all_dac_channels     = 2097167; // B00100000 00000000 00001111//Initialization to set up DAC
int enable_software_ldac        = 3145728; // B00110000 00000000 00000000 //Initialization to set up DAC
int disable_all_dac_channels    = 2097215; //Power Down

#define AD56X4_COMMAND_WRITE_INPUT_REGISTER             0 //B00000000 //Set the channel(s)'s input register.
#define AD56X4_COMMAND_UPDATE_DAC_REGISTER              8 //B00001000 //Update the DAC register (output the set voltage)
#define AD56X4_COMMAND_WRITE_INPUT_REGISTER_UPDATE_ALL  16 //B00010000 //Set channel/s's input register and then update all DAC registers from the input registers.

#define AD56X4_CHANNEL_A                               0 //B00000000
#define AD56X4_CHANNEL_B                               1 //B00000001
#define AD56X4_CHANNEL_C                               2 //B00000010
#define AD56X4_CHANNEL_D                               3 //B00000011
#define AD56X4_CHANNEL_ALL                             7 //B00000111
// commands to write to LDAC buffer, then at channel_D write to DAC register
int writecommand[]  = {AD56X4_COMMAND_WRITE_INPUT_REGISTER  | AD56X4_CHANNEL_A,
                       AD56X4_COMMAND_WRITE_INPUT_REGISTER  | AD56X4_CHANNEL_B,
                       AD56X4_COMMAND_WRITE_INPUT_REGISTER  | AD56X4_CHANNEL_C,
                       AD56X4_COMMAND_WRITE_INPUT_REGISTER_UPDATE_ALL  | AD56X4_CHANNEL_D
                     };
// Set mirror dependent values
int freq            = 650; // 650 Hz recommended cutoff frequency
int fclk_freq       = 60*freq; //39000;    // Recommended 650 Hz * 60 - do not change!!!!!!
double max_voltage  = 157;      // Datasheet spec - do not change!!!!!!
int bias_voltage    = 80;       // Datasheet spec - do not change!!!!!!
double analog_bias  = (bias_voltage*65535)/160 + 0.5;
int dig_bias        = (int) analog_bias;
int data;

/* Change to modify velocity filter buffer size */
#define BUF_SIZE 100

/* Servo related parameters */
double pitch_servo_pos   = 1100; // level
double azimuth_servo_pos = 1500; // midway point
Servo pitch_servo, azimuth_servo;

/* Control related varaibles and parameters */
volatile double difX, difY;  // Current position of MEMS is volts
volatile double incX, incY;  // Amount to move MEMS by in volts.
const double inc = 0.1;      // Tunable parameter, higher --> Respond faster to input but risk 
                             // oscillatory behavior
double bufX[BUF_SIZE] = {};  // circular velocity filter X
double bufY[BUF_SIZE] = {};  // circular velocity filter Y
int p = 0;                   // pointer to circular buffer 
int q = 1;                   // pointer to next circular buffer value
int serial_counter = 0;      // Delay counter to output debug info once in a while

// Tunable parameters: Need to offset rest position, because unmovable laser beam at 0,0
const double initX = -15;
const double initY = 15;

// Tune for Servo response
const double servo_inc = 0.01;

void signal_setup();

void setup() {
  pinMode(enable_pin, OUTPUT);
  pinMode(fclk_pin, OUTPUT);
  pinMode(ss, OUTPUT);
  pinMode(sck, OUTPUT);
  pinMode(mosi, OUTPUT);

  /* Power down pin */
  pinMode(A0,INPUT_PULLUP);
  attachInterrupt(A0, powerdown, FALLING);

  SPI.begin();
  SPI.begin(SPI_MODE_MASTER); // SPI master
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 60 MHz over 4 -> 15 MHz
  SPI.setBitOrder(MSBFIRST); // Bit 23 is first in buffer
  SPI.setDataMode(SPI_MODE1);  // According to github library

  analogWrite(fclk_pin, 127, fclk_freq); // Begin Bessel filter clock set at 50% duty cycle
  
  digitalWrite(enable_pin,LOW);
  initialization();
  Serial.begin(9600);
  
  radio.begin();
  radio.openReadingPipe(0, addresses[0]);
  radio.enableDynamicPayloads();
  radio.setPALevel(RF24_PA_MAX);          // Transmit at maximum power
  radio.setAutoAck(0,false);              // Disable AutoAck
  radio.startListening();

  azimuth_servo.attach(azimuth_pin);
  pitch_servo.attach(pitch_pin);
  azimuth_servo.writeMicroseconds((int)azimuth_servo_pos);
  pitch_servo.writeMicroseconds((int)pitch_servo_pos);

  signal_setup();
}

void loop() {
  // Set analog voltage X+, X-, Y+, Y- from center (analog_bias) always at 80 V
  // Max voltage is 157 so max analog voltage is 157 - if you go beyond 157, the DAC will set to 80 V and your code won't work.

  /* Coarse Tracking Stage */
  /*String inString;
  while(1){
    while (Serial.available() > 0){
      int inChar = Serial.read();
        if (isDigit(inChar)) {
          // convert the incoming byte to a char and add it to the string:
          inString += (char)inChar;
        }
        else if (inChar == ',') {
          azimuth_servo_pos = inString.toInt();
          if (azimuth_servo_pos < 2000 and azimuth_servo_pos > 1000) 
            azimuth_servo.writeMicroseconds(azimuth_servo_pos);
          Serial.print("moving azimuth to ");
          Serial.println(azimuth_servo_pos);
          inString = "";
        }
        else if (inChar == '\n') {
          pitch_servo_pos = inString.toInt();
          if (pitch_servo_pos < 1500 and pitch_servo_pos > 800)
            pitch_servo.writeMicroseconds(pitch_servo_pos);
          Serial.print("moving pitch to ");
          Serial.println(pitch_servo_pos);
          inString = "";
        }
        else if (inChar == 'c'){
          break;
        }
    }
  }*/

  /* Scanning Stage:
   * Start at center and move out in circles or increasing radius
   * set max radius to be 60 volts.
   */
  /*double radius = 0.5;
  double angle = 0;
  while(1){
    if (angle > 2*3.1415926){
      radius += 0.5;
      angle = 0;
    }
    if (difX > 60 or difY > 60){
      difX = 0;
      difY = 0;
    }
    difX = radius * cos(angle);
    difY = radius * sin(angle);
    angle += 2/radius;
    delay(1);
    while(radio.available()) radio.read(&radio_data, sizeof(radio_data[0])*2);
    if (radio_data[0] != 0 or radio_data[1] != 0) break;
    setChannels(difX, difY);
  }
  difX = radius * cos(angle - 2/radius * 5);
  difY = radius * sin(angle - 2/radius * 5);*/

  /* Control system initialization. Start laser beam at offset. */
  incX = 0;
  incY = 0;
  difX = initX;
  difY = initY;
  for (unsigned int k = 0 ; k < BUF_SIZE ; k++){
    bufX[k] = initX;
    bufY[k] = initY;
    //bufX[k] = difX;
    //bufY[k] = difY;
  }
  setChannels(difX, difY);

  /* Main Control Loop 
   * IMPORTANT NOTE: 
   * If you add code to make this loop longer, you must test to make sure 
   * the loop in transmitter is still longer the control loop.
   * Otherwise, the control loop will fall behind in reading transmissions,
   * the radio buffer will go bazerk and things won't work.
   */
  while(1) {
    unsigned long a = micros();
    while(! radio.available()); //wait for new communication
    unsigned long b = micros();
    Serial.println(b-a);
    radio.read(&radio_data, sizeof(radio_data[0])*2);
    //if (radio_data[0] == 0 and radio_data[0] == 0 ) continue;

    /* incX and incY are the velocity vector of the target
     * based on an average of past 100 observed positions.
     * Measure in volts per time interval.
     * Time interval is amount of time it takes for one while loop
     */
    p++;
    if (p>=BUF_SIZE) p=0;
    q = p+1;
    if (q>=BUF_SIZE) q=0;
    bufX[p] = difX;
    bufY[p] = difY;
    incX = (bufX[p]-bufX[q])/BUF_SIZE;
    incY = (bufY[p]-bufY[q])/BUF_SIZE;
    difX += incX;
    difY += incY;
    difX += (double)(radio_data[0])/127.0 * inc;
    difY += (double)(radio_data[1])/127.0 * inc;
    setChannels(difX, difY);

    /* Simple servo control algo */
    azimuth_servo_pos += (difY-initY) * servo_inc;
    if (azimuth_servo_pos > 2200 or azimuth_servo_pos < 800) break;
    azimuth_servo.writeMicroseconds((int)azimuth_servo_pos);
    pitch_servo_pos -= (difX-initX) * servo_inc;
    if (pitch_servo_pos > 1500 or pitch_servo_pos < 800) break;
    pitch_servo.writeMicroseconds((int)pitch_servo_pos);

    /*Serial.print(difX);
    Serial.print(',');
    Serial.print(difY);
    Serial.print(',');
    Serial.print(azimuth_servo_pos);
    Serial.print(',');
    Serial.println(pitch_servo_pos);*/
    /*serial_counter++;
    if (serial_counter > 50){
      Serial.print(difX);
      Serial.print(',');
      Serial.println(difY);
      serial_counter = 0;
    }*/
  }
  powerdown(); //Always power down at the end to disable the MEMS output.. If you don't do this and disconnect the power you may break the MEMS
}

/* MEMS Functions below */
void initialization() {         //Initialization to set up DAC

    digitalWrite(ss,LOW);
    SPI.transfer((full_reset>>16) & 0xFF);  // bits 16-23
    SPI.transfer((full_reset>>8) & 0xFF);   // bits 8-15
    SPI.transfer(full_reset & 0xFF);        // bits 0-7
    digitalWrite(ss,HIGH);

    digitalWrite(ss,LOW);
    SPI.transfer((enable_internal_reference>>16) & 0xFF);
    SPI.transfer((enable_internal_reference>>8) & 0xFF);
    SPI.transfer(enable_internal_reference & 0xFF);
    digitalWrite(ss,HIGH);

    digitalWrite(ss,LOW);
    SPI.transfer((enable_all_dac_channels>>16) & 0xFF);
    SPI.transfer((enable_all_dac_channels>>8) & 0xFF);
    SPI.transfer(enable_all_dac_channels & 0xFF);
    digitalWrite(ss,HIGH);

    digitalWrite(ss,LOW);
    SPI.transfer((enable_software_ldac>>16) & 0xFF);
    SPI.transfer((enable_software_ldac>>8) & 0xFF);
    SPI.transfer(enable_software_ldac & 0xFF);
    digitalWrite(ss,HIGH);

    digitalWrite(enable_pin,HIGH); // Enable MEMS Output
    setChannels(0,0); // Set MEMS to bias offset of 80 V
}

void setChannels(double difX, double difY) {

  //check that the maximum voltage does not exceed 157 V

  double Xp = bias_voltage + difX;
  double Xm = bias_voltage - difX;
  double Yp = bias_voltage + difY;
  double Ym = bias_voltage - difY;

  if (Xp > max_voltage | Xm > max_voltage | Yp > max_voltage | Ym > max_voltage) {
    difX = 0; // If it exceeds 157 V, set the output to the bias voltage.
    difY = 0; // If it exceeds 157 V, set the output to the bias voltage.
  }

  //convert difX and difY to digital values
  double voltage[] = {Xp, Xm, Yp, Ym};

  digitalWrite(ss,HIGH); //pull out of idle

  for (int i = 0; i < 4; i++) {
    double analog_voltage = (voltage[i]*65535)/160 + 0.5; // Convert from voltage to digital value
    data = (int) analog_voltage;

    digitalWrite(ss,LOW);
    SPI.transfer(writecommand[i]); //Defined above to write to LDAC and write to DAC register on last channel
    SPI.transfer((data >> 8) & 0xFF);
    SPI.transfer(data & 0xFF);
    digitalWrite(ss,HIGH);
  }

  // DAC update all outputs simultaneously
  digitalWrite(ss,LOW);
  SPI.transfer(AD56X4_COMMAND_UPDATE_DAC_REGISTER | AD56X4_CHANNEL_ALL);
  SPI.transfer(0);
  SPI.transfer(0);
  digitalWrite(ss,HIGH);

  digitalWrite(ss,LOW); //idle for low current mode without powering down
}

void powerdown() {

  digitalWrite(enable_pin,LOW); //Disable MEMS output

  digitalWrite(ss,HIGH); //pull out of idle
  digitalWrite(ss,LOW);
  SPI.transfer((full_reset>>16) & 0xFF);  // bits 16-23
  SPI.transfer((full_reset>>8) & 0xFF);   // bits 8-15
  SPI.transfer(full_reset & 0xFF);        // bits 0-7
  digitalWrite(ss,HIGH);

  //Power down to idle
  digitalWrite(ss,LOW);
  SPI.transfer((disable_all_dac_channels>>16) & 0xFF);
  SPI.transfer((disable_all_dac_channels>>8) & 0xFF);
  SPI.transfer(disable_all_dac_channels & 0xFF);
  digitalWrite(ss,HIGH);
  //delay(1);
  digitalWrite(ss,LOW);
}

void signal_setup()
{
  pinMode(D1, OUTPUT);
  analogWrite(D1, 256/2, 10000);
}




