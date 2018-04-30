//SYSTEM_MODE(SEMI_AUTOMATIC)
String inData;

void setup() {
	Serial.begin(9600);
}

void loop() {
	while (Serial.available() > 0)
    {
        char recieved = Serial.read();
        inData += recieved; 
        
        // Process message when new line character is recieved
        if (recieved == '\n')
        {
            Serial.print(inData);

            inData = ""; // Clear recieved buffer
        }
    }
}