#include <EEPROM.h>
unsigned int byteIndex;
unsigned int sendpin = 13;
unsigned int recvpin = 12;
byte incID, cmd, b1, b2, b3, cksm;
byte motor[6];
byte tmp;
boolean resp = true;
byte myID = 209;
bool ALLON = 0;
bool USEDEMO = 0;

void setup() {
  randomSeed(analogRead(1));
  //myID = EEPROM.read(0);
  EEPROM.write(0, myID);  
  
  incID = cmd = b1 = b2 = b3 = cksm = 0xFF;
  byteIndex  = 0;
  
  motor[0] = ALLON ? 255 : 0; //3 pin 9
  motor[1] = ALLON ? 255 : 0; // pin 10
  motor[2] = ALLON ? 255 : 0; // pin 11
  motor[3] = ALLON ? 255 : 0; // pin 3
  motor[4] = ALLON ? 255 : 0; // pin 5 
  motor[5] = ALLON ? 255 : 0; // pin 6
 
  pinMode(sendpin, OUTPUT);
  pinMode(recvpin, OUTPUT);
  //available pins: digital 2-11, analog 0-5 (15 total)
  pinMode(3, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  
  digitalWrite(sendpin, LOW);
  digitalWrite(recvpin, LOW); 
  Serial.begin(115200);
}

/*
 * Protocol: *
 Start Byte (FF) ::  0xFF (255)
 Device Number :: 0x00-0xFD (0->253)
 CMD :: 0x00-0xFD (0->253) 
 DATA (3 bytes) :: 0x00-0xFD (0->253) each byte
 checksum (even parity) :: 0x00 or 0x01 (0 or 1)
*/
