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
 *     
 *  Date : 2021/12/19
 *  
 *  Version : 1.0
 *  
 */

#include <avr/sleep.h>
#include <SoftwareSerial.h>  //Currently used : C:\Users\fguie\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.8.3\libraries\SoftwareSerial
// but this one exists and seems optimized for the Attiny84 : https://github.com/shimniok/ATtinySoftSerial

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
const byte SENSOR_ID=2;
const String FIRMWARE_VERSION="1.0";

const byte numReadings = 5;

//int test;

//RX=Arduino Pin 7
//TX=Arduino Pin 6
SoftwareSerial XBee (7,6);

// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  pinMode(LED, OUTPUT); 
  pinMode(PIN_LINK_TO_INT_PIN, INPUT_PULLUP);  

  pinMode(ENABLE_BATTERY_V_READING, OUTPUT); 
  digitalWrite(ENABLE_BATTERY_V_READING,HIGH);
    
  pinMode(WAKE_XBEE, OUTPUT); 
  digitalWrite(WAKE_XBEE,LOW);

  XBee.begin(9600);
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

/*float ReadVoltage() {
  
  digitalWrite(ENABLE_BATTERY_V_READING, LOW);
  analogReference(INTERNAL);    // set the ADC reference to 1.1V
  burn8Readings(ADC_PIN);            // make 8 readings but don't use them  
  delay(10);                    // idle some time
  unsigned int sensorValue = analogRead(ADC_PIN);    // read actual value
  digitalWrite(ENABLE_BATTERY_V_READING, HIGH);

  //with R1 = 10kOhm, R2 = 3.3kOhm
  //Max voltage 4.2v of fully charged battery produces = 1.042v on A0
  //According to voltage divider formula : Vout = Vin * (R2 / (R1 + R2))

  //So 4.2v is roughly represented by 890 value on A0

  XBee.print("analog sensor : ");
  XBee.println(sensorValue);
  
  float voltage = (sensorValue * 4.2) / 890;
  
  return voltage;
}*/

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
  //XBee.print("battery voltage : ");
  //XBee.println(String(battery_voltage,2));

  //XBee.print("{\"hue_id\":\"1\",\"battery\":\"" + String(battery_voltage,2) + "\",\"adc\":\""+String(test)+"\"}");
  XBee.print("{\"hue_id\":"+String(SENSOR_ID)+",\"battery\":\"" + String(battery_voltage,2) + "\",\"fw\":\""+FIRMWARE_VERSION+"\"}");

  //Wake up XBee
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
