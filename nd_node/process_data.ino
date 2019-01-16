
void process_data() {
  // do check sum
  unsigned int check = 0xFF+ incID+ cmd+ b1+ b2+ b3+ cksm; // check it's a valid message
  if (check%2 == 0)
  {   
    switch (cmd)
    {
    case 0xFC: // set the motors:
       
      motor[0] = (b1 >> 4) * 16 + 15;
      motor[1] = (b1 & 15) * 16 + 15;
      motor[2] = (b2 >> 4) * 16 + 15;
      motor[3] = (b2 & 15) * 16 + 15;
      motor[4] = (b3 >> 4) * 16 + 15;
      motor[5] = (b3 & 15) * 16 + 15;

      if (motor[0]) {
         analogWrite( 9, motor[0]) ;
      } else {
        digitalWrite( 9, LOW);
      }
      if (motor[1]) {
         analogWrite(10, motor[1]) ;
      } else {
        digitalWrite(10, LOW);
      }
      if (motor[2]) {
         analogWrite(11, motor[2]) ;
      } else {
        digitalWrite(11, LOW);
      }
      if (motor[3]) {
         analogWrite( 3, motor[3]) ;
      } else {
        digitalWrite( 3, LOW);
      }
      if (motor[4]) {
         analogWrite( 5, motor[4]) ;
      } else {
        digitalWrite( 5, LOW);
      }
      if (motor[5]) {
         analogWrite( 6, motor[5]) ;
      } else {
        digitalWrite( 6, LOW);
      }
      /*
      analogWrite(10, motor[1]); 
      analogWrite(11, motor[2]);  
      analogWrite( 3, motor[3]); 
      analogWrite( 5, motor[4]);  
      analogWrite( 6, motor[5]);    
      */
        
      break;

    case 0xFB://request a color from the device
         //respond(0xF8, r, g, b);
      break;
      
   case 0xF6: // get ID
       //  delay(dt); // collision avoidance:: not working
       respond(0xF4, byte(0x00), byte(0x00), byte(0x00));
     break;
     
   case 0xF5: // set ID
       myID = random(b2, b3); 
       EEPROM.write(0, myID);
     break;
  
     case 0xF3: // turn off responses
       resp = !resp; 
     break;
    }
  } 
  // clear when finished
  clearMsgBuf();
}



