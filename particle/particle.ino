#define AZMTH_SERVO TIM3->CCR2
#define PITCH_SERVO TIM3->CCR1
#define CLOCK_PRESCALER 4
#define MULTIPLIER      12

#include <SPI.h>
#include "nRF24L01.h"
#include "particle-rf24.h"
#include "Particle.h"
#include "math.h"

SYSTEM_MODE(MANUAL);

const int enable_pin  = D7; // (pin 1) pin to enable MEMS output
const int fclk_pin    = D0; // (pin 2) pin to set frequency of Bessel low pass filter

const int ss          = A2; // SS or CSN for MEMS
const int ss_radio    = D5; // SS or CSN for radio
const int sck         = A3; // (pin 6)
const int mosi        = A5; // (pin 4)
                            // Gnd (pin 3 and pin 5)
                            // +5V (pin 7)
const int ce          = D6; // to set TX or RX modes for radio

const int azimuth_pin = D2;
const int pitch_pin   = D3;
int cycle_count = 1;

RF24 radio(ce, ss_radio); // CE, CSN
byte addresses[][6] = {"1Node","2Node"};
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
#define NOISE_LPF_SIZE 4
#define POSITION_LPF_SIZE 4

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
double pitch_servo_pos   = 1000; // level
double azimuth_servo_pos = 1500; // midway point
Servo pitch_servo, azimuth_servo;

// Required constants
volatile double difX, difY, incX, incY, aX, aY, sum_NLPFX, sum_NLPFY, sum_PLPFX, sum_PLPFY;
const double inc = 0.8;
const double bias = 0;
int right = 0;
int up = 0;
const double initX = -5;
const double initY = 5;
double bufX[BUF_SIZE] = {};
double bufY[BUF_SIZE] = {};
double NLPFX[NOISE_LPF_SIZE] = {};
double NLPFY[NOISE_LPF_SIZE] = {};
double PLPFX[POSITION_LPF_SIZE] = {};
double PLPFY[POSITION_LPF_SIZE] = {};
int p = 0;
int q = 1;
int pNL = 0;
int qNL = 1;
int pPL = 0;
int qPL = 1;
int serial_counter = 1;
int servo_counter  = 1;

void azimuth_write(double pos);
void azimuth_controller_update(double mems_pos);
void pitch_write(double pos);
void pitch_controller_update(double mems_pos);

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
  radio.setPALevel(RF24_PA_MIN);
  radio.openReadingPipe(1, addresses[0]);
  //radio.setAutoAck(0,false);
  radio.startListening();

  servos_setup();
  //pitch_servo.attach(pitch_pin);
  //azimuth_servo.attach(azimuth_pin);
  //pitch_servo.writeMicroseconds(pitch_servo_pos);
  //azimuth_servo.writeMicroseconds(azimuth_servo_pos);
  //delay(2000); //wait for servos to move into position
}

