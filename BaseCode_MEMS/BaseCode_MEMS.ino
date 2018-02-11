// ------------
// Setup for Mirrorcle PicoAmp 4.6
// ------------

/* Installation instructions for particle cli:
 * Following command will install Homebrew (if not already installed), and use Homebrew
 * to install dfu-utils, openssl, and particle:
 *   $ bash <( curl -sL https://particle.io/install-cli )
 * Add particle to PATH:
 *   $ export PATH=$PATH:/Users/<replace-with-username>/bin
 * 
 * To compile, run:
 *   $ particle compile photon BaseCode_MEMS.ino --saveTo out.bin
 *   
 * To flash, FIRST PUT PHOTON IN DFU STATE.
 *   You do this by holding down both buttons at same time, then release reset button only.
 *   The light will first blink magenta then blink yellow. You can release the setup button
 *   when the light blinks yellow.
 * Then run:
 *   particle flash --usb /Users/christopherliao/Desktop/BaseCode_MEMS/out.bin
 * Wait for 10-15 seconds.
 * 
 * If you set SYSTEM_MODE(SEMI_AUTOMATIC), the light should breath white. If the light blinks 
 * blue, something is wrong.
 * If the MEMS mirror is enabled, blue light on D7 should be blue.
 * 
 * DEVELOPMENT NOTES:
 *   o Use spherical_to_tilt to convert from sphrical coordinates to x and y tilt in radians.
 *   o Use rad_to_volts to convert the tilt in radians to votlages
 *   o Use setChannels to set the voltages of the MEMS mirror.
 *   o rad_to_volts is not yet done, it currently assumes linear relationship
 *     between radians and voltage, which it's not.
 *     
 * COORDIANTE SYSTEM NOTES:
 *   o tilt in x direction is right-hand rule around x-axis and tilt in y direction is right
 *     hand rult around y axis.
 *   o z axis points out of mirror.
 *   o coordinates are defined in spherical coordinates without radius, phi is azimuth
 *     from x axis and theta is inclination from z axis. Use ISO convention
 *     https://en.wikipedia.org/wiki/Spherical_coordinate_system
 */
#include "math.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

int enable_pin  = D7; // (pin 1) pin to enable MEMS output
int fclk_pin    = D0; // (pin 2) pin to set frequency of Bessel low pass filter

int ss          = A2; // (pin 8)
int sck         = A3; // (pin 6)
int mosi        = A5; // (pin 4)
                      // Gnd (pin 3 and pin 5)
                      // +5V (pin 7)

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

volatile bool left = true;

typedef struct {
  double x;     // tilt in radians around x axis (-pi/2,pi/2)
  double y;     // tily in radians around y axis (-pi/2.pi/2)
} tilt;

typedef struct {
  double phi;   // azimuth from x axis in rad [0,2*pi]
  double theta; // inclination from z axis in rad [0,pi]
} spherical;

// store incident laser vector in spherical coordinates here.
spherical incident = {pi/2, pi/4};

void setup() {
  pinMode(enable_pin, OUTPUT);
  pinMode(fclk_pin, OUTPUT);
  pinMode(ss, OUTPUT);
  pinMode(sck, OUTPUT);
  pinMode(mosi, OUTPUT);

  SPI.begin();
  SPI.begin(SPI_MODE_MASTER); // SPI master
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 60 MHz over 4 -> 15 MHz
  SPI.setBitOrder(MSBFIRST); // Bit 23 is first in buffer
  SPI.setDataMode(SPI_MODE1);  // According to github library

  analogWrite(fclk_pin, 127, fclk_freq); // Begin Bessel filter clock set at 50% duty cycle

  digitalWrite(enable_pin,LOW);
  initialization();
}

// Required constants
int numloops        = 0;
int maxloops        = 10;
double difX, difY;

void circle(double r);
void square(double r);
void liney(double r);
void linex(double r);
void linexy(double r);
void eclipse(double theta);
// phi is azimuth measured in rad counter clockwise around z axis deom +x axis.
// theta is inclination measured from +z axis.
tilt spherical_to_tilt(double theta, double phi);
double rad_to_volts(double rad);

void loop() {
  // Set analog voltage X+, X-, Y+, Y- from center (analog_bias) always at 80 V
  // Max voltage is 157 so max analog voltage is 157 - if you go beyond 157, the DAC will set to 80 V and your code won't work.

  difX = 0; //Can be positive or negative
  difY = 0;

  while(1){
    if (left){
      difX --;
      if (difX < -50) left = !left;
      setChannels(difX, 0);
    }
    else{
      difX ++;
      if (difX > 50) left = !left;
      setChannels(difX, 0);
    }
    delay(10);
  }
  
  powerdown(); //Always power down at the end to disable the MEMS output.. If you don't do this and disconnect the power you may break the MEMS
}

void square(double r){
  setChannels(r,r);
  delay(1);
  setChannels(-r,r);
  delay(1);
  setChannels(-r,-r);
  delay(1);
  setChannels(r,-r);
  delay(1);
}

void circle(double r){
  for (double theta = 0; theta < 2*pi ; theta += 1){
    setChannels(r*cos(theta),r*sin(theta));
    delay(1);
  }
}

void liney(double r){
  for (double i = -r ; i < r ; i += 1){
    setChannels(0,i);
    delayMicroseconds(10);
  }
}

void linexy(double r){
  for (double i = -r ; i < r ; i += 1){
    setChannels(i,i);
    delayMicroseconds(10);
  }
}

void linex(double r){
  for (double i = -r ; i < r ; i += 1){
    setChannels(i,0);
    delayMicroseconds(10);
  }
}

void eclipse(double theta){
  //theta is inclination in rad
  tilt t;
  for(double phi ; phi < 2*pi ; phi += 0.1){
    t = spherical_to_tilt(theta,phi);
    setChannels(rad_to_volts(t.x),rad_to_volts(t.y));
  }
}

double rad_to_volts(double rad){
  return rad/2.0/pi*360.0/6*50;
}

tilt spherical_to_tilt(double theta, double phi){
  tilt return_tilt;
  return_tilt.x = atan2(-sin(incident.theta)*sin(incident.phi) - sin(theta)*sin(phi),
                        cos(incident.theta) + cos(theta));
  return_tilt.y = atan2(sin(incident.theta)*cos(incident.phi) - sin(theta)*cos(phi),
                        cos(incident.theta) + cos(theta));
  return return_tilt;
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
