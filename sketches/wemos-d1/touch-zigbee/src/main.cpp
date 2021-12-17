#include <Arduino.h>
#include <SoftwareSerial.h>

// D1 Pinout - https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

//RX=GPIO5=D5 (but it is D1 on silk screen)
//TX=GPIO4=D4 (but it is D2 on silk screen)
//Only TX will be used here
SoftwareSerial XBee (D5,D4);


void setup() {
  //Must be the same than Serial interface configured in XCTU
  XBee.begin(9600);
  Serial.begin(9600);
}

void loop() {

  XBee.println("Bonjour");
  Serial.println("Bonjour");
  delay(1000);
}