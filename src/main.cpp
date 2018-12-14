/*
  Arduino RFID Access Control
  Security !

  To keep it simple we are going to use Tag's Unique IDs
  as only method of Authenticity. It's simple and not hacker proof.
  If you need security, don't use it unless you modify the code

  Copyright (C) 2015 Omer Siar Baysal

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>	// Library for Mifare RC522 Devices
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#include <Servo.h>

/*
	For visualizing whats going on hardware
	we need some leds and
	to control door lock a relay and a wipe button
	(or some other hardware)
	Used common anode led,digitalWriting HIGH turns OFF led
	Mind that if you are going to use common cathode led or
	just seperate leds, simply comment out #define COMMON_ANODE,
 */

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define redLed 7		// Set Led Pins
#define greenLed 6
#define blueLed 8

#define servo_pin 5			// Set servo Pin (gpio5 = D1 sur nodemcu 0.9)
#define wipeB 3			// Button pin for WipeMode

#define RST_PIN	4  // RST-PIN für RC522 - RFID - SPI -
#define SS_PIN	2  // SDA-PIN für RC522 - RFID - SPI -

boolean match = false;          // initialize card match to false
boolean programMode = false;	// initialize programming mode to false

boolean doorIsLocked = false;
boolean webServerIsBusy = false; //trick to prevent the RFID reader from interrupting web requests processing

int successRead = 0;		// Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];		// Stores an ID read from EEPROM
byte readCard[4];		// Stores scanned ID read from RFID Module
byte masterCard[4];		// Stores master card's ID read from EEPROM

const char* ssid = "BOUM";
const char* password = "boumbadaboum";

const char* www_username = "admin";
const char* www_password = "plop";



// WIRING qui marche pour le nodemcu 0.9 avec le RC522
//confirmé débranché rebranché, ça marche
//ATTENTION les câbles pour MISO et MOSI sont interchangés sur ta serrure de porte à Troyes
// 3.3V------ 3.3V
// RST ------ (D2) - GPIO4
// GND------- GND
// IRQ -----
// MISO -------(D6) - GPIO12 - HMISO
// MOSI -------(D7) - GPIO13 - RXD2 - HMOSI
// SCK --------(D5) - GPIO14 - HSCLK
// SS ----------(D4) - GPIO2 - TXD1

MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance.
Servo daServo;
ESP8266WebServer server(80); //listen on port 80

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



///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //Arduino Pin Configuration
  //pinMode(redLed, OUTPUT);
  //pinMode(greenLed, OUTPUT);
  //pinMode(blueLed, OUTPUT);
  //pinMode(wipeB, INPUT_PULLUP);		// Enable pin's pull up resistor
  //pinMode(servo_pin, OUTPUT);
  //Be careful how relay circuit behave on while resetting or power-cycling your Arduino
  //digitalWrite(redLed, LED_OFF);	// Make sure led is off
  //digitalWrite(greenLed, LED_OFF);	// Make sure led is off
  //digitalWrite(blueLed, LED_OFF);	// Make sure led is off

  //Protocol Configuration
  Serial.begin(9600);	 // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware
  mfrc522.PCD_DumpVersionToSerial();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);   //If you set Antenna Gain to Max it will increase reading distance


  //setup wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  // Set up mDNS responder:
