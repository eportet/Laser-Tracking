/*
 * Adapted from code by Dejan Nedelkovski, www.HowToMechatronics.com
 * Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
 */
#include <SPI.h>
#include "nRF24L01.h"
#include "particle-rf24.h"
#include "Particle.h"
#include "math.h"
SYSTEM_MODE(MANUAL);

/* Phton Pin Mapping 
 *  
 *  IMPORTANT:
 *  *** Connect A0 to D0 to trigger reset of mux index at falling edge of MSB
 *  *** Connect WKP to D4 to trigger read of max out pin at value change of LSB
 *  
 * Multiplexer pins:
 * BIT_16_PIN --> D0 --> A0 --> TIM4_CH2          (MSB)
 * BIT_8_PIN  --> D1 --> TIM4_CH1
 * BIT_4_PIN  --> D2 --> TIM3_CH2
 * BIT_2_PIN  --> D3 --> TIM3_CH1
 * BIT_1_PIN  --> D4 --> WKP --> TIM5_CH1  (LSB)
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
 */

#define BIT_16_PIN D0 //MSB
#define BIT_8_PIN  D1
#define BIT_4_PIN  D2
#define BIT_2_PIN  D3
#define BIT_1_PIN  D4  //LSB
#define OUT_PIN    D5
#define X_MAX      9.25
#define Y_MAX      6.25
#define SIGNAL_PIN D7
#define RESET_PIN  A0

const int ss          = A2; // SS or CSN for MEMS
const int sck         = A3; // (pin 6)
const int mosi        = A5; // (pin 4)
const int ce          = D6; // to set TX or RX modes for radio

/* This is the diode number .
 * The mux index is offset by 5. For example diode #3 --> 8 on mux
 *  | | <-- 5
 * 29  30  27  28      ^ This way up (such that array is at bottom right of board)
 *   5   6   3   4
 * 13 14  11  12
 *   9  10   7   8 
 * 1   2  31  0
 *   25  26  23 24 
 *   
 * -9.25   -4.25     1.75   6.75 
 *   -6.75    -1.75   4.25     9.25
 *   
 * dx and dy indexed by diode #.
 * Diode # is shown above. 
 * dx and dy is distance from x and y axes in millimeters
 * origin is in center of photodiode array.
 */
 
const double dx[] = {6.75, -9.25, -4.25, 4.25,   // 0 1 2 3 
                     9.25, -6.75, -1.75, 4.25,   // 4 5 6 7
                     9.25, -6.75, -1.75, 1.75,   // 8 9 10 11
                     6.75, -9.25, -4.25, 0,      // 12 13 14 15
                     0, 0, 0, 0,                 // 16 17 18 19
                     0, 0, 0, 4.25,              // 20 21 22 23
                     9.25, -6.75, -1.75, 1.75,   // 24 25 26 27
                     6.75, -9.25, -4.25, 1.75};  // 28 29 30 31

const double dy[] = {-3.75, -3.75, -3.75,         // 0 1 2
                     3.75, 3.75, 3.75, 3.75,      // 3 4 5 6
                     -1.25, -1.25, -1.25, -1.25,  // 7 8 9 10
                     1.25, 1.25, 1.25, 1.25,      // 11 12 13 14
                     0, 0, 0, 0, 0, 0, 0, 0,      // 15 16 17 18 19 20 21 22
                     -6.25, -6.25, -6.25, -6.25,  // 23 24 25 26
                     6.25, 6.25, 6.25, 6.25,      // 27 28 19 30
                     -3.75};                      // 31
             
RF24 radio(ce, ss); // CE, CSN
byte addresses[][6] = {"1Node","2Node"};  
int number_diodes_on;
int8_t data[2] = {0,0};
double dy_mean, dx_mean, dy_sum, dx_sum;
volatile bool mux_array[32];
volatile unsigned char mux_index = 0;

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.openWritingPipe(addresses[0]);
  radio.enableDynamicPayloads();
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(0,false);
  radio.stopListening();

  TC_setup();
  pinMode(BIT_1_PIN,  INPUT_PULLUP);
  pinMode(OUT_PIN,    INPUT_PULLUP);
  pinMode(RESET_PIN ,INPUT_PULLUP);
  pinMode(SIGNAL_PIN, INPUT_PULLUP);
  attachInterrupt(RESET_PIN, mux_index_reset, FALLING);
  attachInterrupt(BIT_1_PIN, mux_read, CHANGE);
  for (int i = 0 ; i < 32 ; i++) mux_array[i] = 0;
}

