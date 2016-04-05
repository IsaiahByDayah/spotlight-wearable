/*********************************************************************
 This is an example for our nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <ArduinoJson.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"



/*
 * NeoPixel Stuff
 */
 
/*=========================================================================
    APPLICATION SETTINGS

    FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
   
                              Enabling this will put your Bluefruit LE module
                              in a 'known good' state and clear any config
                              data set in previous sketches or projects, so
                              running this at least once is a good idea.
   
                              When deploying your project, however, you will
                              want to disable factory reset by setting this
                              value to 0.  If you are making changes to your
                              Bluefruit LE device via AT commands, and those
                              changes aren't persisting across resets, this
                              is the reason why.  Factory reset will erase
                              the non-volatile memory where config data is
                              stored, setting it back to factory default
                              values.
       
                              Some sketches that require you to bond to a
                              central device (HID mouse, keyboard, etc.)
                              won't work at all with this feature enabled
                              since the factory reset will clear all of the
                              bonding data stored on the chip, meaning the
                              central device won't be able to reconnect.
    MINIMUM_FIRMWARE_VERSION  Minimum firmware version to have some new features
    MODE_LED_BEHAVIOUR        LED activity, valid options are
                              "DISABLE" or "MODE" or "BLEUART" or
                              "HWUART"  or "SPI"  or "MANUAL"
    -----------------------------------------------------------------------*/
    #define FACTORYRESET_ENABLE         1
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
    #define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

// Create the bluefruit object, either software serial...uncomment these lines
/*
SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);

Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN,
                      BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
*/

/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
// Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}





/*
 * NeoPixel Stuff
 */
#define NeoPixel_PIN 6

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, NeoPixel_PIN, NEO_GRB + NEO_KHZ800);




/*
 * GLOBAL VARIABLES
 */

char messageTerminatorChar = '~';
String messageTerminatorString = "~";

String userID = "abc123";
String inputBuffer;
int signalStrength = 0;
int prevSignalStrength = 0;



/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit Command <-> Data Mode Example"));
  Serial.println(F("------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code


  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  proximityLignt(2);

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set module to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));
}

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/
void loop(void)
{
  // Check for user input
  char n, inputs[BUFSIZE+1];

  // For sending straight messages across
  if (Serial.available())
  {
    n = Serial.readBytes(inputs, BUFSIZE);
    inputs[n] = 0;
    // Send characters to Bluefruit
    Serial.print("Sending: ");
    Serial.println(inputs);

    // Send input data to host via Bluefruit
    ble.print(inputs);
  }

  // Check for any new messages
  bool msgFinished = false;
  retrieveMsg(msgFinished);

  if (msgFinished){
    handleMessage();
  }

  // Update LED ring
  updateLEDRing();

  // MARK: Check for single click and call sendLike()

  // MARK: Check for double click and call sendDismiss()
}

// Parse message
void handleMessage() {
  Serial.print("Message Recieved: ");
  // Do Stuff

  // Grab input
  String msg;
  msg.concat(inputBuffer);

  // reset input buffer
  inputBuffer = "";

  Serial.println(msg);

//    String toPi = "This is a really long string that is being sent from the arduino to the the pi.";
//    sendMessage(toPi);

  String testJsonToPi = "{\"Test\":\"Hello World\"}";
  sendMessage(testJsonToPi);

  DynamicJsonBuffer jsonBuffer;
  JsonObject& msgRoot = jsonBuffer.parseObject(msg);

  const char* msgTypeChar = msgRoot["msgType"];
  String msgType = String(msgTypeChar);

  if (msgType == "UserID") {
    const char* requestChar = msgRoot["request"];
    String request = String(requestChar);
    if (request == "GET") {
      sendUserID();
    }
    else if (request == "SET") {
      const char* newUserID = msgRoot["userID"];
      
      userID = String(newUserID);
    }
    else {
      // Unknow UserID Request  
    }
  }
  else if (msgType == "SignalStrength") {
    signalStrength = (int) msgRoot["signalStrengthValue"];
  }
  else {
    // Unknown msgType recieved  
  }
}

void updateLEDRing(){
  // TODO: Update signalstrength value / Color to LED Ring
  Serial.print("Signal: ");
  Serial.println(signalStrength);
  proximityLignt(signalStrength);
}

void retrieveMsg(bool& msgFinished) {

  while (ble.available() > 0) {
    
    char c = (char)ble.read();

//    Serial.print("Char recieved: ");
//    Serial.println(c);

    if (c == '~') {
      msgFinished = true;

//      Serial.print("Final msg: ");
//      Serial.println(inputBuffer);
      
      return;  
    }
    else {
      inputBuffer.concat(c);
    
//      Serial.print("Current msg: ");
//      Serial.println(inputBuffer);
    }
  }
}

void sendMessage(String& msg) {
  if (! msg.endsWith("~")) {
    msg.concat("~");
  }

  ble.print(msg);
}

void sendUserID() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& userIDMessageRoot = jsonBuffer.createObject();
  userIDMessageRoot["msgType"] = "UserID";
  userIDMessageRoot["userID"] = userID;
  
  // Build & send String message

  String userIDMessage;
  userIDMessageRoot.printTo(userIDMessage);

  sendMessage(userIDMessage);  
}

void sendLike() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& likeMessageRoot = jsonBuffer.createObject();
  likeMessageRoot["msgType"] = "Like";
  
  // Build & send String message

  String likeMessage;
  likeMessageRoot.printTo(likeMessage);

  sendMessage(likeMessage);  
}

void sendDismiss() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& dismissMessageRoot = jsonBuffer.createObject();
  dismissMessageRoot["msgType"] = "Dismiss";
  
  // Build & send String message

  String dismissMessage;
  dismissMessageRoot.printTo(dismissMessage);

  sendMessage(dismissMessage);  
}

void proximityLignt(int prox){
  if(prox==1){
     colorWipe(strip.Color(67, 0, 0), 20); // Red
  }
  if(prox==2){
    colorWipe(strip.Color( 70, 0, 56),20); // Purple
  }
  if(prox==3){
    colorWipe(strip.Color(0, 0, 67), 20); // Blue
  }

  }
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}
