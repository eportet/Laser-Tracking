#include <Servo.h>     //Servo library
#include <Wire.h>
#include <LIDARLite.h>

// Globals
LIDARLite lidarLite;
int cal_cnt = 0;
 
 Servo servo_test;      //initialize a servo object for the connected servo  
                
 int angle = 0;    
 //int potentio = A0;      // initialize the A0analog pin for potentiometer
 int potentio=0;
 int count = 1;
 int mode=1; // tells which direction to move 
 void setup() 
 { 
  Serial.begin(9600); // Initialize serial connection to display distance readings

  lidarLite.begin(0, true); // Set configuration to default and I2C to 400 kHz
  lidarLite.configure(0); // Change this number to try out alternate configurations
  servo_test.attach(9);   // attach the signal pin of servo to pin9 of arduino
 } 
 
 void loop() 
 { 
  if (potentio==0){
    mode=1;
   }
  if(potentio==170){
    mode=-1;
   }
   potentio=potentio+(mode*1);
   angle=potentio;
  //angle = analogRead(potentio);            // reading the potentiometer value between 0 and 1023 
  //angle = map(angle, 0, 1023, 0, 179);     // scaling the potentiometer value to angle value for servo between 0 and 180) 
   servo_test.write(angle);    //command to rotate the servo to the specified angle 
 int dist;

  // At the beginning of every 100 readings,
  // take a measurement with receiver bias correction
  if ( cal_cnt == 0 ) {
    dist = lidarLite.distance();      // With bias correction
  } else {
    dist = lidarLite.distance(false); // Without bias correction
  }

  // Increment reading counter
  cal_cnt++;
  cal_cnt = cal_cnt % 100;

  // Display distance
  Serial.print(dist);
  Serial.println(" cm");

  delay(10);      
 }  
