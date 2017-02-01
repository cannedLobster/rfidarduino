# rfidarduino
Arduino code for MFRC522 RFID reader connected through SPI. Uses motor, LCD, LED, and push button in conjunction to create a simple programmable security system.


Push button to enable user to designate MASTER card. MASTER will allow user to designate specific cards access through the system. All data will be saved on the Arduino EEPROM which is non-volatile. Motor will turn 180 degrees when access is granted. 

The MFRC552 is provably weak and has already been exploited. Paper can be found here : https://eprint.iacr.org/2008/166
