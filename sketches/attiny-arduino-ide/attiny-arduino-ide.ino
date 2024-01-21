/*
 *  Touch-Zigbee project
 *  
 *  Description:
 *     
 *     This program uses interrupt to detect when the capacitive button is touched
 *     Then it wakes up the XBee and send a message
 *     The message is received on another XBee plugged in a Raspberry.
 *     A Nodered flow then toggle the Philipps Hue light bulb on and off depending on the current light bulb state.
 *     
 *     This is an ultra low power battery operated design. It consumes only 10uA when sleeping.
 *     
 *     More info here : https://github.com/fguiet/touch-zigbee
 *     
 *  Board used : Using board 'ATtinyX4' from platform in folder: C:\Users\fguie\AppData\Local\Arduino15\packages\attiny\hardware\avr\1.0.2   
 *               Using Arduino As ISP to program the ATtiny84
 *               Careful : Serial only work at Clock Internal 1Mhz (do not use Clock Internal 8Mhz)
 *     
 *
 *  History : 
 *     - 2021/12/19 - Creation date - Firware Version 1.0
 *     - 2024/01/21 - Replace use of SoftwareSerial library, use of ATtinySerialOut at 38400 bauds. 
 *                  - ATtinySerialOut consumes less memory and allow use of 38400 bauds. SoftwareSerial only works at 9600 bauds
 *                  - Change to XBee API Mode 2 instead of Transparent mode
 *                  - ATtinySerialOut must be connected to PIN PB1 = Physical PIN 3 otherwise it does not work (so PCB schematic has been changed as well)
 *                  - Firmware Version is now 2.0
 *  
 *  Version : 2.0
 *  
 */

#include <avr/sleep.h>

//Uncomment this line below if you want to use 38400 bauds instead of 115200 bauds
#define TINY_SERIAL_DO_NOT_USE_115200BAUD

//PIN PB1 = Physical PIN 3
#define TX_PIN 1

//Work only clock internal set to 1Mhz and with TX_PIN set to 2, baud is 38400 bauds
#include "ATtinySerialOut.hpp"  // Available as Arduino library "ATtinySerialOut"

// ATTIny84 
// Use Arduino Pin on the pinout 
const byte LED = 10;
const byte PIN_LINK_TO_INT_PIN=8;
const byte INT_PIN=0; //This is the interrupt number link to pin 8
const byte WAKE_XBEE=5;
const byte ADC_PIN=0; //Arduino pin 13
const byte ENABLE_BATTERY_V_READING=4;

//Hue light bulb
//Office = 2
const String SENSOR_ID="2";
const String FIRMWARE_VERSION="2.0.0";

const byte numReadings = 5;

// the setup routine runs once when you press reset:
void setup() {
  // Must be called once if pin is not set to output otherwise
  initTXPin();

  // initialize the digital pin as an output.
  pinMode(LED, OUTPUT); 
  pinMode(PIN_LINK_TO_INT_PIN, INPUT_PULLUP);  

  pinMode(ENABLE_BATTERY_V_READING, OUTPUT); 
  digitalWrite(ENABLE_BATTERY_V_READING,HIGH);
    
  pinMode(WAKE_XBEE, OUTPUT); 
  digitalWrite(WAKE_XBEE,LOW);
}

// Needed to compute ZigBee API 2 mode frame length
void computeMSB_LSB(uint16_t number, uint8_t &msb, uint8_t &lsb) {
    msb = (number >> 8) & 0xFF; // Shift right by 8 bits to get MSB
    lsb = number & 0xFF;        // Mask to get LSB
}

// See https://forum.arduino.cc/t/how-to-calculate-checksum-for-16bit-address-xbee-tx-frame/439001/4
uint8_t calculateChecksum(const uint8_t* data, size_t length) {  
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; ++i) {
    //Serial.println(data[i],HEX);
    checksum += data[i];
  }  
  checksum &= 0xFF;        // low order 8 bits
  checksum = 0xFF - checksum; // subtract from 0xFF

  return  checksum;
}

