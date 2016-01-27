/*************************************************************************
* An Arduino sketch that connects to ThingWorx and receives readouts     *
* from various Mashup graphical interface items.                         *
* Change the TODO: values and destination server where needed.           *
*                                                                        *
* Written by Marien "dotCID" Wolthuis, 27/01/2016.                       *
**************************************************************************/

#include <SoftwareSerial.h> 
#include <SparkFunESP8266WiFi.h>
#include <string.h>


#define mySSID "TODO:YOUR_SSID_HERE"
#define myPSK  "TODO:YOUR_PASSWORD_HERE"

#define destServer "tudelft.cloud.thingworx.com"

//ThingWorx App key which replaces login credentials
#define appKey "TODO:YOUR_APPKEY_HERE"

// ThingWorx Thing name for which you want to set properties values
// TODO: change this to your Thing's name
#define thingName "SST_LedController"

// ThingWorx service that will set values for the properties you need
// TODO: change this to your Service's name
#define serviceName "SST_getData"

// Interval of time at which you want the properties values to be gotten from TWX server [ms]
#define timeBetweenMessages 2500

// The maximum length we expect for a replied JSON string.
#define REPLY_MAX_LENGTH 150

// TODO: change this to the pins you are using.
#define messagePin 4
#define blinkPin 5
#define controlledPin 6

String httpRequest;
unsigned long send_time;    // this holds the timestamp of our last request
bool displayRequest = false;  // this controls whether we print the request we send
bool displayResponse = false; // this controls whether we see the HTTP response
bool displayJSON = true;    // this controls whether we print the JSON that was found

// The variables related to the LEDs
int blinkDelay;
unsigned long blink_on_time;  // this holds the timestamp of when the blinking LED was last turned on
bool messageLEDStatus, controlledLEDStatus, blinkStatus;

void setup(){
  Serial.begin(9600);

  // initializeESP8266() verifies communication with the WiFi shield, and sets it up.
  initializeESP8266();

  // connectESP8266() connects to the defined WiFi network.
  connectESP8266();

  // displayConnectInfo prints the Shield's local IP and the network it's connected to.
  displayConnectInfo();
  
  pinMode(messagePin, OUTPUT);
  pinMode(blinkPin, OUTPUT);
  pinMode(controlledPin, OUTPUT);  
}

void loop()
{
  // we use this if statement to not overflow the server with requests
  if((millis() - send_time) > timeBetweenMessages){
    // To get data from the server, we use a GET request.
    httpRequest = "GET /Thingworx/Things/";
    httpRequest+= thingName;
    httpRequest+= "/Services/";
    httpRequest+= serviceName;
    httpRequest+= "/result";
    httpRequest+= "?appKey=";
    httpRequest+= appKey;
    httpRequest+= "&method=post&x-thingworx-session=true";
    httpRequest+= " HTTP/1.1\r\n";
    httpRequest+= "Host: ";
    httpRequest+= destServer;
    httpRequest+= "\r\n";
    httpRequest+= "Content-Type: text/html\r\n\r\n";

    if(displayRequest) {
      Serial.println(F("\nSending request:\n"));
      Serial.println(httpRequest);
    }else{
      Serial.println(F("\nSending data to ThingWorx."));
    }

    // We are using character arrays to use less memory than Strings
    char *response = sendRequest();
    send_time = millis();

    
    // process the data:
    if(strlen(response) > 1){ // Sometimes data doesn't come through properly. This checks whether we got any
      if(displayJSON){
        Serial.print(F("  Received the following JSON: \n  "));
        Serial.println(response);
      }
      saveToVars(response);
    }else{
      Serial.println(F("  No data received."));
    }
  }
    
  // You will notice how the blinking stops at certain intervals. This is the duration of the HTTP request to the server and getting data back from it.
  
  ledBlink(blinkDelay);
  digitalWrite(controlledPin, controlledLEDStatus);
  digitalWrite(messagePin, messageLEDStatus);
}

// Blinks an LED at certain intervals
// This method avoids using delay() since that stops all other execution
// Written by Marien Wolthuis
void ledBlink(int interval){
  // (de)activate red LED based on how long it's been on
  if(!blinkStatus){
    if((millis() - blink_on_time) > (interval * 2)){ // if it's been off long enough, turn it on
      blink_on_time = millis();
      blinkStatus = HIGH;
    }
  }else{
    if((millis()-blink_on_time) > interval){ // if it's been on long enough, turn it off
      blinkStatus = LOW;
    }
  }
  digitalWrite(blinkPin, blinkStatus);
}

// Sends a HTTP request based on the contents of a variable called httpRequest.
// Originally written by SparkFun, adapted by Adrie Kooijman and Marien Wolthuis
char* sendRequest()
{
  ESP8266Client client;
  // ESP8266Client connect([server], [port], [keepAlive]) is used to connect to a server (const char * or IPAddress) on a specified port, and keep the connection alive for the specified amount of time.
  // Returns: 1 on success, 2 on already connected, negative on fail (-1=TIMEOUT, -2=UNKNOWN, -3=FAIL).
  int retVal = client.connect(destServer, 80, timeBetweenMessages);
  if (retVal <= 0)
  {
    Serial.print(F("  Failed to connect to server."));
    Serial.print(F(" retVal: "));
    Serial.println(retVal);
    switch(retVal){
      case -1:
        Serial.println(F("  The connection timed out."));
        break;
      case -2:
        Serial.println(F("  An unknown error occurred."));
        break;
      case -3:
        Serial.println(F("  The connection failed."));
        break;
    }
    return "";
  }

  // Send data to a connected client connection.
  client.print(httpRequest);
  
  // Clear the httpRequest to free up some memory
  httpRequest = "";
  
  // available() will return the number of characters currently in the receive buffer.
  // The code below filters out all HTML and only saves what might be a JSON string
  // This cannot deal with nested JSON arrays!
  char reply[REPLY_MAX_LENGTH] = "";
  int i = 0;
  while (client.available()){
    char rep = client.read(); // read() gets the FIFO char (-1 if none available)
    if(displayResponse){
      Serial.print(rep);        // This outputs the entire response to Serial
    }
    if(rep == '{'){                 // JSON strings always start with this
      while (client.available() && rep != '}'){
        reply[i] = rep;
        rep = client.read();
        i++;
      }
      reply[i] = rep; // to get the closing bracket too
    }
  }
  
  // connected() is a boolean return value; 1 if the connection is active, 0 if it's closed.
  if (client.connected()){
    client.stop(); // stop() closes a TCP connection.
  }
  
  return reply;
}

