
void loop() {

  if (USEDEMO) {
    tmp++;
    for (int i=0; i<6; i++) {
      motor[i] = (tmp % 6) == i ? 220 : 0;
    }
    analogWrite( 9, motor[0]);  
    analogWrite(10, motor[1]); 
    analogWrite(11, motor[2]);  
    analogWrite( 3, motor[3]); 
    analogWrite( 5, motor[4]);  
    analogWrite( 6, motor[5]);  
    delay(1000);  
  }
  
  if (Serial.available() > 0){
    byte b = Serial.read();
    // Checksum
    if (byteIndex == 6 && b != 0xFF){
      if ( b < 0x02){ // should only be 0 or 1
          cksm = b;
          process_data();
       } else {
          clearMsgBuf();
       }
    }
    // DATA Byte 3
    if (byteIndex == 5 && b != 0xFF){
      if (b3 == 0xFF){
          b3 = b;
          byteIndex = 6;
       } else {
          clearMsgBuf();
       }
    }
    // DATA Byte 2
    if (byteIndex == 4 && b != 0xFF){
      if (b2 == 0xFF){
          b2 = b;
          byteIndex = 5;
       } else {
          clearMsgBuf();
       }
    }
    // DATA Byte 1
    if (byteIndex == 3 && b != 0xFF){
      if (b1 == 0xFF){
          b1 = b;
          byteIndex = 4;
       } else {
          clearMsgBuf();
       }
    }
    // CMD
    if (byteIndex == 2 && b != 0xFF){
      if (cmd == 0xFF){
          cmd = b;
          byteIndex = 3;
       } else {
          clearMsgBuf();
       }
    }
    // Device Number -- make sure the ID is valid  [0xFE == all devices]
    if (byteIndex == 1 && b != 0xFF){
       if (incID == 0xFF && (b == myID || b == 0xFE)){
          incID = b;
          byteIndex = 2;
       } else {
          clearMsgBuf();
       }
    }
    //Start Byte (FF)
    if (byteIndex == 0 && b == 0xFF){
      byteIndex = 1;
    }
  }
}

void clearMsgBuf()
{
    byteIndex = 0;
    incID = cmd = b1 = b2 = b3 = cksm = 0xFF; 
}