//Create the ZigBee Mode API 2 - frame
//
//Using 0x00 - 64-bit Transmit Request (Deprecated one, should use 0x10 but I am using old XBee S1 (802.15.4 protocol stack)
//See https://www.digi.com/resources/documentation/Digidocs/90001477/reference/r_frame_0x00.htm?tocpath=API%20frames%7C_____3
void createAndSendZigBeeAPIFrame(String message) {
  
  //Gets an array of String : message
  byte messageArray[message.length()+1];
  message.getBytes(messageArray, message.length()+1);

  //Get messageArraySize
  int messageArraySize = sizeof(messageArray) / sizeof(messageArray[0]);
  
  //Declare array with needed space for message + need byte of frame
  uint8_t frame[14+sizeof(messageArray)]; 
  
  //Empty frame
  for (int i=0; i < sizeof(frame) / sizeof(frame[0]) -1; i++) {    
    frame[i] = 0;    
  }

  //Initialize Frame
  frame[0] = 0x7E; // Start delimiter
  //
  frame[1] = 0x00; // Length MSB (to be updated later)
  frame[2] = 0x00; // Length LSB (to be updated later)
  //
  frame[3] = 0x00; // Frame type ie 0x00 - Tx (Transmit) - Request: 64-bits address
  // 
  frame[4] = 0x42; // Frame ID 0x42 why not ;-)
  //
  // Desination address : here my coordinator 
  frame[5] = 0x00;
  frame[6] = 0x13;
  frame[7] = 0xA2;
  frame[8] = 0x00;
  frame[9] = 0x40;
  frame[10] = 0xB2;
  frame[11] = 0x45;
  frame[12] = 0xBF;
  //
  frame[13] = 0x00; // Options
  //
  // DATA      
  for (int i=0; i < messageArraySize-1; i++) { 
    //Serial.println("TOTO:"+String(messageArray[i], HEX));
    frame[14+i] = messageArray[i];    
  }
  //Checksum = all data except start + length (on non escaped data)
  //Start compute at frame+3 (ie frame type) to 11 (frame type to options + message size)
  frame[14+messageArraySize-1] = calculateChecksum(frame+3, 11+messageArraySize-1);

  //In API 2 mode, the length field does not include any escape character in the frame and the checksum is calculated with non-escaped data.  
  uint8_t msb, lsb;
  //Serial.println("Message Size = " + String(11+messageArraySize-1));
  computeMSB_LSB(11+messageArraySize-1, msb, lsb);  

  //Serial.print("MSB: 0x");
  //Serial.println(msb, HEX);

  //Serial.print("LSB: 0x");
  //Serial.println(lsb, HEX);

  //Set frame length
  frame[1] = msb;
  frame[2] = lsb;

  int j=1;
  //Compute number final arraylenght necessary with escaped characters needed
  for (int i=1; i < 14+messageArraySize; i++) {
    
    if (frame[i] == 0x7E 
     || frame[i] == 0x7D 
     || frame[i] == 0x13
     || frame[i] == 0x11)
      j++;
    
    j++;
  }

  //
  // DEBUG
  //
  /*Serial.println("DEBUT FRAME");
  for (int i=0; i < sizeof(frame); i++) {    
    Serial.print("0x" + String(frame[i], HEX) + " ");
  }
  Serial.println("FIN FRAME");
  return ;*/

  //Escape frame (all byte exept Start delimiter)
  uint8_t escapedFrame[j];

  //Start delimiter
  escapedFrame[0] = frame[0];
  j = 1;

  for (int i=1; i < 14+messageArraySize; i++) {
    
    if (frame[i] == 0x7E) {
      escapedFrame[j] = 0x7D;
      j++;
      escapedFrame[j] = 0x5E;      
      j++;
      continue;
    }

    if (frame[i] == 0x7D) {
      escapedFrame[j] = 0x7D;
      j++;
      escapedFrame[j] = 0x5D;      
      j++;
      continue;
    }

    if (frame[i] == 0x13) {
      escapedFrame[j] = 0x7D;
      j++;
      escapedFrame[j] = 0x33;      
      j++;
      continue;
    }

    if (frame[i] == 0x11) {
      escapedFrame[j] = 0x7D;
      j++;
      escapedFrame[j] = 0x31;      
      j++;
      continue;
    }
    
    //other case
    escapedFrame[j] = frame[i];
    j++;

  }
  //
  // DEBUG
  //
  /*Serial.println("DEBUT FRAME");
  for (int i=0; i < sizeof(escapedFrame); i++) {    
    Serial.print("0x" + String(escapedFrame[i], HEX) + " ");
  }
  Serial.println("FIN FRAME");*/

  //Send frame
  //Serial.write(escapedFrame);
  
  //char tt []= {0x7E, 0x00, 0x7D, 0x33, 0x00, 0x42, 0x00, 0x7D, 0x33, 0xA2, 0x00, 0x40, 0xB2, 0x45, 0xBF, 0x00, 0x66, 0x72, 0x65, 0x64, 0x65, 0x72, 0x69, 0x63, 0xCE};

  //WORKING
  /*char tt [] = {0x7e, 0x0, 0x2a, 0x0, 0x42, 0x0, 0x7d, 0x33, 0xa2, 0x0, 0x40, 0xb2, 0x45, 0xbf, 0x0, 0x7b, 0x22, 0x74, 0x69, 0x70, 0x22, 0x3a, 0x22, 0x74, 0x72, 0x75, 0x65, 0x22, 0x2c, 0x22, 0x62, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79, 0x22, 0x3a, 0x22, 0x32, 0x2e, 0x34, 0x32, 0x22, 0x7d, 0x5d, 0x9c};
  for (int i=0;i<sizeof(tt);i++) {
    Serial.print((char)tt[i]);    
  }*/

  //XBee.flush();

  //uint8_t tt []= {0x7E, 0x00, 0x7D, 0x33, 0x00, 0x42, 0x00, 0x7D, 0x33, 0xA2, 0x00, 0x40, 0xB2, 0x45, 0xBF, 0x00, 0x66, 0x72, 0x65, 0x64, 0x65, 0x72, 0x69, 0x63, 0xCE};
  //char* ptr = escapedFrame;
  //Serial.print(tt);

  for (int i=0;i<sizeof(escapedFrame);i++) {
    Serial.print((char)escapedFrame[i]);    
  }

  //Serial.flush();
}

