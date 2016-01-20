#include <SoftwareSerial.h> 
#include <SparkFunESP8266WiFi.h>

#define mySSID "TODO:YOUR_NETWORK_NAME_HERE"
#define myPSK  "TODO:YOUR_PASSOWRD_HERE"

#define destServer "tudelft.cloud.thingworx.com"

//ThingWorx App key which replaces login credentials
#define appKey "TODO:YOUR_APPKEY_HERE"

// ThingWorx Thing name for which you want to set properties values
// TODO: change this to your Thing's name
#define thingName "SST_ReadPotentiometer"

// ThingWorx service that will set values for the properties you need
// TODO: change this to your Service's name
#define serviceName "SST_setPotValue"

// Name of the variable used in Thingworx
// TODO: change this to your Property's name
#define propertyName "potMeterValue"

// The maximum length we expect for a replied JSON string.
#define REPLY_MAX_LENGTH 150

//Interval of time at which you want the properties values to be sent to TWX server [ms]
#define timeBetweenMessages 500

// TODO: change this to the pin you are using
#define potPin A0

ESP8266Client client;
String httpRequest;
bool displayResponse = true; // this controls whether we see the HTTP response
bool displayRequest = true;  // this controls whether we print the request we send

// Initialised to -1 to be able to see whether we read any values from the sensor
int potValue = -1;



void setup(){
  Serial.begin(9600);

  // initializeESP8266() verifies communication with the WiFi shield, and sets it up.
  initializeESP8266();

  // connectESP8266() connects to the defined WiFi network.
  connectESP8266();

  // displayConnectInfo prints the Shield's local IP and the network it's connected to.
  displayConnectInfo();
  
  pinMode(potPin, INPUT);
  
}

void loop() 
{

	potValue = analogRead(potPin);
	Serial.print(F("potValue = "));
	Serial.println(potValue);
	
	// now make a request to post this to the server.
	httpRequest = "POST /Thingworx/Things/";
	httpRequest+= thingName;
	httpRequest+= "/Services/";
	httpRequest+= serviceName;
	httpRequest+= "?appKey=";
	httpRequest+= appKey;
	httpRequest+= "&method=post&x-thingworx-session=true";
	httpRequest+= "<&";
	httpRequest+= propertyName;
	httpRequest+= "=";
	httpRequest+= potValue;
	httpRequest+= "> HTTP/1.1\r\n";
	httpRequest+= "Host: ";
	httpRequest+= destServer;
	httpRequest+= "\r\n";
	httpRequest+= "Content-Type: text/html\r\n\r\n";
	
	if(displayRequest) {
		Serial.println(F("Sending request:\n"));
		Serial.println(httpRequest);
	}else{
		Serial.println(F("Sending data to ThingWorx."));
	}
	
	char* reply = sendRequest();
	if(displayResponse){
		Serial.println(reply);
	}
	
	delay(timeBetweenMessages);
}

// Sends a HTTP request based on the contents of a variable called httpRequest.
// Originally written by SparkFun, adapted by Adrie Kooijman and Marien Wolthuis
char* sendRequest()
{
	ESP8266Client client;
	// ESP8266Client connect([server], [port], [keepAlive]) is used to connect to a server (const char * or IPAddress) on a specified port, and keep the connection alive for the specified amount of time.
	// Returns: 1 on success, 2 on already connected, negative on fail (-1=TIMEOUT, -3=FAIL).
	int retVal = client.connect(destServer, 80, 100);
	if (retVal <= 0)
	{
		Serial.print(F("Failed to connect to server."));
		Serial.print(F("retVal: "));
		Serial.println(retVal);
		return "";
	}

	// Send data to a connected client connection.
	client.print(httpRequest);

	// available() will return the number of characters currently in the receive buffer.
	// The code below filters out all HTML and only saves what might be a JSON string
	// This cannot deal with nested JSON arrays!
	char reply[REPLY_MAX_LENGTH] = "";
	int i = 0;
	while (client.available()){
		if(displayResponse){
		  // This outputs the entire response to Serial and saves none of it
			Serial.write(client.read()); // read() gets the FIFO char
		}else{
			char rep = client.read();
			if(rep == '{'){                 // JSON strings always start with this
				while (client.available() && rep != '}'){
					reply[i] = rep; 
					rep = client.read();
					i++;
				}
				reply[i] = rep; // to get the closing bracket too
			}
		}
	}
	
	// connected() is a boolean return value; 1 if the connection is active, 0 if it's closed.
	if (client.connected()){
		client.stop(); // stop() closes a TCP connection.
	}
	
	return reply;
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
	Serial.println(F("This is usually fixed by resetting the ESP8266 (ground the RST pin for a second), then resetting the Arduino."));
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
