#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#define DEBUG


/*
 * TODO - What needs to be done:
 * - detect if break between showing cards is too long
 * - open lock
 * - LED for status 
 * - lock after success
 * - currently hard-coded to for two cards
 * - save battery
 */


/*
 * Card owner
 */
#define NONE 0
#define NA 1
#define FE 2


/*
 * Timing
 */
#define SHORT false
#define LONG true
#define LONG_THRESHOLD 300


/*
 * Pattern
 */
const byte PATTERN_LEN = 3;
const byte ORDER[PATTERN_LEN] = { FE, FE, FE };
const bool TIMING[PATTERN_LEN] = { SHORT, LONG, SHORT };


/*
 * Cards
 */
const byte CARD_FE[4] = { 153, 53, 212, 179 };
const byte CARD_NA[4] = { 121, 38, 223, 162 };


/* 
 * Pins 
 */
#define SERVO_PIN 6
#define RFID_SDA_PIN 10
#define RFID_RST_PIN 9


MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);
MFRC522::MIFARE_Key key; 

Servo lockServo;

byte currentPos = 0;
byte currentCard = NONE;
unsigned long currentCardTime;



void setup() { 
  Serial.begin(9600);
  while (!Serial); // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  lockServo.attach(SERVO_PIN);

  SPI.begin();
  
  rfid.PCD_Init(); 

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}



void loop() {

  bool shown = waitForCardChange();
  if(shown) {
    /* 
     * card was shown, read owner and check order
     */

    // if you swipe to fast, we might have missed the remove action
    if (currentCard != NONE) {
      handleBadPattern("Missed remove");
      return;  
    }
    
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      Serial.println(F("Your tag is not of type MIFARE Classic."));
      Serial.println(rfid.PICC_GetTypeName(piccType));
      showSystemError();
      return;
    }
  
    #ifdef DEBUG
    Serial.print(F("UID tag is:"));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("Current pos: "));
    Serial.println(currentPos);
    #endif

    // determine card owner
    if (isCard(rfid.uid.uidByte, CARD_NA)) {
      currentCard = NA;
    } else if (isCard(rfid.uid.uidByte, CARD_FE)) {
      currentCard = FE;
    } else {
      handleBadPattern("Unknown card");
      return;
    }
    
    // check pattern order
    if(ORDER[currentPos] != currentCard) {
      handleBadPattern("Wrong pattern order");
      #ifdef DEBUG
      Serial.print(F("Expected: "));
      Serial.print(ORDER[currentPos]);
      Serial.print(F(" but was: "));
      Serial.println(currentCard);
      #endif
      return;
    }

    // store time stamp
    currentCardTime = millis();
    
  } else {
    /* 
     * card removed, check pattern timing 
     */

    // if you swipe to fast, we might have missed the show action
    if (currentCard == NONE) {
      handleBadPattern("Missed add");
      return;  
    }
    
    unsigned long duration = millis() - currentCardTime;
    bool longSwipe = (duration > LONG_THRESHOLD);

    if (TIMING[currentPos] == longSwipe) {
      currentPos++;
    } else {
      handleBadPattern("Wrong pattern timing");
      #ifdef DEBUG
      Serial.print(F("Expected: "));
      Serial.print(TIMING[currentPos]);
      Serial.print(F(" but was: "));
      Serial.println(longSwipe);
      #endif
      return;
    }

    currentCard = NONE;
    currentCardTime = 0;
 
  }
  
  if(currentPos == PATTERN_LEN) {
    #ifdef DEBUG
    Serial.println(F("Pattern matched, unlock"));
    #endif
    // TODO open lock
  }

  /*
  delay(100);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  */
}

bool waitForCardChange () {
  bool noCard = false;
  bool hasCard = false;
  int noCounter = 0;
  int hasCounter = 0;
  bool current, previous;
  
  previous = rfid.PICC_IsNewCardPresent();

  while(!noCard || !hasCard){
    delay(50);
    current = rfid.PICC_IsNewCardPresent();

    if (!current && !previous) noCounter++;
    if (current || previous) hasCounter++;

    previous = current;

    noCard = (noCounter > 2);
    hasCard = (hasCounter > 0);   
  }

  bool cardReadable = (rfid.PICC_ReadCardSerial());
  
  #ifdef DEBUG
  if(cardReadable) {
    Serial.println(F("\nCard shown\n"));
  } else {
    Serial.println(F("\nCard removed\n"));
  }
  #endif
  
  return cardReadable;
}

bool isCard(byte *actual, const byte *expected) {
  if (actual[0] == expected[0] &&
    actual[1] == expected[1] &&
    actual[2] == expected[2] && 
    actual[3] == expected[3] ) {
    return true; 
  } else {
    return false;
  }
}

void showGoodCard() {
  #ifdef DEBUG
  Serial.println(F("goodCard"));
  #endif
  // TODO blink LED
}

void handleBadPattern(const char *reason) {
  currentPos = 0;
  currentCardTime = 0;
  currentCard = NONE;
  #ifdef DEBUG
  Serial.print(F("handleBadPattern: "));
  Serial.println(reason);
  #endif
  // TODO blink LED
}

void showSystemError() {
  #ifdef DEBUG
  Serial.println(F("showSystemError"));
  #endif
  // TODO blink LED
}

void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
