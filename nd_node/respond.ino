
void respond(byte respb, byte byte1, byte byte2, byte byte3)
{
  if (resp){
      if (incID == 0xFE){
       unsigned int dt;
      
       if (myID == 0xFD)
          dt = myID + random(0, 252);
        else 
          dt = myID;
        delay(dt);
      }
      
      digitalWrite(sendpin, HIGH);
      Serial.write(0xFF); //start byte
      Serial.write(myID); // tell master who you are
      Serial.write(respb); //response cmd
      Serial.write(byte1);
      Serial.write(byte2);
      Serial.write(byte3);
      Serial.write(byte((0xFF + myID + respb + byte1 + byte2 + byte3)%2)); //checksum (1 or 0)
      Serial.write(byte(0x00));//just a spacer to give the checksum time to send 
                               //(this doesn't always make it through)
      Serial.flush();
      digitalWrite(sendpin, LOW);
  }
}