void loop() {
  dx_sum = 0;
  dy_sum = 0;
  number_diodes_on = 0;
  //unsigned long a = micros();
  for (int i = 0 ; i < 32 ; i++){
    if (mux_array[i]){
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
  data[0] = round(dx_mean/X_MAX * 127);
  data[1] = round(dy_mean/Y_MAX * 127);
  //unsigned long b = micros();
  
  //Serial.println(b-a);

  // Serial Statements somewhat intentional to induce necessary delay
  Serial.print(data[0]);
  Serial.print(',');
  Serial.println(data[1]);
  radio.write(&data, sizeof(data[0])*2);
}

void mux_read(){
  mux_array[mux_index] = digitalRead(OUT_PIN);
  mux_index++;
  if (mux_index > 31) mux_index = 0;
}

void mux_index_reset(){
  mux_index = 0;
}

/* Setup timer counters in a configurations such that 
 * LSB waveform has highest frequency and MSB waveform has lowest freuency,
 * Each bit's frequency is exactly half the lower bit's frequency.
 * 
 * Note: The bit order is not linear from 0 to 31, it jumps around
 */
void TC_setup()
{
  // Enable TIM 3,4,5 peripheral
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

  // 120Mhz master clock

  //Synchronize
  TIM3->SMCR = RESET;
  TIM4->SMCR = RESET;
  TIM5->SMCR = RESET;

  // TIM8 is master of TIM5 is master of TIM3 is master of TIM4
  TIM5->CR2   |= TIM_CR2_MMS_1;  // Mster mode
  TIM3->CR2   |= TIM_CR2_MMS_2;  // Mster mode
  TIM3->SMCR  |= TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1 | TIM_SMCR_SMS_2; // Slave mode
  TIM4->SMCR  |= TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1 | TIM_SMCR_SMS_2; // Slave mode
  TIM3->SMCR  |= TIM_SMCR_TS_1; // Internal clock signal routing Table 60.
  TIM4->SMCR  |= TIM_SMCR_TS_1; // Internal clock signal routing Table 60.
  
  // Configure TIM3
  TIM3->ARR    = 3;
  TIM3->CCR1   = 2;  // Compare capture reg 1 bit 4
  TIM3->CCR2   = 2;  // Compare capture reg 2 bit 2
  TIM3->CCMR1 |= TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; // PWM mode Channel 1
  TIM3->CCMR1 |= TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1; // Toggle mode Channel 2
  TIM3->CCER  |= TIM_CCER_CC1E | TIM_CCER_CC2E; //Capture Compare Enable channel 1 and 2
  TIM3->PSC    = 0;  // prescale clock
  TIM3->CR1   |= TIM_CR1_CEN; //Counter enable

  // Configure TIM4
  TIM4->ARR    = 3;
  TIM4->CCR1   = 2;  // Compare capture reg 1 bit 16 (MSB)
  TIM4->CCR2   = 2;  // Compare capture reg 2 bit 8
  TIM4->CCMR1 |= TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 ; // PWM mode Channel 1
  TIM4->CCMR1 |= TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1 ; // Toggle mode Channel 2
  TIM4->CCER  |= TIM_CCER_CC1E | TIM_CCER_CC2E; //Capture Compare Enable channel 1 and 2
  TIM4->PSC    = 0;  // prescale clock
  TIM4->CR1   |= TIM_CR1_CEN; //Counter enable

  // Configure TIM5
  TIM5->ARR    = 64;
  TIM5->CCR1   = 0;  // Compare capture reg 1 bit 1 (LSB)
  TIM5->CCMR1 |= TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1; // Toggle mode Channel 1
  TIM5->CCER  |= TIM_CCER_CC1E ; //Capture Compare Enable channel 1
  TIM5->PSC    = 16;  // prescale clock
  TIM5->CR1   |= TIM_CR1_CEN; //Counter enable

  // Additional setup
  HAL_Pin_Mode(D0,AF_OUTPUT_PUSHPULL);
  HAL_Pin_Mode(D1,AF_OUTPUT_PUSHPULL);
  HAL_Pin_Mode(D2,AF_OUTPUT_PUSHPULL);
  HAL_Pin_Mode(D3,AF_OUTPUT_PUSHPULL);
  HAL_Pin_Mode(WKP,AF_OUTPUT_PUSHPULL);
  pinMode(D4, INPUT);
  GPIO_PinAFConfig(PIN_MAP[D3].gpio_peripheral,PIN_MAP[D3].gpio_pin_source,GPIO_AF_TIM3);
  GPIO_PinAFConfig(PIN_MAP[D2].gpio_peripheral,PIN_MAP[D2].gpio_pin_source,GPIO_AF_TIM3);
  GPIO_PinAFConfig(PIN_MAP[D0].gpio_peripheral,PIN_MAP[D0].gpio_pin_source,GPIO_AF_TIM4);
  GPIO_PinAFConfig(PIN_MAP[D1].gpio_peripheral,PIN_MAP[D1].gpio_pin_source,GPIO_AF_TIM4);
  GPIO_PinAFConfig(PIN_MAP[WKP].gpio_peripheral,PIN_MAP[WKP].gpio_pin_source,GPIO_AF_TIM5);
}