float averageVoltage() {

  analogReference(INTERNAL);    // set the ADC reference to 1.1V
  
  int total = 0;
  
  digitalWrite(ENABLE_BATTERY_V_READING, LOW);
  
  for(byte i=0;i<numReadings;i++) {
    total = total + analogRead(ADC_PIN);
  }
  
  digitalWrite(ENABLE_BATTERY_V_READING, HIGH);
  
  int averageSensor = total / numReadings; 
  //test = averageSensor;
  
  //XBee.print("analog sensor : ");
  //XBee.println(averageSensor);

  //with R1 = 10kOhm, R2 = 3.3kOhm
  //Max voltage 4.2v of fully charged battery produces = 1.042 on A0
  //According to voltage divider formula : Vout = Vin * (R2 / (R1 + R2))

  //So 4.2v is roughly represented by 992 value on A0
  
  float voltage = (averageSensor * 4.28) / 992;
  
  return voltage;           
}

// the loop routine runs over and over again forever:
void loop() {

  attachInterrupt(INT_PIN, blink, LOW);
  
  //Go to sleep
  sleep_function();
  
  detachInterrupt(INT_PIN);   

  //Wake up XBee
  digitalWrite(WAKE_XBEE,HIGH);

  delay(50);

  float battery_voltage = averageVoltage();

  //Send XBee message  
  //XBee.print("fred");
  //XBee.println(String(battery_voltage,2));

 // XBee.print("{\"hue_id\":\"1\",\"battery\":\"" + String(battery_voltage,2) + "\",\"adc\":\""+String(test)+"\"}");
  // Serial.print("{\"hue_id\":"+String(SENSOR_ID)+",\"battery\":\"" + String(battery_voltage,2) + "\",\"fw\":\""+FIRMWARE_VERSION+"\"}");
  String message = "{\"id\":\""+SENSOR_ID+"\",\"bat\":\"" + String(battery_voltage,2) + "\",\"fw\":\""+FIRMWARE_VERSION+"\"}";
  //String message = "frederic";
  createAndSendZigBeeAPIFrame(message);

  //DEBUG WORKS
  /*
  char tt [] = {0x7e, 0x0, 0x2a, 0x0, 0x42, 0x0, 0x7d, 0x33, 0xa2, 0x0, 0x40, 0xb2, 0x45, 0xbf, 0x0, 0x7b, 0x22, 0x74, 0x69, 0x70, 0x22, 0x3a, 0x22, 0x74, 0x72, 0x75, 0x65, 0x22, 0x2c, 0x22, 0x62, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79, 0x22, 0x3a, 0x22, 0x32, 0x2e, 0x34, 0x32, 0x22, 0x7d, 0x5d, 0x9c};
  for (int i=0;i<sizeof(tt);i++) {
    Serial.print((char)tt[i]);    
  }
  */
  
  //wait for message to be sent
  delay(100);

  //Go to sleep XBee
  digitalWrite(WAKE_XBEE,LOW);  
}

void blink() {
  digitalWrite(LED, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);               // wait for a second
  digitalWrite(LED, LOW);    // turn the LED off by making the voltage LOW
  delay(500);               // wait for a second
}

void sleep_function()
{ 
   
  // disable ADC
  ADCSRA &= ~_BV(ADEN);                   // ADC off
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);//set deep sleep mode
  sleep_enable(); //enable sleep
  sleep_mode(); //start sleep
  sleep_disable(); //code continues here after wake up  

  // enable ADC
  ADCSRA |= _BV(ADEN);                    // ADC on
}