void loop() {
  // Set analog voltage X+, X-, Y+, Y- from center (analog_bias) always at 80 V
  // Max voltage is 157 so max analog voltage is 157 - if you go beyond 157, the DAC will set to 80 V and your code won't work.

  difX = initX; //Can be positive or negative
  difY = initY;
  for (unsigned int k = 0 ; k < BUF_SIZE ; k++){
    bufX[k] = initX;
    bufY[k] = initY;
  }
  sum_NLPFX = 0;
  sum_NLPFY = 0;
  for (unsigned int k = 0 ; k < NOISE_LPF_SIZE ; k++){
    NLPFX[k] = 0;
    NLPFY[k] = 0;
  }
  sum_PLPFX = 0;
  sum_PLPFY = 0;
  for (unsigned int k = 0 ; k < POSITION_LPF_SIZE ; k++){
    PLPFX[k] = initX;
    PLPFY[k] = initY;
    sum_PLPFX += initX;
    sum_PLPFY += initY;
  }
  incX = 0;
  incY = 0;
  setChannels(difX, difY);

  while(1) {
    while(! radio.available()); //wait for new communication
    radio.read(&radio_data, sizeof(radio_data[0])*2);
    
    pNL++;
    if (pNL>=NOISE_LPF_SIZE) pNL=0;
    sum_NLPFX -= NLPFX[pNL];
    sum_NLPFY -= NLPFY[pNL];
    NLPFX[pNL] = (double)(radio_data[0])/127.0;
    NLPFY[pNL] = (double)(radio_data[1])/127.0;
    sum_NLPFX += NLPFX[pNL];
    sum_NLPFY += NLPFY[pNL];
    aX = sum_NLPFX / NOISE_LPF_SIZE;
    aY = sum_NLPFY / NOISE_LPF_SIZE;

    difX += aX * inc;
    difY += aY * inc;

    pPL++;
    if (pPL>=POSITION_LPF_SIZE) pPL=0;
    sum_PLPFX -= PLPFX[pPL];
    sum_PLPFY -= PLPFY[pPL];
    PLPFX[pPL] = difX;
    PLPFY[pPL] = difY;
    sum_PLPFX += PLPFX[pPL];
    sum_PLPFY += PLPFY[pPL];
    difX = sum_PLPFX / POSITION_LPF_SIZE;
    difY = sum_PLPFY / POSITION_LPF_SIZE;
    
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

    /*azimuth_controller_update(difY-initY);
    azimuth_write(azimuth_servo_pos);
    delayMicroseconds(100);
    pitch_controller_update(difX-initX);
    pitch_write(pitch_servo_pos);*/

    setChannels(difX, difY);

    Serial.print(aX);
    Serial.print(',');
    Serial.print(aY);
    Serial.print(',');
    Serial.print(difX);
    Serial.print(',');
    Serial.print(difY);
    Serial.print(',');
    Serial.println(servo_counter);
    serial_counter++;
    //servo_counter++;
    
    if (serial_counter > 50){
      //Serial.print(';');
      //Serial.print(azimuth_servo_pos);
      //Serial.print(',');
      //Serial.println(pitch_servo_pos);
      serial_counter = 0;
    }
    if (servo_counter > 400){
      //Serial.print(';');
      //Serial.print(azimuth_servo_pos);
      //Serial.print(',');
      //Serial.println(pitch_servo_pos);
      azimuth_increment();
      servo_counter = 0;
    }
    
  }
  powerdown(); //Always power down at the end to disable the MEMS output.. If you don't do this and disconnect the power you may break the MEMS
}

void azimuth_controller_update(double mems_pos) {
    static const double kp=0.01;
    double err_p;
    err_p = -mems_pos;
    azimuth_servo_pos += err_p * kp;
}

void pitch_controller_update(double mems_pos) {
    static const double kp=0.01;
    double err_p;
    err_p = -mems_pos;
    pitch_servo_pos += err_p * kp;
}

void azimuth_write(double pos){
  if (pos < 800 or pos > 2000) return;
  azimuth_servo.writeMicroseconds((int)(pos));
}
void pitch_write(double pos){
  if (pos < 800 or pos > 1500) return;
  //pitch_servo.writeMicroseconds((int)(pos));
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

void azimuth_increment()
{
  static int i = 1400*MULTIPLIER;
  static int increasing = true;
  if ( increasing )
    {
    i+=5*MULTIPLIER;
    if (i >= 1500*MULTIPLIER)
      {
      increasing = false;
      }
    }
  else
    {
    i-=5*MULTIPLIER;
    if (i <= 1400*MULTIPLIER)
      {
      increasing = true;
      }
    }
  AZMTH_SERVO = i;
}

void servos_setup()
{
  // Enable TIM 3 peripheral
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

  // 120Mhz master clock
  // We divide clock by 4
  
  TIM3->CR1   |= TIM_CR1_CEN; //Counter enable
  // TIM3->ARR    = 100; Set ARR to default max
  TIM3->CCR2   = 1400*MULTIPLIER ;  // Compare capture reg 1
  TIM3->CCR1   = 1000*MULTIPLIER ;  // Compare capture reg 2
  TIM3->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 ; // PWM mode Channel 1
  TIM3->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 ; // PWM mode Channel 2
  TIM3->CCER  |= TIM_CCER_CC1E | TIM_CCER_CC2E; //Capture Compare Enable channel 1 and 2
  TIM3->PSC    = CLOCK_PRESCALER;  // prescale clock by 4
  
  HAL_Pin_Mode(D2,AF_OUTPUT_PUSHPULL);
  HAL_Pin_Mode(D3,AF_OUTPUT_PUSHPULL);
  GPIO_PinAFConfig(PIN_MAP[D3].gpio_peripheral,PIN_MAP[D3].gpio_pin_source,GPIO_AF_TIM3);
  GPIO_PinAFConfig(PIN_MAP[D2].gpio_peripheral,PIN_MAP[D2].gpio_pin_source,GPIO_AF_TIM3);
  //s.attach(D3);
  //s.writeMicroseconds(1001);
  delay(1000);
}


