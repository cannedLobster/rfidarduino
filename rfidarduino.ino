/*
 * Simple security access code implementing RFID
 * 
 * Written with help of online guide and examples.
 * 
 * Sunny Xu
 */


// Imports

#include <SPI.h>
#include <MFRC522.h>  //Mifare RC522 necessary libraries

#include <EEPROM.h> // Read/write from/to Arduino's EEPROM

#include <Servo.h>  // Physical indication of granted access
#include <LiquidCrystal_I2C.h>

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

// Set LED Pins
#define redLed 4
#define greenLed 5 
#define yellowLed 6

// Set Button Pin for Reset
#define resetB 8

// Set Servo Pin
#define servoPin 7

// initialize match confirmation and programming mode false
boolean match = false;
boolean programming = false;
boolean replaceMaster = false;

uint8_t successRead;
byte masterCard[4];
byte storedCard[4];
byte readCard[4]; // Scanned card from RFID module

// Creating MFRC552 instance
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Creating LCD instance
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Creating Servo instance
Servo lock;

void setup() {
  lock.attach(servoPin);
  //OPEN: lock.write(0);
  //CLOSE: lock.write(180);
  
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(resetB, INPUT_PULLUP);  // Enable pin's pull up resistor
  pinMode(servoPin, OUTPUT);
  // Making sure led is off
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(yellowLed, LED_OFF);

  // Protocol Config
  Serial.begin(9600);
  SPI.begin();  
  mfrc522.PCD_Init(); // Initialize RFID reader hardware

  // Debugging purposes
  Serial.println(F("RFID MFRC552 SECURITY ACCESS PROJECT "));
  ShowReaderDetails();

  // Antenna Gain set to max will increase reading distance from sensor
  //mfrcc552.PCD_SetAntennaGain(mfrc552.RxGain_max);

  // Wipe code - If reset button held while on, EEPROM is wiped
  if (digitalRead(resetB) == LOW) {
    digitalWrite(redLed, LED_ON);
    digitalWrite(greenLed, LED_ON);
    digitalWrite(yellowLed, LED_ON);
    Serial.println(F("Wipe button pressed"));
    Serial.println(F("You have 10 seconds to CANCEL"));
    Serial.println(F("This action will remove all records and cannot be undone"));
    delay(10000);
    if (digitalRead(resetB) == LOW) {
      Serial.println(F("Starting wipe"));
      for (uint8_t x = 0; x < EEPROM.length(); x++) {
        if (EEPROM.read(x) == 0) {} // Skipping cleared memory 
        else EEPROM.write(x , 0);
      }
      Serial.println(F("EEPROM successfully wiped"));

      // Visualizing wipe with LED blinks
      blinkAll();
      blinkAll();
      blinkAll();
    }
  } else {
    Serial.println(F("Canceled wipe"));
    digitalWrite(redLed, LED_OFF);
    digitalWrite(greenLed, LED_OFF);
    digitalWrite(yellowLed, LED_OFF);
  }

  // Confirming master card
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No master card defined"));
    Serial.println(F("Please scan the MASTER card"));
    do {
      match = getID(); // successRead set to 1 when scanner reads card
      digitalWrite(greenLed, LED_ON);
      digitalWrite(yellowLed, LED_ON);
      delay(200);
      digitalWrite(greenLed, LED_OFF);
      digitalWrite(yellowLed, LED_OFF);
    } while (!successRead);
    for (uint8_t i = 0; i < 4; i++) {
      EEPROM.write(2 + i, readCard[i]);
    }
    EEPROM.write(1, 143); // Conformation of Master Card in EEPROM
    Serial.println(F("Master card defined"));
  }
  for (uint8_t i = 0; i < 4; i++)
    masterCard[i] = EEPROM.read(2 + i); // Write to master card variable
    
}

void loop() {
  do {
    match = getID();
    if (digitalRead(resetB) == LOW) {
      digitalWrite(redLed, LED_ON);
      digitalWrite(greenLed, LED_OFF);
      digitalWrite(yellowLed, LED_OFF);

      //
      Serial.println(F("Reset button pressed!"));
      Serial.println(F("10 seconds until Master will be erased."));
      delay(10000);
      if (digitalRead(resetB) == LOW) {
        EEPROM.write(1, 0);
        Serial.println(F("RESTART"));
        while(1);
      }
    }
    if (programming) cycle();
    else readLed();
  } while (!match);
  
  if (programming) {
    if (checkTwo(readCard , masterCard)) {
      Serial.println(F("Master scanned | Exiting programming mode"));
      programming = false;
      return;
    } else {
      if (findID(readCard)) {
        // If scanned card is known, delete
        Serial.println(F("Known card, removing..."));
        deleteID(readCard);
        Serial.println(F("Scan a card to ADD or REMOVE to memory"));
      } else {
        Serial.println(F("Unknown card, adding..."));
        writeID(readCard);
        Serial.println(F("Scan a card to ADD or REMOVE to memory"));
      }
    }
  }
  else {
    if (checkTwo(readCard, masterCard)) {
      // Check if scanned card's ID matches Master card's ID
      programming = true;
      Serial.println(F("MASTER - Entered programming mode"));
      uint8_t count = EEPROM.read(0); // Read amt of IDs in EEPROM
      Serial.print(F("There are "));
      Serial.print(count);
      Serial.println(F(" record(s) on EEPROM"));
      Serial.println(F("Scan a card to ADD or REMOVE to memory"));
      Serial.println(F("Scan MASTER again to exit programming"));
    } else {
      if (findID(readCard)) {
        Serial.println(F("Access granted"));
        granted(300);
      } else {
        Serial.println(F("Access denied"));
        denied();
      }
    }
  }
}



// LED HELPER //

void blinkAll() {
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(yellowLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  digitalWrite(greenLed, LED_ON);
  digitalWrite(yellowLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(yellowLed, LED_OFF);
}

void cycle() {
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
}

void readLed() {
  digitalWrite(yellowLed, LED_ON);  // Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF);  // Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure Green LED is off
  lock.write(180);  // Make sure door is locked
}

// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
}

// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
}

// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(yellowLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(yellowLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(yellowLed, LED_ON);  // Make sure blue LED is on
  delay(200);
}

// ////////// //

void granted (uint8_t setDelay) {
  digitalWrite(yellowLed, LED_OFF);   // Turn off blue LED
  digitalWrite(redLed, LED_OFF);  // Turn off red LED
  digitalWrite(greenLed, LED_ON);   // Turn on green LED
  lock.write(0);      // Unlock door
  delay(setDelay);          // Hold door lock open for given seconds
  lock.write(180);    // Relock door
  delay(1000);            // Hold green LED on for a second
}

void denied() {
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_ON);   // Turn on red LED
  delay(1000);
}

void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
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
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
    digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
    digitalWrite(redLed, LED_ON);   // Turn on red LED
    while (true); // do not go further
  }
}

uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

// Check bytes
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