// - first argument is the domain name, in this example
//   the fully-qualified domain name is "esp8266.local"
// - second argument is the IP address to advertise
//   we send our IP address on the WiFi network
if (!MDNS.begin("doorlock")) {
  Serial.println("Error setting up MDNS responder!");
  while(1) {
    delay(1000);
  }
}
Serial.println("mDNS responder started");


  //hook du serveur pour ouvrir/fermer le verrou
  server.on("/", [](){
    webServerIsBusy = true; //pause RFID scanning to properly process web requests
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    moveServo();
    webServerIsBusy = false; //ok, ready to use RFID again
  });

  //hook du serveur pour donner le statut du verrou
  server.on("/status", [](){
    webServerIsBusy = true; //pause RFID scanning to properly process web requests
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication();
    getStatus();
    webServerIsBusy = false; //ok, ready to use RFID again
  });

  server.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");


  //initialisation de l'EEPROM (spécifique esp8266)
  EEPROM.begin(512);

  Serial.println(F("Access Control v3.3"));   // For debugging purposes
  //ShowReaderDetails();	// Show details of PCD - MFRC522 Card Reader details



  //Wipe Code if Button Pressed while setup run (powered on) it wipes EEPROM
  // if (digitalRead(wipeB) == LOW) {	// when button pressed pin should get low, button connected to ground
  //   //digitalWrite(redLed, LED_ON);	// Red Led stays on to inform user we are going to wipe
  //   Serial.println(F("Wipe Button Pressed"));
  //   Serial.println(F("You have 5 seconds to Cancel"));
  //   Serial.println(F("This will be remove all records and cannot be undone"));
  //   delay(5000);                        // Give user enough time to cancel operation
  //   if (digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
  //     Serial.println(F("Starting Wiping EEPROM"));
  //     // for (int x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
  //     //   if (EEPROM.read(x) == 0) {              //If EEPROM address 0
  //     //     // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
  //     //   }
  //     //   else {
  //     //     EEPROM.write(x, 0); 			// if not write 0 to clear, it takes 3.3mS
  //     //     EEPROM.commit(); //ajouté pour esp8266
        //   }
  //     // }
  //
  //     //adaptation pour l'ESP pour clear la EEPROM
  //     for (int i = 0; i < 512; i++)
  //     EEPROM.write(i, 0);
  //    EEPROM.commit(); //ajouté pour esp8266
  //     Serial.println(F("EEPROM Successfully Wiped"));
  //     //digitalWrite(redLed, LED_OFF); 	// visualize successful wipe
  //     delay(200);
  //     //digitalWrite(redLed, LED_ON);
  //     delay(200);
  //     // digitalWrite(redLed, LED_OFF);
  //     delay(200);
  //     // digitalWrite(redLed, LED_ON);
  //     delay(200);
  //     // digitalWrite(redLed, LED_OFF);
  //   }
  //   else {
  //     Serial.println(F("Wiping Cancelled"));
  //     digitalWrite(redLed, LED_OFF);
  //   }
  // }



  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
      //digitalWrite(blueLed, LED_ON);    // Visualize Master Card need to be defined
      //delay(200);
      //digitalWrite(blueLed, LED_OFF);
      //delay(200);
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( int j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    EEPROM.commit(); //ajouté pour esp8266;
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( int i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything Ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  //cycleLeds();    // Everything ready lets give user some feedback by cycling leds


  daServo.attach(servo_pin);
  servo(0);		// Make sure door is unlocked (power cuts etc)
  delay(200);
  servo(100);
  delay(200);
  servo(0);
  daServo.detach();
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
    ArduinoOTA.handle();
    server.handleClient();
    if (!webServerIsBusy){
    successRead = getID(); 	// sets successRead to 1 when we get read from reader otherwise 0
    // if (programMode) {
    //   //cycleLeds();              // Program Mode cycles through RGB waiting to read a new card
    // }
    // else {
    //   //normalModeOn(); 		// Normal mode, blue Power LED is on, all others are off
    // }
    //delay(0); //EXPERIMENTAL to prevent nodemcu soft resets when there is no card on the reader
  }
  if (successRead){
  if (programMode) {
    if ( isMaster(readCard) ) { //If master card scanned again exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  	// If scanned card's ID matches Master Card's ID enter program mode
      programMode = true;
      Serial.println(F("Hello Master - Entered Program Mode"));
      int count = EEPROM.read(0); 	// Read the first Byte of EEPROM that
      Serial.print(F("I have "));    	// stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) {	// If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));
        moveServo();
      }
      else {			// If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        denied();
      }
    }
  }
}
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void moveServo() {
  // digitalWrite(blueLed, LED_OFF); 	// Turn off blue LED
  // digitalWrite(redLed, LED_OFF); 	// Turn off red LED
  // digitalWrite(greenLed, LED_ON); 	// Turn on green LED

  //passer le tag une fois et le verrou s'ouvre. le passer une seconde fois et il se ferme.

  if (doorIsLocked) {
    servo(0); 		// Unlock door!
    doorIsLocked = false;
    server.send(200, "text/plain", "door unlocked");
  }
  else
  {
    servo(180); 		// Lock door!
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
    //delay(50);
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    //delay(50);
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

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) { 		// Before we write to the EEPROM, check to see if we have seen this card before!
    int num = EEPROM.read(0); 		// Get the numer of used spaces, position 0 stores the number of ID cards
    int start = ( num * 4 ) + 6; 	// Figure out where the next slot starts
    num++; 								// Increment the counter by one
    EEPROM.write( 0, num ); 		// Write the new count to the counter
    for ( int j = 0; j < 4; j++ ) { 	// Loop 4 times
      EEPROM.write( start + j, a[j] ); 	// Write the array values to EEPROM in the right position
    }
    EEPROM.commit(); //ajouté pour esp8266
    //successWrite();
	Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    //failedWrite();
	Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) { 		// Before we delete from the EEPROM, check to see if we have this card!
    //failedWrite(); 			// If not
	Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    int num = EEPROM.read(0); 	// Get the numer of used spaces, position 0 stores the number of ID cards
    int slot; 			// Figure out the slot number of the card
    int start;			// = ( num * 4 ) + 6; // Figure out where the next slot starts
    int looping; 		// The number of times the loop repeats
    int j;
    int count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a ); 	// Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--; 			// Decrement the counter by one
    EEPROM.write( 0, num ); 	// Write the new count to the counter
    for ( j = 0; j < looping; j++ ) { 				// Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j)); 	// Shift the array values to 4 places earlier in the EEPROM
    }
    for ( int k = 0; k < 4; k++ ) { 				// Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    EEPROM.commit(); //ajouté pour esp8266
    //successDelete();
	Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL ) 			// Make sure there is something in the array first
    match = true; 			// Assume they match at first
  for ( int k = 0; k < 4; k++ ) { 	// Loop 4 times
    if ( a[k] != b[k] ) 		// IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) { 			// Check to see if if match is still true
    return true; 			// Return true
  }
  else  {
    return false; 			// Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); 			// Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) { 		// Loop once for each EEPROM entry
    readID(i); 								// Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) { 	// Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; 				// The slot number of the card
      break; 					// Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0);			// Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) {  	// Loop once for each EEPROM entry
    readID(i); 					// Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {  	// Check to see if the storedCard read from EEPROM
      return true;
      break; 	// Stop looking we found it
    }
    else {  	// If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
// void successWrite() {
//   digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
//   digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
//   digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is on
//   delay(200);
//   digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
//   delay(200);
//   digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
//   delay(200);
//   digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
//   delay(200);
//   digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
//   delay(200);
//   digitalWrite(greenLed, LED_ON); 	// Make sure green LED is on
//   delay(200);
// }

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
// void failedWrite() {
//   digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
//   digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
//   digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
//   delay(200);
//   digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
//   delay(200);
//   digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
//   delay(200);
//   digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
//   delay(200);
//   digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
//   delay(200);
//   digitalWrite(redLed, LED_ON); 	// Make sure red LED is on
//   delay(200);
// }

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
// void successDelete() {
//   digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
//   digitalWrite(redLed, LED_OFF); 	// Make sure red LED is off
//   digitalWrite(greenLed, LED_OFF); 	// Make sure green LED is off
//   delay(200);
//   digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
//   delay(200);
//   digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
//   delay(200);
//   digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
//   delay(200);
//   digitalWrite(blueLed, LED_OFF); 	// Make sure blue LED is off
//   delay(200);
//   digitalWrite(blueLed, LED_ON); 	// Make sure blue LED is on
//   delay(200);
// }

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

/////////////////////Servo Method///////////////////////////////////////
void servo(int datPos)
{
  daServo.attach(servo_pin);
    daServo.write(datPos);
    delay(1000); //give the servo time to get to its assigned position
    daServo.detach();
}
