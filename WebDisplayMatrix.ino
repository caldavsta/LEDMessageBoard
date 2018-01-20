#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <TimeLib.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <vector>
#include <string>
#include <Metro.h> 
#include <SPI.h>

using namespace std;
#define    MAX_DEVICES    8
#define    MAX_ZONES    2
#define    CLK_PIN        14
#define    DATA_PIN    13
#define    CS_PIN        15
#define CONNECTED_PIN 5

MD_Parola P = MD_Parola(CS_PIN, MAX_DEVICES);


/*
						Declare Time 
*/
static const char ntpServerName[] = "time.nist.gov";//a
const int timeZone = -8; // Central European Time //a
WiFiUDP Udp;//a
unsigned int localPort = 8888;//a
time_t getNtpTime();//a
void sendNTPpacket(IPAddress &address); //a
char numstr_last[6];
time_t t;
time_t prevDisplay = 0; //a
Metro timeMetro = Metro(7200000); //2 hours

/*
						Declare Message Stuff
*/
const string LASTLINEBEFOREMESSAGES = "Content-Type"; //The line that gets detected before reading messages. should be some part of the HTTP response
vector <string> messageList;
const char* hostMessageServer = "onagainapps.com"; //message server credentials
const char* urlMessageServer = "/messageboard/messages.txt";

/*
						Declare Weather stuff
*/

const char host[] = "openweathermap.org";
String data = "";
Metro weatherMetro(1800000); //30 minutes
StaticJsonBuffer<200> jsonBuffer;


struct WeatherData
{
	float temp;
	char conditions[32];
};


/*
						Declare Display Stuff
*/

#define ZONE_TIME 1
#define ZONE_MESSAGE 0


const short int ANIMATION_DELAY = 20; // in milliseconds
char zone0Message[256] = "Message"; // message zone string
char zone1Message[6] = "88:88"; // time zone string
String zone0String;



#define ANIMATION_TIMEOUT 20000 //milliseconds 
vector<string> getMessagesFromServer();
vector<string> getResponseFromHost(const char* host, const char* url);
void performMessageOperations();

void convertUrlToAscii(char data[]);
WiFiClient client;


