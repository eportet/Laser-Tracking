#include <Servo.h>

Servo myservo;  // create servo object to control a servo
                // a maximum of eight servo objects can be created

int pos = 0;    // variable to store the servo position

void setup()
{
  Serial.begin(9600);
  myservo.attach(9);   // attach the servo on the D0 pin to the servo object
  myservo.write(25);    // test the servo by moving it to 25°
}

void loop()
{
  while(!Serial.available());
  int d = Serial.parseInt();
  if(d != 0){
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    Serial.println(d);
  }
  delay(100);                       // waits 15ms for the servo to reach the position
}
