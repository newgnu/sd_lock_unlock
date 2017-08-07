#include <SPI.h>

int chipSelect = 10; //CS pin for SD Card. Pull low when talking
//int rspTime; //see how long certain commands take


void setup() {
  // put your setup code here, to run once:
  uint8_t res, ver, csd[16], cid[16];
  
  pinMode (chipSelect, OUTPUT);
  
  Serial.begin(115200);
  while (!Serial); //wait for serial port

//  Serial.println("Press ENTER to begin");
//  while(Serial.read() != '\n'); //wait for input

  SPI.begin();
  SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
  
  //send dummy clock. SD card won't start listening until >74 pulses
  digitalWrite(chipSelect, HIGH); //CS should be high for this part. 
  for(uint8_t i = 0; i < 10; i++) SPI.transfer(0xFF); //10x1-byte = 80 pulses.

  Serial.print("Set SD to SPI mode: ");
  //set sd to spi
  for (uint8_t i = 0; ((res = cardCommand(0,0)) != 0x01) && i < 0xFF; i++); //keep trying until status is 0x01 or timeout
  if (res == 1){
    Serial.println("OK");
  }else{
    Serial.print("Failed. 0x");
    spzp(res);
    Serial.println();
    return;
  }
    
  Serial.print("SD Card type: ");
  res = cardCommand(8, 0x1AA);  // cmd8 is only supported on sdhc. if this fails, it's sd. if it succeeds, it's sdhc.
  if (res == 0x04 || res == 0x7F) { // from spec sheets, I expect just an 'illegal command'. In practice, I get 0x7F.
    Serial.println("SD1\n");
    ver = 1;
  }else{
    for (uint8_t i = 0; i < 4; i++) res = rwByte(0xFF); //if sd2, it's a 5-byte response. I just need the last one.
    if (res != 0xAA) {
      Serial.print("Error: 0x");
      spzp(res);
      Serial.println();
      return;
    }else{
      Serial.println("SD2");
      ver = 2;
    }
  }

  Serial.print("SD Init: ");
  for (uint16_t i = 0; ((res = cardAdvCommand(41, (ver == 1) ? 0 : 0x40000000)) != 0x00) && i < 0xFFFF; i++);
  if (res != 0x00 ){
    Serial.print("Error: 0x");
    spzp(res);
    Serial.println();
    return;
  }else{
    Serial.println("OK");
  }

  for (uint16_t i = 0; (rwByte(0xFF) != 0xFF) && i < 0xFFFF; i++); //wait until not busy or timeout

  // if SD2, read CCS bit in OCR table to see if size is in blocks or bytes. (SD vs SDHC)
  if (ver == 2){
    Serial.print("Read OCR: ");
    //for (uint8_t i = 0; (res = cardCommand(58,0) != 0x00) && i < 0xFF; i++);
    res = cardCommand(58,0);
    if (res){ //expecting 0x00 status from card. anything else is failure
      Serial.print("Error: 0x");
      spzp(res);
      Serial.println();
      return;
    }
    if ((rwByte(0xFF) & 0xC0) == 0xC0) { //command 58 returns 5 bytes total. first byte after status contains the CCS bit we want to know about
      ver = 3;
      Serial.println("Found CCS bit. Card is SDHC");
    }else{
      Serial.println("No CCS bit. Card is not SDHC");
    }
    for (uint8_t i = 0; i < 3; i++) rwByte(0xFF); //read the rest, but disregard them.
  }

  // set 32B for setting/clearing passwords. Length can only be 16 chars, so it doesn't make sense to send 512B
    Serial.print("Setting block size to 32B: ");
    res = cardCommand(16, 0x20);
    if (res){
      Serial.print("Error: 0x");
      spzp(res);
      Serial.println();
    }else{
      for (uint16_t i = 0; ((res = rwByte(0xFF)) != 0xFF) && i < 0xFFFF; i++); // wait until not busy or time out
      if (res){
        Serial.println("OK");
      }else{
        Serial.print("Busy: 0x");
        spzp(res);
        Serial.println();
        return;
      }
    }
//  }

  Serial.print("Status: 0x");
  spzp(cardCommand(13,0));
  spzp(rwByte(0xFF)); //status returns 2 bytes. Read a second one.
  Serial.println();

  Serial.print("Read CSD: 0x");
  spzp(cardCommand(9,0));
  for (uint8_t i = 0; ((res = rwByte(0xFF)) != 0xFE) && i != 0xFF; i++); // look for 0xFE byte. Data starts immediately after.
  if (res == 0xFE){
    for (uint8_t i = 0; i < 16; i++){ //returns a 16-byte + CRC response.
      csd[i] = rwByte(0xFF);
      spzp(csd[i]);
    }
    for (uint8_t i = 0; i < 2; i++) rwByte(0xFF); //dump 2 more bytes. These are for CRC. 
  }
  Serial.println();

}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t opt, inChar;
  bool dataRead;

  Serial.println();
  Serial.println("1) Lock Card");
  Serial.println("2) Unlock Card");
  Serial.println("3) Force Erase");
  Serial.println("4) Card Status");

  dataRead = 0;
  while (! dataRead){ //wait for user input
    while(Serial.available()){
      inChar = (char)Serial.read();
      if (inChar == '\n'){
        dataRead = 1;
      }else{
        opt = inChar - 0x30; // convert ascii char to integer. ie: '1' is ascii 0x31. 'A' is ascii 0x41, etc.
      }
    }
  }

  if (opt == 1){ //lock card
    cardLockUnlock(1);
  }else if(opt == 2){ //unlock card
    cardLockUnlock(2);
  }else if(opt == 3){ //force erase
    cardLockUnlock(3);
  }else if(opt == 4){
    //do nothing. status is printed after every entry anyway.
    //this just skips the error
  }else{
    Serial.println("Error: Selection out of range");
  }

  Serial.print("Status: 0x");
  spzp(cardCommand(13,0));
  spzp(rwByte(0xFF)); //status returns 2 bytes. Read a second one.
  Serial.println();
}