// This function saves the variables that are contained in a JSON-like character array to separate variables.
// Please note that this can not deal with variable names or values that contain a space " " 
// character due to this being a custom implementation of a JSON interpreter that is 
// purely aimed at conserving as much memory as possible and to serve only purposes 
// like the one in the exercise at hand.
// Written by Marien Wolthuis
void saveToVars(char *input){
  // Deliminators are characters that are used to separate parts of a phrase or expression
  #define DELIMINATORS "{ ':,}"
  char * pch;

  // We expect this to be our first variable name: messageLEDStatus
  pch = strtok(input,DELIMINATORS);
  // Then this is the value associated with it:
  pch = strtok(NULL, DELIMINATORS);
  
  if(pch[0] == 't'){   // Checks whether the first character in the array is a 't' for 'true'
    messageLEDStatus = true;
  }else if(pch[0] == 'f'){
    messageLEDStatus = false;
  }
  
  // blinkDelay name:
  pch = strtok(NULL, DELIMINATORS);
  // blinkDelay value:
  pch = strtok(NULL, DELIMINATORS);
  // strtok() gives us a character array instead of a numeric value, atoi() converts this back to a proper integer.
  blinkDelay = atoi(pch);
  
  // controlledLEDStatus name:
  pch = strtok(NULL, DELIMINATORS);
  // controlledLEDStatus value:
  pch = strtok(NULL, DELIMINATORS);
  
  if(pch[0] == 't'){
    controlledLEDStatus = true;
  }else if(pch[0] == 'f'){
    controlledLEDStatus = false;
  }
}

// ---- Helper functions for the ESP, written by Sparkfun -------

void initializeESP8266()
{
  // esp8266.begin() verifies that the ESP8266 is operational and sets it up for the rest of the sketch.
  // It returns either true or false -- indicating whether communication was successul or not.
  int test = esp8266.begin();
  if (test != true)
  {
    Serial.println(F("Error talking to ESP8266."));
  // Inserted by Marien Wolthuis; easiest way to fix connection issues
  Serial.println(F("This is usually fixed by resetting the ESP8266 (ground the RST pin for a second, then wait for the blue LED to light), then resetting the Arduino."));
    errorLoop(test);
  }
  Serial.println(F("ESP8266 Shield Present"));

}

void connectESP8266()
{
  // The ESP8266 can be set to one of three modes:
  //  1 - ESP8266_MODE_STA - Station only
  //  2 - ESP8266_MODE_AP - Access point only
  //  3 - ESP8266_MODE_STAAP - Station/AP combo
  // Use esp8266.getMode() to check which mode it's in:
  int retVal = esp8266.getMode();
  if (retVal != ESP8266_MODE_STA)
  { // If it's not in station mode.
    // Use esp8266.setMode([mode]) to set it to a specified
    // mode.
    retVal = esp8266.setMode(ESP8266_MODE_STA);
    if (retVal < 0)
    {
      Serial.println(F("Error setting mode."));
      errorLoop(retVal);
    }
  }
  Serial.println(F("Mode set to station"));

  // esp8266.status() indicates the ESP8266's WiFi connect status.
  // A return value of 1 indicates the device is already connected.
  //   0 indicates disconnected. (Negative values equate to communication errors.)
  retVal = esp8266.status();
  if (retVal <= 0)
  {
    Serial.print(F("Connecting to "));
    Serial.println(mySSID);
    // esp8266.connect([ssid], [psk]) connects the ESP8266
    // to a network.
    // On success the connect function returns a value >0
    // On fail, the function will either return:
    //  -1: TIMEOUT - The library has a set 30s timeout
  //  -2: UNKNOWN - some unknown error occurred
    //  -3: FAIL - Couldn't connect to network.
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0)
    {
      Serial.println(F("Error connecting"));
      errorLoop(retVal);
    }
  } else {
    Serial.print(F("Already "));
  }
}

void displayConnectInfo()
{
  char connectedSSID[24];
  memset(connectedSSID, 0, 24);
  // esp8266.getAP() can be used to check which AP the ESP8266 is connected to. It returns an error code.
  // The connected AP is returned by reference as a parameter.
  int retVal = esp8266.getAP(connectedSSID);
  if (retVal > 0)
  {
    Serial.print(F("Connected to: "));
    Serial.println(connectedSSID);
  }

  // esp8266.localIP returns an IPAddress variable with the ESP8266's current local IP address.
  IPAddress myIP = esp8266.localIP();
  Serial.print(F("My IP: ")); Serial.println(myIP);
}

// errorLoop prints an error code, then loops forever.
void errorLoop(int error)
{
  Serial.print(F("Error: ")); Serial.println(error);
  Serial.println(F("Looping forever."));
  for (;;)
    ;
}
