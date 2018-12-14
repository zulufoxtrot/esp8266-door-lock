//déclaration des fonctions avant qu'elles soient utilisées pour éviter le "not declared in this scope" (apparemment c'est une pratique normale en c++)
void ShowReaderDetails();
int getID();
void cycleLeds();
void normalModeOn();
void readID( int number);
void writeID( byte a[] );
void deleteID( byte a[] );
boolean findID( byte find[] );
int findIDSLOT( byte find[] );
void servo(int datPos);
boolean isMaster( byte test[] );

void moveServo();
void denied();
void getStatus();

void successWrite();
void failedWrite();
void successDelete();



/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void moveServo() {
  // digitalWrite(blueLed, LED_OFF); 	// Turn off blue LED
  // digitalWrite(redLed, LED_OFF); 	// Turn off red LED
  // digitalWrite(greenLed, LED_ON); 	// Turn on green LED

  //passer le tag une fois et le verrou s'ouvre. le passer une seconde fois et il se ferme.

  if (doorIsLocked) {
    servo(180); 		// Unlock door!
    doorIsLocked = false;
    server.send(200, "text/plain", "door unlocked");
  }
  else
  {
    servo(0); 		// Lock door!
    doorIsLocked = true;
    server.send(200, "text/plain", "door locked");
  }



  //delay(1000); 						// Hold green LED on for a second
}

void getStatus(){
  if (doorIsLocked) {
    server.send(200, "text/plain", "door locked");
  }
  else
  {
    server.send(200, "text/plain", "door unlocked");
  }
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  // digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  // digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  // digitalWrite(redLed, LED_ON); 	// Turn on red LED
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
int getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    delay(50);
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    delay(50);
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
	// Get the MFRC522 software version
	byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
	Serial.print(F("MFRC522 Software Version: 0x"));
	Serial.print(v, HEX);
	if (v == 0x91)
		Serial.print(F(" = v1.0"));
	else if (v == 0x92)
		Serial.print(F(" = v2.0"));
	else
		Serial.print(F(" (unknown)"));
	Serial.println("");
	// When 0x00 or 0xFF is returned, communication probably failed
	if ((v == 0x00) || (v == 0xFF)) {
		Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
		while(true);  // do not go further
	}
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
  digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
  digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, LED_ON); 	// Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF); 	// Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF); 	// Make sure Green LED is off
  digitalWrite(servo_pin, HIGH); 		// Make sure Door is Locked
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; 		// Figure out starting position
  for ( int i = 0; i < 4; i++ ) { 		// Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i); 	// Assign values read from EEPROM to array
  }
}