void setup()
{	
	delay(1000);
	t = now();
	Serial.begin(115200);
	Serial.write("STARTING");
	pinMode(CONNECTED_PIN, OUTPUT);

	/*
				SETUP LED MATRIX
	*/
	P.begin(2);

	delay(50);
	P.setZone(0, 0, 4); // message scroller
	P.setZone(1, 5, 7); // time
	P.setTextBuffer(0, zone0Message);
	P.setTextBuffer(1, zone1Message);
	//P.setFont(ZONE_MESSAGE, font);
	P.setIntensity(0, 1);
	P.setIntensity(1, 3);
	P.setSpeed(ZONE_TIME, 20);
	P.setPause(0, 0);
	P.setTextEffect(0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
	P.setTextEffect(1, PA_WIPE_CURSOR, PA_NO_EFFECT);
	P.setTextAlignment(ZONE_MESSAGE, PA_CENTER);
	P.setTextAlignment(ZONE_TIME, PA_CENTER);



	delay(50);
	/*
				SETUP WIFI
	*/
	WiFiManager wifiManager;
	wifiManager.autoConnect("Matrix8x64", "password");
	while (WiFi.status() != WL_CONNECTED) {
		delay(250);
		Serial.print(".");
	}

/*
			  SETUP TIME
*/
	Serial.print("local ip:");
	Serial.println(WiFi.localIP());
	Serial.println("Starting UDP");
	Udp.begin(localPort);
	Serial.print("Local port: ");
	Serial.println(Udp.localPort());
	Serial.println("waiting for sync");
	setSyncProvider(getNtpTime);
	setSyncInterval(300);


	/*
				SETUP WEATHER
	*/
	

	delay(50);
}

void loop()
{
	//if its time to, get the messages from server
	if (timeMetro.check() == 1)
	{
		performTimeKeepingOperations();
	}

	if (weatherMetro.check() == 1)
	{
		//getWeatherData();
	}

	/*
		while (Serial.available()) {

		zone0String = Serial.readString();// read the incoming data as string

		Serial.println(zone0String);
		strcpy(zone0Message, zone0String.c_str());
		convertUrlToAscii(zone0Message);
		P.displayReset(ZONE_MESSAGE);


	}
	*/


	performMessageOperations();
	//system_soft_wdt_feed(); //disabled because its underlined in red
	P.displayAnimate();
	
}

void getWeatherData()
{
	httpRequest();
	JsonObject& root = jsonBuffer.parse(client);



}



void httpRequest() {
	// close any connection before send a new request.
	// This will free the socket on the WiFi shield
	client.stop();
	delay(100);

	Serial.print("connecting to ");
	Serial.println(host);

	// Use WiFiClient class to create TCP connections
	const int httpPort = 80;
	if (!client.connect(host, httpPort)) {
		Serial.println("connection failed");
		return;
	}

	Serial.print("Requesting URL: ");
	// This will send the request to the server
	client.print(String("GET ") + "/data/2.5/weather?id=5812944&units=imperial&appid=e8e5a79d31772266504ee1f0f748842e" + " HTTP/1.1\r\n");
	client.print("Host: api.openweathermap.org\r\n");
	client.print("Connection: close\r\n\r\n");

	delay(10);
}

void performMessageOperations()
{
	static int currentMessageNumber = 0;

	if (P.getZoneStatus(ZONE_MESSAGE))// || millis() - LAST_MILLISECONDS > ANIMATION_TIMEOUT)//if animation complete or ANIMATION_TIMEOUT ms have passed
	{
		if (messageList.empty() || currentMessageNumber == messageList.size()-1) //if reached end of list OR no list
		{
			//Serial.println("Reached end of messageList.");
			getMessagesFromServer();
			currentMessageNumber = 0;
		} else
		{
			//Serial.print("This messages first letter is ");
			//Serial.println (messageList[currentMessageNumber][0]);
			currentMessageNumber++;
		}
		//Serial.println("Setting message");
		//strcpy(zone0Message, static_cast<string>(messageList[currentMessageNumber]).c_str());

		strcpy(zone0Message, messageList[currentMessageNumber].substr(1, 255).c_str());

		if (messageList[currentMessageNumber].size() < 9)
		{
			P.setPause(ZONE_MESSAGE, 350);
			P.setSpeed(ZONE_MESSAGE, 15);
			P.setTextEffect(ZONE_MESSAGE, PA_SCROLL_DOWN, PA_SCROLL_DOWN);
		} else
		{
			P.setPause(ZONE_MESSAGE, 0);
			P.setSpeed(ZONE_MESSAGE, 23);
			P.setTextEffect(ZONE_MESSAGE, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
		}
		P.displayReset(ZONE_MESSAGE);

	}
}

void convertUrlToAscii(char data[])
{
	// Create two pointers that point to the start of the data
	char *leader = data;
	char *follower = leader;

	// While we're not at the end of the string (current character not NULL)
	while (*leader) {
		// Check to see if the current character is a %
		if (*leader == '%') {

			// Grab the next two characters and move leader forwards
			leader++;
			char high = *leader;
			leader++;
			char low = *leader;

			// Convert ASCII 0-9A-F to a value 0-15
			if (high > 0x39) high -= 7;
			high &= 0x0f;

			// Same again for the low byte:
			if (low > 0x39) low -= 7;
			low &= 0x0f;

			// Combine the two into a single byte and store in follower:
			*follower = (high << 4) | low;
		}
		else {
			// All other characters copy verbatim
			*follower = *leader;
		}

		// Move both pointers to the next character:
		leader++;
		follower++;
	}
	// Terminate the new string with a NULL character to trim it off
	*follower = 0;
}

vector<string> getMessagesFromServer()
{
	messageList.clear();
	Serial.println("Fetching messages from server...");
	bool readingMessages = false;
	vector<string> response = getResponseFromHost(hostMessageServer, urlMessageServer);

	for (string currentLine : response) { // loop through each line of response
		Serial.println(currentLine.c_str());
		if (!readingMessages && currentLine.find(LASTLINEBEFOREMESSAGES) != string::npos)
		{
			readingMessages = true;
		} else
		{
			if (readingMessages) //read releavant strings into messageList
			{
				if (currentLine.length() > 1) // make sure line isn't blank
				{
					messageList.push_back(currentLine);
				}
			}
		}
	}

	Serial.print(messageList.size());
	Serial.println(" messages found from server");
	return response;
}

vector<string> getResponseFromHost(const char* host, const char* url)
{
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	vector<string> response;

	//try to connect
	if (!client.connect(host, httpPort)) {
		Serial.println("connection failed");
		return response;
	}

	//send request to server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + host + "\r\n" +
		"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			Serial.println(">>> Client Timeout !");
			client.stop();
			return response;
		}
	}

	//prepare for the response

	digitalWrite(CONNECTED_PIN, HIGH); // turn on the light
	while (client.available())
	{
		string thisLine = client.readStringUntil('\r').c_str();
		response.push_back(thisLine);
	}
	digitalWrite(CONNECTED_PIN, LOW); // turn off the light

	if (timeStatus() != timeNotSet) {
		if (now() != prevDisplay) { //update the display only if time has changed
			prevDisplay = now();
			performTimeKeepingOperations();
		}
	} else
	{

	}

	return response;
}

void performTimeKeepingOperations()
{
	static int prevHour, prevMinute;
	static int nowHour, nowMinute;
	nowHour = hour();
	nowMinute = minute();
	static int formattedHr;

	if (nowHour != prevHour || nowMinute != prevMinute)
	{ //update time display
		if (nowHour == 0)
		{
			formattedHr = 12;
		} else if (nowHour > 12)
		{
			formattedHr = nowHour - 12;
		} else
		{
			formattedHr = nowHour;
		}
		sprintf(zone1Message, "%i:%02i", formattedHr, minute());
		

		Serial.println(zone1Message);
		P.displayReset(ZONE_TIME);
		
		prevHour = nowHour;
		prevMinute = nowMinute;
	}
}



/*
 *                     NTP TIME STUFF
*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
	IPAddress ntpServerIP; // NTP server's ip address

	while (Udp.parsePacket() > 0); // discard any previously received packets
	Serial.println("Transmit NTP Request");
	// get a random server from the pool
	WiFi.hostByName(ntpServerName, ntpServerIP);
	Serial.print(ntpServerName);
	Serial.print(": ");
	Serial.println(ntpServerIP);
	sendNTPpacket(ntpServerIP);
	uint32_t beginWait = millis();
	while (millis() - beginWait < 1500) {
		int size = Udp.parsePacket();
		if (size >= NTP_PACKET_SIZE) {
			Serial.println("Receive NTP Response");
			Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
			unsigned long secsSince1900;
			// convert four bytes starting at location 40 to a long integer
			secsSince1900 = (unsigned long)packetBuffer[40] << 24;
			secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
			secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
			secsSince1900 |= (unsigned long)packetBuffer[43];
			return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
		}
	}
	Serial.println("No NTP Response :-(");
	return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
							 // 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer, NTP_PACKET_SIZE);
	Udp.endPacket();
}
