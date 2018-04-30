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
const int mosi        = A5; // (pin 4)
                            // Gnd (pin 3 and pin 5)
                            // +5V (pin 7)
const int ce          = D6; // to set TX or RX modes for radio

const int mon_pin     = WKP; // WKP is monitor pin for Lidar

const int azimuth_pin = TX;
const int pitch_pin   = RX;

RF24 radio(ce, ss_radio); // CE, CSN
const byte address[][6] = {"00001","00002"};
int8_t radio_data[2] = {0,0};

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

#define pi 3.1415926
#define BUF_SIZE 100

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
int pitch_servo_pos   = 1800; // level
int azimuth_servo_pos = 1500; // midway point
int azimuth_ctl_i_acc = 0;
int azimuth_ctl_d_last = 0;
Servo pitch_servo, azimuth_servo;

// Required constants
volatile double difX, difY, incX, incY;
const double inc = 0.2;
int right = 0;
int up = 0;
double bufX[BUF_SIZE] = {};
double bufY[BUF_SIZE] = {};
int p = 0;
int q = 1;
int serial_counter = 0;
const double ki=0, kp=1, kd=0;

double pulseWidth=0.0; // radial distance
double az=0.0; // azimuth angle of laser
double el=0.0; // elevation angle of laser
double h=0.0; // sqrt(x^2+y^2)
double x=0.0;
double y=0.0;
double z=0.0;

void azimuth_write(int pos);
int azimuth_controller_update(double mems_pos);
void pitch_write(int pos);

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
  Serial.begin();
  
  radio.begin();
  radio.openReadingPipe(0, address[0]);
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(0,false);
  radio.startListening();

  //pitch_servo.attach(pitch_pin);
  azimuth_servo.attach(azimuth_pin);
  //pitch_servo.writeMicroseconds(pitch_servo_pos);
  azimuth_servo.writeMicroseconds(azimuth_servo_pos);
  delay(3000); //wait for servos to move into position
  
}

void loop() {
  // Set analog voltage X+, X-, Y+, Y- from center (analog_bias) always at 80 V
  // Max voltage is 157 so max analog voltage is 157 - if you go beyond 157, the DAC will set to 80 V and your code won't work.

  difX = 0; //Can be positive or negative
  difY = 0;
  incX = 0;
  incY = 0;
  setChannels(difX, difY);

  while(1) {
    while(! radio.available()); //wait for new communication
    radio.read(&radio_data, sizeof(radio_data[0])*2);
    p++;
    bufX[p] = difX;
    bufY[p] = difY;
    q = p+1;
    if (p>=BUF_SIZE) p=0;
    if (q>=BUF_SIZE) q=0;
    
    pulseWidth = pulseIn(WKP, HIGH); // Count how long the pulse is high in microseconds
    pulseWidth = pulseWidth/1000; // Get radial distance in meters

    incX = (bufX[p]-bufX[q])/BUF_SIZE;
    incY = (bufY[p]-bufY[q])/BUF_SIZE;
    difX += incX;
    difY += incY;
    difX += (double)(radio_data[0])/127.0 * inc;
    difY += (double)(radio_data[1])/127.0 * inc;

    az=difX*(2.0/pi*360.0/6*50); // volts to radians
    el=difY*(2.0/pi*360.0/6*50); // volts to radians
    h=pulseWidth*cos(el);
    z=pulseWidth*sin(el);
    //incX=atan(radio_data[0]/radio_data[1])/2.0/pi*360.0/6*50;
    //incY=atan(z/sqrt(radio_data[0]^2+radio_data[1]^2))/(2.0/pi*360.0/6*50); //radians to volts
    
    azimuth_servo_pos = azimuth_controller_update(difX);
    azimuth_write(azimuth_servo_pos);
    /*
    if (difX > 10){
      azimuth_servo_pos ++;
      azimuth_write(azimuth_servo_pos);
    }
    else if (difX < -10){
      azimuth_servo_pos --;
      azimuth_write(azimuth_servo_pos);
    }
    */
    
    setChannels(difX, difY);

    //Serial.print(radio_data[0]);
    //Serial.print(',');
    //Serial.println(radio_data[1]);
    
    serial_counter++;
    if (serial_counter > 50){
      Serial.print(difX);
      Serial.print(',');
      Serial.print(difY);
      Serial.print(';');
      Serial.println(azimuth_servo_pos);
      serial_counter = 0;
    }
  }
  powerdown(); //Always power down at the end to disable the MEMS output.. If you don't do this and disconnect the power you may break the MEMS
}

int azimuth_controller_update(double mems_pos) {
    const double ki = 0.0, kp=0.1, kd=0.0;
    static double err_i = 0;
    static double mems_last = 0;
    double err_p, err_d;

    err_i += mems_pos;
    err_p = mems_pos;
    err_d = mems_pos - mems_last;

    mems_last = mems_pos;

    return (int)(1500 + (err_i * ki) + (err_p * kp) + (err_d * kd));
}

void azimuth_write(int pos){
  if (pos < 1200 or pos > 1800) return;
  azimuth_servo.writeMicroseconds(pos);
}
void pitch_write(int pos){
  if (pos < 1600 or pos > 2000) return;
  pitch_servo.writeMicroseconds(pos);
}

// All functions for this to work
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