uint8_t rwByte(uint8_t cmd){
  return rwByte(cmd, 1);
}

uint8_t rwByte(uint8_t cmd, bool autoCS) {
  uint8_t res;
  if (autoCS) digitalWrite(chipSelect, LOW);
  res = SPI.transfer(cmd);
  if (autoCS) digitalWrite(chipSelect, HIGH);
  return res;
}

uint8_t cardCommand(uint8_t cmd, uint32_t arg) {
  digitalWrite(chipSelect, LOW);
  for (uint16_t i = 0; (SPI.transfer(0xFF) != 0xFF) && i < 0xFFFF; i++); //wait until not busy
  
  SPI.transfer(cmd | 0x40);
  for (int8_t i = 24; i >= 0; i -= 8) SPI.transfer(arg >> i); //fancy way of sending 1 byte at a time for 4 bytes

  uint8_t crc = 0xFF;
  if (cmd == 0) crc = 0x95; // crc for CMD0 with no arg
  if (cmd == 8) crc = 0x87; // crc for CMD8 with arg 0x01AA

  SPI.transfer(crc);

  uint8_t rsp;
  for (uint8_t i = 0; ((rsp = SPI.transfer(0xFF)) & 0x80) && i < 0xFF; i++);// rspTime = i; //keep reading until first bit is 0 or timeout at 255 bytes. Save time for analasys
  digitalWrite(chipSelect, HIGH);
  return rsp;
}

uint8_t cardAdvCommand(uint8_t cmd, uint32_t arg) {
  cardCommand(55,0);
  return cardCommand(cmd,arg);
}

void cardLockUnlock(int locktype){
  // 1 = lock
  // 2 = unlock
  // 3 = force erase
  
  bool dataRead;
  char pass[16];
  uint8_t res, len, inChar;

  if (locktype == 1){
    Serial.println("Lock Password (16 char max):");
  }else if (locktype == 2){
    Serial.println("Unlock password (16 char max):");
  }else if (locktype == 3){
    Serial.println("This option will clear the password, but you will lose all data");
    Serial.println("Force full erase. Are you sure? (Y/n)");
  }else{
    Serial.println("Unknown locktype. Aborting");
    return;
  }

  dataRead = 0;
  len = 0;
  while (! dataRead){ //wait for user input
    while(Serial.available()){
      inChar = (char)Serial.read();
      if (inChar == '\n'){
        dataRead = 1;
        for (uint8_t i = len; i < 17; i++) pass[i] = 0x00; //fill the rest of the string with null chars
      }else{
        pass[len] = inChar;
        len++;
      }
    }
  }

  if (locktype == 3){
    //password is now a yes/no prompt
    if (pass[len-1] != 'Y' && pass[len-1] != 'y'){
      Serial.println("Aborting at user request.");
      return;
    }else{
      //clear password.
      for (int8_t i = 0; i < 17; i++) pass[i] = 0x00;
      len = 0;
    }
  }

  res = cardCommand(42,0);
  if (res){
    Serial.print("CMD42 Error: 0x");
    spzp(res);-
    Serial.println();
    return;
  }else{
    digitalWrite(chipSelect, LOW);
    
    rwByte(0xFF, 0); //need at least one byte between response and token
    rwByte(0xFE, 0); //Data token
    if (locktype == 1){ //The only real difference between lock/unlock/erase is the data header. bits are 4x reserved (set 0), ERASE, LOCK_UNLOCK, CLR_PWD, SET_PWD.
      rwByte(0x05, 0); // 0000 0101 -- lock_unlock and set_pw set to 1
    }else if(locktype == 2){
      rwByte(0x02, 0); // 0000 0010 -- lock_unlock to 0, clr_pwd set to 1.
    }else if(locktype == 3){
      rwByte(0x08, 0); // 0000 1000 -- erase set to 1.
    }
    rwByte(len, 0);  //next byte is pwd length
    for (int8_t i = 0; i < 17; i++) rwByte(pass[i], 0); //then comes the password.
    for (int8_t i = 0; i < 13; i++) rwByte(0x00, 0); //fill the rest of the data block with 0x00. 
    rwByte(0xFF, 0); //2-byte dummy CRC
    rwByte(0xFF, 0);

    digitalWrite(chipSelect, HIGH);
    
    Serial.println();
    res = rwByte(0xFF); //Response byte should come immediately after last CRC.
    if (res == 0xE5){
      Serial.println("OK");
    }else{
      Serial.print("Error: 0x");
      spzp(res);
      Serial.println();
    }
  }
}

void spzp(uint8_t num){ //Serial print, zero-padded
  if (num < 0x10) Serial.print("0");
  Serial.print(num, HEX);
}
