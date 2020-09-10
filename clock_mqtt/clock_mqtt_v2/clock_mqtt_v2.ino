#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h> 

/* ----------------------------------------------SETTINGS---------------------------------------------- */
static const char* const      mqtt_server                    = "192.168.1.56";
static const uint16_t         mqtt_port                      = 1883;
static const char* const      static_ip                      = "192.168.1.53";
static const char* const      static_gw                      = "192.168.1.1";
static const char* const      static_sn                      = "255.255.255.0";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_username                  = "datel";
static const char* const      mqtt_key                       = "hanka12";
static const char* const      mqtt_base                      = "Clock";
static const char* const      mqtt_request_topic             = "request_Clock";


#define STARTUP_BRIGHTNESS 30	// Brightness that the clock is using after startup (0-255)

#define PIN D4                // Pin of the led strip, default 2 (that is D4 on the wemos)

// uncomment the line below to enable the startup animation
#define STARTUP_ANIMATION

// uncomment the line below to enable logging into serial
#define DEBUG_SERIAL 

#define AUTOCONNECTNAME   HOSTNAMEOTA
#define AUTOCONNECTPWD    "password"


#define ota
#ifdef ota
#include <ArduinoOTA.h>
#define HOSTNAMEOTA   "ClockBig"
#endif


#ifdef DEBUG_SERIAL
  #define DEBUG_PRINT(x)         Serial.print(x)
  #define DEBUG_PRINTDEC(x)      Serial.print(x, DEC)
  #define DEBUG_PRINTLN(x)       Serial.println(x)
  #define DEBUG_PRINTHEX(x)      Serial.print(x, HEX)
  #define DEBUG_PRINTF(x,y)      Serial.printf(x, y)
  #define PORTSPEED 115200
  #define SERIAL_BEGIN           Serial.begin(PORTSPEED);
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
#endif 


uint32_t heartBeat                          = 0;

/* ----------------------------------------------SETTINGS END---------------------------------------------- */

//for LED status
#include <Ticker.h>
Ticker ticker;

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}



/* ----------------------------------------------GLOBALS---------------------------------------------- */
WiFiClient espClient;
PubSubClient client(espClient);

int mqttdata = 0;	// indicates if a new message is available

// clock time variables
int hours = 0;
int mins = 0;
int secs = 0;
unsigned long timemillis = 0;

// Timer variables
int T_hours = 0;
int T_mins = 0;
int T_secs = 0;
unsigned long timermillis;
unsigned long weathermillis;
unsigned long alarmmillis;

int weathersecs = 5;	// indicates how long to display the temperature
int pt1 = 0;
int pt2 = 0;


int alarmstate = 0;
int alarmcounter = 0;


// State of the dots
int dot1 = 1;
int dot2 = 1;

int newData = 0;
const byte numChars = 50;
char receivedChars[numChars]; // an array to store the received data

// Numbers for the digits
int d1 = 99;
int d2 = 99;
int d3 = 99;
int d4 = 99;

// Color of the digits
int cd[] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };

// Color of the alarm digits
int fadespeed = 0;
int fadeamount = 0;
int ad = 0;
int ar = 255;
int ag = 255;
int ab = 255;

// values of custom mode
int cv1 = 0;
int cv2 = 0;
int cv3 = 0;
int cv4 = 0;

// Colors of the dots
int cdo[] = { 255, 255, 255, 255, 255, 255 };

int wr = 255;
int wg = 255;
int wb = 255;

char type = '0';  // current mode
char mymode2 = '0';	// second mode, (fade)
char old_type = '0';  // current mode

char vals[14][6] = { "", "", "", "", "", "", "", "", "", "", "", "", "", "" };
/* ----------------------------------------------GLOBALS END---------------------------------------------- */


/* ----------------------------------------------LED STRIP CONFIG---------------------------------------------- */
#define NUMPIXELS 30
//offsets
#define Digit1 0
#define Digit2 7
#define Digit3 16
#define Digit4 23

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
/* ----------------------------------------------LED STRIP CONFIG END---------------------------------------------- */


/* ----------------------------------------------FUNCTIONS START---------------------------------------------- */


/*
 * Function: ExtractValues
 * Used to extract a given amount of values from the message with a start index
 * Parameters:
 * - startindex: position in the string where to start
 * - valuecount: amount of values to capture
 */
void ExtractValues(int startindex, int valuecount) {
	int pos = startindex;
	for (int c = 0; c < valuecount; c++) {
		int i = 0;
		while (receivedChars[pos] != ';' && receivedChars[pos] != '\0') {
			vals[c][i] = receivedChars[pos];
			pos++;
			i++;
		}
		vals[c][i] = '\0';
		pos++;
	}
	for (int p = 0; p < valuecount; p++) {
		DEBUG_PRINT("Extracting: "); DEBUG_PRINTLN(vals[p]);
	}
}

/*
 * Function: CallMode
 * Function calls the corresponding function to the type character
 * Called when a new message arrives
 * Parameters: mymode: character
 */
void CallMode(char mymode) {
	type = mymode;
	if (mymode == '!') Alarm();        // !
	else if (mymode == '0') {
    Off();          // 0
  } else if (mymode == '#') {
    mymode2 = '0'; type = old_type; 
  }	else if (mymode == 'd') {
    SetDots(receivedChars[2] - '0', receivedChars[4] - '0');      // d;1;0
	} else if (mymode == 'i') {
    Set1Dot(receivedChars[2] - '0');      // i;1
	} else if (mymode == 'a') {
    SetMode(receivedChars[2]);      // a;c
	} else if (mymode == 'w') {
    weathersecs = (int)(receivedChars[8] - '0') * 10 + (int)(receivedChars[10] - '0');
    Weather(receivedChars[2] - '0', receivedChars[4] - '0', receivedChars[6]);      // w;1;9;+;1;5
  }	else if (mymode == 'z') {
    CustomValues(receivedChars[2] - '0', receivedChars[4] - '0', receivedChars[6] - '0', receivedChars[8] - '0'); // z;0;2;0;4
	} else if (mymode == 'x')	{
		ExtractValues(2, 3);
		SetGeneralColor(atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
	}	else if (mymode == 's') {     // s;2;2;5;8;2;2
		int h = 10 * (receivedChars[2] - '0') + (receivedChars[4] - '0');
		int m = 10 * (receivedChars[6] - '0') + (receivedChars[8] - '0');
		int s = 10 * (receivedChars[10] - '0') + (receivedChars[12] - '0');
		SetTime(h, m, s);
	}	else if (mymode == 'e')	{	// e;255;0;0;0;255;0
		ExtractValues(2, 6);
		SetDotColors(atoi(vals[0]), atoi(vals[1]), atoi(vals[2]), atoi(vals[3]), atoi(vals[4]), atoi(vals[5]));
	}	else if (mymode == 'h') { // h;1;255;0;0 - nastavi barvu tecek
		ExtractValues(4, 3);
		Set1DotColor(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
	}	else if (mymode == 'f') { // f;1;8;255;34 - nastavi barvu cislic
		ExtractValues(4, 3);
		Set1Color(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
	} else if (mymode == 'g') {   // g;255;10;3;100;255;200;255;10;3;100;255;200
		ExtractValues(2, 12);
		SetColors(atoi(vals[0]), atoi(vals[1]), atoi(vals[2]), atoi(vals[3]), atoi(vals[4]), atoi(vals[5]), atoi(vals[6]), atoi(vals[7]), atoi(vals[8]), atoi(vals[9]), atoi(vals[10]), atoi(vals[11]));
	}	else if (mymode == 't')	{			  // t;01;15;30
		int timer_h = (receivedChars[2] - '0') * 10 + receivedChars[3] - '0';
		int timer_m = (receivedChars[5] - '0') * 10 + receivedChars[6] - '0';
		int timer_s = (receivedChars[8] - '0') * 10 + receivedChars[9] - '0';
		SetTimerDyn(timer_h, timer_m, timer_s);
	}	else if (mymode == 'b')	{	//b;80
		ExtractValues(2, 1);
		SetBrightness(atoi(vals[0]));
	} else if (mymode == '*')	{
		mymode2 = '*';
		SetFadeSpeed(receivedChars[2] - '0');
	}	else if (mymode == '*' || mymode == '#' || mymode == '0' || mymode == '!')ResetAlarmLeds();
	else 	{
		DEBUG_PRINT("Mode did not match: "); DEBUG_PRINTLN(mymode);
		type = old_type;
	}
	pixels.show();
}




/*
 * Function: SetGeneralColor
 * Used to set the color of all digits and dots
 * Called when mode is "x"
 * Parameters:
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 */
void SetGeneralColor(int r, int g, int b) {
#ifdef DEBUG_SERIAL
	Serial.printf("Setting colors for all leds: r:%d, g:%d, b:%d\n", r, g, b);
#endif
	cd[0] = r;	cd[1] = g;	cd[2] = b;	cd[3] = r;	cd[4] = g;	cd[5] = b;	cd[6] = r;	cd[7] = g;	cd[8] = b;	cd[9] = r;	cd[10] = g;	cd[11] = b;
	cdo[0] = r;	cdo[1] = g;	cdo[2] = b;	cdo[3] = r;	cdo[4] = g;	cdo[5] = b;
	DrawDots();
	if (old_type == 'c') DrawTime();
	else if (old_type == 't') DrawTimer();
	else if (old_type == 'z')CustomValues(cv1, cv2, cv3, cv4);
	if (old_type != 'w' && old_type != '0')DrawDots();
	pixels.show();
	type = old_type;
	DEBUG_PRINTF("Mode is now: %c\n", type);
}


/*
 * Function: serialNew
 * Used to process new data
 * Called when a new message arrives
 * Parameters: none
 */
void serialNew() {
	displayData();
	GetMode();
	CallMode(receivedChars[0]);
	newData = false;
	mqttdata = 0;
}

/*
 * Function: displayData
 * Prints the message into serial console for debugging
 * Called when a new message arrives
 * Parameters: none
 */
void displayData()
{
	if (newData == true) {
		DEBUG_PRINT("This just in ... ");
		DEBUG_PRINTLN(receivedChars);
	}
}

/*
 * Function: GetMode
 * Extracts the type from the first character of the message
 * Called when a new message arrives
 * Parameters: none
 */
void GetMode()
{
	old_type = type;
	if (old_type == '0' || old_type == '!') pixels.setBrightness(40);
	type = receivedChars[0];
}

/*
 * Function: ResetAlarmLeds
 * Resets alarm status to initial values
 * Called when alarm ends
 * Parameters: none
 */
void ResetAlarmLeds()
{
	ad = 0;
	ar = 255;
	ag = 255;
	ab = 255;
}

/*
   Function: Weather
   Extracts temperature readings and display duration from the message,
   afterwards it draws the temperature symbol(°C) and the temperature
   Temperatures are in CELCIUS
   Called when a type is 'w'
   Parameters:
   - t1: first digit of the temperature
   - t2: second digit of the temperature
   - sign: either "+" or "-"
*/
void Weather(int t1, int t2, char sign) {
  //int pt1 = t1;
  //int pt2 = t2;
  int temp = t1 * 10 + t2;
  if (sign == '-') temp = temp * -1;
  // if (sign == '-')t2 = t2 * (-1);
#ifdef DEBUG_SERIAL
  Serial.printf("Weather: sign: %c, t1:%d, t2:%d, time:%ds\n", sign, t1, t2, weathersecs);
#endif
  for (int i = 0; i < NUMPIXELS; i++)pixels.setPixelColor(i, pixels.Color(0, 0, 0));

  GetWeatherColor(temp);

  // °
  pixels.setPixelColor(Digit4 + 0, pixels.Color(wr, wb, wg));
  pixels.setPixelColor(Digit4 + 1, pixels.Color(wr, wb, wg));
  pixels.setPixelColor(Digit4 + 2, pixels.Color(wr, wb, wg));
  pixels.setPixelColor(Digit4 + 3, pixels.Color(wr, wb, wg));

  weathermillis = millis();

  if (t1==0) {
    if (sign == '-') {
      pixels.setPixelColor(Digit2 + 0, pixels.Color(wr, wb, wg));
    }
    DrawDigit(Digit3, wr, wg, wb, t2);
  } else {
    if (sign == '-') {
      pixels.setPixelColor(Digit1 + 0, pixels.Color(wr, wb, wg));
    }
    DrawDigit(Digit2, wr, wg, wb, t1);
    DrawDigit(Digit3, wr, wg, wb, t2);
  }

  // //WeatherSymbols
  // if (sign != '-') {
    // // °
    // pixels.setPixelColor(Digit3 + 0, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 1, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 2, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 3, pixels.Color(61, 225, 255));
  // } else {
    // // - 
    // pixels.setPixelColor(Digit1 + 0, pixels.Color(61, 225, 255));
  // }

  // // C
  // pixels.setPixelColor(Digit4 + 2, pixels.Color(61, 225, 255));
  // pixels.setPixelColor(Digit4 + 3, pixels.Color(61, 225, 255));
  // pixels.setPixelColor(Digit4 + 4, pixels.Color(61, 225, 255));
  // pixels.setPixelColor(Digit4 + 5, pixels.Color(61, 225, 255));

  // weathermillis = millis();
  // GetWeatherColor(temp);
  // // if (sign == '-' && t1 == '0') {
    // // pixels.setPixelColor(Digit3 + 0, pixels.Color(61, 225, 255));
    // // pixels.setPixelColor(Digit3 + 1, pixels.Color(61, 225, 255));
  // // }
  // if (t1 != 0) {
    // if (sign != '-') {
      // if (mymode2 == '*') {
        // DrawDigit(Digit1, ar, ag, ab, t1);
      // } else {
        // DrawDigit(Digit1, wr, wg, wb, t1);
      // }
      // if (mymode2 == '*') {
        // DrawDigit(Digit2, ar, ag, ab, t2);
      // } else DrawDigit(Digit2, wr, wg, wb, t2);
    // } else {
      // if (mymode2 == '*') {
        // DrawDigit(Digit2, ar, ag, ab, t1);
      // } else {
        // DrawDigit(Digit2, wr, wg, wb, t1);
      // }
      // if (mymode2 == '*') {
        // DrawDigit(Digit3, ar, ag, ab, t2);
      // } else {
        // DrawDigit(Digit3, wr, wg, wb, t2);
      // }
    // }
  // } else {
    // pixels.setPixelColor(Digit3 + 0, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 1, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 2, pixels.Color(61, 225, 255));
    // pixels.setPixelColor(Digit3 + 3, pixels.Color(61, 225, 255));
    // if (sign != '-') {
      // if (mymode2 == '*') {
        // DrawDigit(Digit2, ar, ag, ab, t2);
      // } else {
        // DrawDigit(Digit2, wr, wg, wb, t2);
      // }
    // } else {
      // if (mymode2 == '*') {
        // DrawDigit(Digit2, ar, ag, ab, t2);
      // } else {
        // DrawDigit(Digit2, wr, wg, wb, t2);
      // }
    // }
  // }

  pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
  pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
  //dots off
	SetDots(0, 0);
  
  pixels.show();
  DEBUG_PRINTF("Showing the weather for %d seconds\n", weathersecs);
}

/*
 * Function: GetWeatherColor
 * Used to get temperature color, high temperature = more yellow, low temperature = more blue
 * Sets the result into the global variables: wr, wg, wb
 * Called by function Weather
 * Parameters: temperature in °C
 */
void GetWeatherColor(int temp) {
	int R = 255; int G = 255; int B = 255;
	String hex = "";
	if (temp < -40) {
		R = 0; G = 197;	B = 229;
	}	else if (temp < -30) {
		R = 28;	G = 203; B = 204;
	}	else if (temp < -20) {
		R = 56;	G = 209; B = 180;
	}	else if (temp < -10) {
		R = 84;	G = 216; B = 156;
	}	else if (temp < 0) {
		R = 112; G = 222; B = 131;
	}	else if (temp < 4) {
		R = 140; G = 229; B = 107;
	}	else if (temp < 8) {
		R = 168; G = 235; B = 83;
	}	else if (temp < 14) {
		R = 196; G = 242; B = 58;
	}	else if (temp < 18) {
		R = 224; G = 248; B = 34;
	}	else if (temp < 22) {
		R = 253, G = 244, B = 10;
	}	else if (temp < 26) {
		R = 253, G = 233, B = 10;
	}	else if (temp < 30) {
		R = 254, G = 142, B = 10;
	}	else if (temp < 34) {
		R = 254, G = 105, B = 10;
	}	else if (temp > 40) {
		R = 255, G = 68, B = 10;
	}
	wr = R; wg = G; wb = B;
}

/*
 * Function: Off
 * Turns every pixel off and sets brightness to 0
 * Called when mode is '0'
 * Parameters: none
 */
void Off()
{
	DEBUG_PRINTLN("Clock is turning off");

	for (int i = 0; i < NUMPIXELS; i++)
	{
		pixels.setPixelColor(i, pixels.Color(0, 0, 0));
	}
	pixels.setBrightness(0);
	pixels.show();
}

/*
 * Function: Set1Dot
 * Used to set the color of the dot
 * Called when a new message arrives
 * Parameters: int dotcode:
 *   - 0 == Dot1 off
 *   - 1 == Dot1 on
 *   - 2 == Dot2 off
 *   - 3 == Dot2 on
 */
void Set1Dot(int dotcode) {
	if (dotcode == 0) {
		pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
		dot1 = 0;
	}
	if (dotcode == 1) {
		pixels.setPixelColor(Digit3 - 1, pixels.Color(cdo[3], cdo[4], cdo[5]));
		dot1 = 1;
	}
	if (dotcode == 2) {
		pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
		dot2 = 0;
	}
	if (dotcode == 3) {
		pixels.setPixelColor(Digit3 - 2, pixels.Color(cdo[3], cdo[4], cdo[5]));
		dot2 = 1;
	}
	DrawDots();
	pixels.show();
	//type = old_type;
}


/*
 * Function: SetTimerDyn
 * Used to read time of the message and start the timer
 * Called when mode is "t"
 * Parameters:
 * - h: hours (0-23)
 * - m: minutes (0-59)
 * - s: seconds (0-59)
 */
void SetTimerDyn(int h, int m, int s)
{
	T_hours = h;
	T_mins = m;
	T_secs = s;
#ifdef DEBUG_SERIAL
	Serial.printf("Timer Started: %d:%d:%d\n", T_hours, T_mins, T_secs);
#endif
	DrawTimer();
	timermillis = millis();
	type = 't';
	pixels.show();
}


/*
 * Function: SetMode
 * Used to set the mode manually and to print the mode into serial
 * Called when mode is "a"
 * Parameters: type: any character that matches a mode, e.g.: "c", "!"
 */
void SetMode(char newtype)
{
	old_type = newtype;
	type = newtype;
	DEBUG_PRINTF("Mode updated: %c\n", type);
	if (type == '0') Off();
	if (type == 'c') {
		if (mymode2 != '*')DrawTime();
		if (mymode2 != '*')DrawDots();
	}
}

/*
 * Function: SetFadeSpeed
 * Used to determine the fade amount and the fade delay
 * Called when mode is "*"
 * Parameters: fadevalue: final fade level, value 0-9
 * Note: untilize a switch statement in the next update instead of 10x if()
 */
void SetFadeSpeed(int fadevalue) {
	if (fadevalue == 0) {
		fadeamount = 1;
		fadespeed = 1000;
	}
	else if (fadevalue == 1) {
		fadeamount = 1;
		fadespeed = 500;
	}
	else if (fadevalue == 2) {
		fadeamount = 1;
		fadespeed = 250;
	}
	else if (fadevalue == 3) {
		fadeamount = 1;
		fadespeed = 100;
	}
	else if (fadevalue == 4) {
		fadeamount = 1;
		fadespeed = 25;
	}
	else if (fadevalue == 5) {
		fadeamount = 2;
		fadespeed = 25;
	}
	else if (fadevalue == 6) {
		fadeamount = 2;
		fadespeed = 0;
	}
	else if (fadevalue == 7) {
		fadeamount = 3;
		fadespeed = 0;
	}
	else if (fadevalue == 8) {
		fadeamount = 4;
		fadespeed = 0;
	}
	else if (fadevalue == 9) {
		fadeamount = 12;
		fadespeed = 0;
	}
	else
	{
		fadeamount = 1;
		fadespeed = 1;
	}
	DEBUG_PRINT("Fadelevel is set to: "); DEBUG_PRINTLN(fadevalue);

}


/*
 * Function: SetDots
 * Used to read dot values and to draw the dots
 * Called when mode is "d"
 * Parameters: state of the dots, 0 = off, 1 = on
 */
void SetDots(int mydot1, int mydot2) {
	dot1 = mydot1;
	dot2 = mydot2;
	DrawDots();
	pixels.show();
	type = old_type;
}

/*
 * Function: SetDotColors
 * Used to set both dot colors
 * Called when mode is "e"
 * Parameters: none
 */
void SetDotColors(int r1, int g1, int b1, int r2, int g2, int b2) {
	cdo[0] = r1;
	cdo[1] = g1;
	cdo[2] = b1;
	cdo[3] = r2;
	cdo[4] = g2;
	cdo[5] = b2;
	DrawDots();
	if (type == 'c') DrawTime();
	else if (type == 't') DrawTimer();
	else if (type == 'z')CustomValues(cv1, cv2, cv3, cv4);
	pixels.show();
	type = old_type;
}

/*
 * Function: DrawDots
 * Used update the dots, the function sets the dots according to the "dot1" and "dot2" global variable
 * Called when dots state or color gets changed
 * Parameters: none
 */
void DrawDots() {
	if (dot1)pixels.setPixelColor(Digit3 - 1, pixels.Color(cdo[0], cdo[1], cdo[2]));
	else pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
	if (dot2)pixels.setPixelColor(Digit3 - 2, pixels.Color(cdo[3], cdo[4], cdo[5]));
	else pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
}


/*
 * Function: Set1DotColor
 * Used to set the color of a single dot
 * Called when mode is "h"
 * Parameters:
 * - dotnr: determines the dot, 1 or 2
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 */
void Set1DotColor(int dotnr, int r, int g, int b) {
	if (dotnr == 1)	{
		cdo[0] = r;
		cdo[1] = g;
		cdo[2] = b;
	}
	if (dotnr == 2)	{
		cdo[3] = r;
		cdo[4] = g;
		cdo[5] = b;
	}
	Serial.printf("Setting Dot Color of %d", dotnr);
  DEBUG_PRINTLN();
	DrawDots();
	if (type == 'c') DrawTime();
	else if (type == 't') DrawTimer();
	else if (type == 'z') CustomValues(cv1, cv2, cv3, cv4);
	type = old_type;
	pixels.show();
}

/*
 * Function: Set1DotColor
 * Used to set the color of a single dot
 * Called when mode is "h"
 * Parameters:
 * - mydigit: determines the digit (0-4), a "0" selects all digits
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 */
void Set1Color(int mydigit, int r, int g, int b) {
#ifdef DEBUG_SERIAL
	Serial.printf("Setting digit: %d, r:%d g:%d b:%d\n", mydigit, r, g, b);
#endif
	if (mydigit == 1 || mydigit == 0) {
		cd[0] = r;
		cd[1] = g;
		cd[2] = b;
	};
	if (mydigit == 2 || mydigit == 0) {
		cd[3] = r;
		cd[4] = g;
		cd[5] = b;
	};
	if (mydigit == 3 || mydigit == 0) {
		cd[6] = r;
		cd[7] = g;
		cd[8] = b;
	};
	if (mydigit == 4 || mydigit == 0) {
		cd[9] = r;
		cd[10] = g;
		cd[11] = b;
	};

	if (old_type == 'c') DrawTime();
	else if (old_type == 't') DrawTimer();
	else if (old_type == 'z')CustomValues(cv1, cv2, cv3, cv4);
	DrawDots();
	type = old_type;
	pixels.show();
}

/*
 * Function: SetColors
 * Used to set individual colors for all digits
 * Called when mode is "g"
 * Parameters: none
 * - r1, g1, b1: rgb components for digit nr. 1
 * - r2, g2, b2: rgb components for digit nr. 2
 * - r3, g3, b3: rgb components for digit nr. 3
 * - r4, g4, b4: rgb components for digit nr. 4
 */
void SetColors(int r1, int g1, int b1, int r2, int g2, int b2, int r3, int g3, int b3, int r4, int g4, int b4)
{
	cd[0] = r1;
	cd[1] = g1;
	cd[2] = b1;
	cd[3] = r2;
	cd[4] = g2;
	cd[5] = b2;
	cd[6] = r3;
	cd[7] = g3;
	cd[8] = b3;
	cd[9] = r4;
	cd[10] = g4;
	cd[11] = b4;

#ifdef DEBUG_SERIAL
	Serial.printf("Setting Colors: r:%d g:%d b:%d r:%d g:%d b:%d r:%d g:%d b:%d r:%d g:%d b:%d  \n", cd[0], cd[1], cd[2], cd[3], cd[4], cd[5], cd[6], cd[7], cd[8], cd[9], cd[10], cd[11]);
#endif

	type = old_type;
	if (type == 'c') DrawTime();
	else if (type == 't') DrawTimer();
	else if (type == 'z')CustomValues(cv1, cv2, cv3, cv4);
	DrawDots();
	pixels.show();
}

/*
 * Function: SetBrightness
 * Used to set the brightness of the clock
 * Called when mode is "b"
 * Parameters: brightness (0-255)
 */
void SetBrightness(int brightness) {
	Serial.printf("Setting brightness to %d%%\n", brightness);
	pixels.setBrightness(brightness);
	type = old_type;
	//if (mymode2 != '*')DrawDots();
	pixels.show();
	if (type == 'c' && mymode2 != '*') DrawTime();
	else if (type == 't' && mymode2 != '*') DrawTimer();
}

/*
 * Function: ModeClock
 * Updates time if the time difference to the last update exceeds 60 seconds
 * Called in the loop
 * Parameters: none
 */
void ModeClock() {
	if ((millis() - timemillis) >= 1000) {
		unsigned long minidiff = millis() - timemillis - 1000;
    //DEBUG_PRINTLN(minidiff);
		if (minidiff < 200) {
      timemillis += 1000 + minidiff;
		} else {
      timemillis += 1000;
    }
    
		secs++;
		if (secs >= 60) {
			secs = 0;
			mins++;
			if (mins >= 60) {
				mins = 0;
				hours++;
			}
			if (hours >= 24) hours = 0;
			if (type == 'c') {
				if (mymode2 != '*') DrawTime();
				if (mymode2 != '*') DrawDots();
				pixels.show();
			}
		}
	}
  // if (type == 'c') {
    // if (secs%2 == 0) {
      // SetDots(1,1);
    // } else {
      // SetDots(0,0);
    // }
  // }
}

/*
 * Function: ModeTimerDyn
 * Updates the timer if the time difference to the last update exceeds 1 second
 * Called in the loop if mode is "t"
 * Parameters: none
 */
void ModeTimerDyn()
{
	if (T_hours <= 0 && T_mins <= 0 && T_secs <= 0) type = '1';
	else {
		if ((millis() - timermillis) >= 1000)	{
			unsigned long minidiff = millis() - timermillis - 1000;
			if (minidiff < 200)timermillis += 1000 + minidiff;
			else timermillis += 1000;
			T_secs--;
			if (T_secs < 0) {
				if (T_mins > 0 || T_hours > 0)T_secs = 59;
				T_mins--;
				if (T_mins <= 0) {
					if (T_hours > 0) {
						T_mins = 59;
						T_hours--;
					}
				}
			}
			DrawTimer();
		}
	}
}


/*
 * Function: ModeWeather
 * If the weather display duration is reached, then the mode will be set to clock
 * Called in the loop if mode is "w"
 * Parameters: none
 */
void ModeWeather() {
	if (mymode2 == '*') {
		DrawDigit(Digit1, ar, ag, ab, pt1);
		DrawDigit(Digit2, ar, ag, ab, pt2);
	}
	if (((millis() - weathermillis) / 1000) > weathersecs) {
		DEBUG_PRINTLN("Setting mode back to Clock");
    SetDots(1,1);
		type = 'c';
		if (mymode2 != '*')DrawTime();
		if (mymode2 != '*')DrawDots();
		pixels.show();
	}
}

/*
 * Function: TimerAlarm
 * Flashes all digits on/off and fades the colors
 * Called when the timer runs out (mode is = "1")
 * Parameters: none
 */
void TimerAlarm() {
	if (alarmstate)	{
		if ((millis() - timermillis) >= 350) {
			DEBUG_PRINTLN("ALARM!");
			timermillis = millis();
			alarmstate = 0; pixels.setBrightness(0); pixels.show();
		}
	}	else {
		if ((millis() - timermillis) >= 100) {
			DEBUG_PRINTLN("ALARM!");
			timermillis = millis();
			alarmstate = 1; pixels.setBrightness(255);
			ShiftAlarmLeds(51);
			DrawDigit(Digit1, ar, ag, ab, 0);
			DrawDigit(Digit2, ar, ag, ab, 0);

			pixels.setPixelColor(Digit3 - 1, pixels.Color(ar, ag, ab));
			pixels.setPixelColor(Digit3 - 2, pixels.Color(ar, ag, ab));

			DrawDigit(Digit3, ar, ag, ab, 0);
			DrawDigit(Digit4, ar, ag, ab, 0);
			pixels.show();
		}
	}

}

/*
 * Function: Alarm
 * Flashes all digits on/off and fades the colors
 * Called when mode is "!"
 * Parameters: none
 */
void Alarm(){
	if (alarmstate)	{
		if ((millis() - timermillis) >= 350) {
			DEBUG_PRINTLN("ALARM!");

			timermillis = millis();
			alarmstate = 0; pixels.setBrightness(0); pixels.show();
		}
	} else {
		if ((millis() - timermillis) >= 100) {
			DEBUG_PRINTLN("ALARM!");

			timermillis = millis();
			alarmstate = 1; pixels.setBrightness(255);
			ShiftAlarmLeds(51);

			DrawDigit(Digit1, ar, ag, ab, hours / 10); //Draw the first digit of the hour
			DrawDigit(Digit2, ar, ag, ab, hours - ((hours / 10) * 10)); //Draw the second digit of the hour

			DrawDigit(Digit3, ar, ag, ab, mins / 10); //Draw the first digit of the minute
			DrawDigit(Digit4, ar, ag, ab, mins - ((mins / 10) * 10)); //Draw the second digit of the minute
			pixels.show();
		}
	}
}

/*
 * Function: ModeFade
 * Fades the colors
 * Called when mode2 is "*"
 * Parameters: none
 */
void ModeFade()
{
	if ((millis() - timermillis) >= fadespeed)
	{
		//Serial.printf("fade: d=%d r=%d g=%d b=%d\n", ad, ar, ag, ab);

		timermillis = millis();
		alarmstate = 1;
		ShiftAlarmLeds(fadeamount);

		if (type != 'w')
		{
			DrawDigit(Digit1, ar, ag, ab, hours / 10); //Draw the first digit of the hour
			DrawDigit(Digit2, ar, ag, ab, hours - ((hours / 10) * 10)); //Draw the second digit of the hour

			DrawDigit(Digit3, ar, ag, ab, mins / 10); //Draw the first digit of the minute
			DrawDigit(Digit4, ar, ag, ab, mins - ((mins / 10) * 10)); //Draw the second digit of the minute

			if (dot1)pixels.setPixelColor(Digit3 - 1, pixels.Color(ar, ag, ab));
			else pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
			if (dot2)pixels.setPixelColor(Digit3 - 2, pixels.Color(ar, ag, ab));
			else pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
		}
		else

			pixels.show();
	}
}


/*
 * Function: DrawTimer
 * Displays remaining time in HH:MM or MM:SS depending on time
 * Called in loop when mode is "w"
 * Parameters: none
 */
void DrawTimer() {
#ifdef DEBUG_SERIAL
	Serial.printf("Updating Timer: %d:%d:%d\n", T_hours, T_mins, T_secs);
#endif
	if (T_hours > 0) {
		DrawDigit(Digit1, cd[0], cd[1], cd[2], T_hours / 10);
		DrawDigit(Digit2, cd[3], cd[4], cd[5], T_hours - ((T_hours / 10) * 10));

		DrawDigit(Digit3, cd[6], cd[7], cd[8], T_mins / 10);
		DrawDigit(Digit4, cd[9], cd[10], cd[11], T_mins - ((T_mins / 10) * 10));
	} else {
		DrawDigit(Digit1, cd[0], cd[1], cd[2], T_mins / 10);
		DrawDigit(Digit2, cd[3], cd[4], cd[5], T_mins - ((T_mins / 10) * 10));

		DrawDigit(Digit3, cd[6], cd[7], cd[8], T_secs / 10);
		DrawDigit(Digit4, cd[9], cd[10], cd[11], T_secs - ((T_secs / 10) * 10));
	}
}

/*
 * Function: CustomValues
 * Displays custom values on the clock, values greater than 9 will cause the digit to be disabled
 * Called when mode is "z"
 * Parameters:
 * - d1: value to display on the digit nr. 1
 * - d2: value to display on the digit nr. 2
 * - d3: value to display on the digit nr. 3
 * - d4: value to display on the digit nr. 4
 */
void CustomValues(int d1, int d2, int d3, int d4)
{
	DEBUG_PRINT("Setting custom values!");
	cv1 = d1; cv2 = d2; cv3 = d3; cv4 = d4;
	if (d1 < 10) DrawDigit(Digit1, cd[0], cd[1], cd[2], d1); //Draw the first digit of the hour
	else DisableDigit(1);
	if (d2 < 10) DrawDigit(Digit2, cd[3], cd[4], cd[5], d2); //Draw the second digit of the hour
	else DisableDigit(2);

	if (d3 < 10) DrawDigit(Digit3, cd[6], cd[7], cd[8], d3); //Draw the first digit of the minute
	else DisableDigit(3);
	if (d4 < 10) DrawDigit(Digit4, cd[9], cd[10], cd[11], d4); //Draw the second digit of the minute
	else DisableDigit(4);
	pixels.show();
	type = 'z';
}

/*
 * Function: DrawTime
 * Displays current time
 * Called in loop when mode is "w"
 * Parameters: none
 */
void DrawTime() {
#ifdef DEBUG_SERIAL
	Serial.printf("Updating Time: %d:%d:%d\n", hours, mins, secs);
#endif
	DrawDigit(Digit1, cd[0], cd[1], cd[2], hours / 10); //Draw the first digit of the hour
	DrawDigit(Digit2, cd[3], cd[4], cd[5], hours - ((hours / 10) * 10)); //Draw the second digit of the hour

	DrawDigit(Digit3, cd[6], cd[7], cd[8], mins / 10); //Draw the first digit of the minute
	DrawDigit(Digit4, cd[9], cd[10], cd[11], mins - ((mins / 10) * 10)); //Draw the second digit of the minute
}

/*
 * Function: SetTime
 * Updates local time
 * Called in loop when mode is "s"
 * Parameters:
 * - h: hours (0-23)
 * - m: minutes (0-59)
 * - s: seconds (0-59)
 */
void SetTime(int h, int m, int s)
{
	timemillis = millis();
	DEBUG_PRINT("\nSetting the time: ");

	hours = h;
	mins = m;
	secs = s;

	DEBUG_PRINT(hours);
	DEBUG_PRINT(":");
	DEBUG_PRINT(mins);
	DEBUG_PRINT(":");
	DEBUG_PRINTLN(secs);

	if (old_type == 'c')DrawTime();
	type = old_type;
}

/*
 * Function: DisableDigit
 * Displays remaining time in HH:MM or MM:SS depending on time
 * Parameters: position on the clock (1-4)
 */
void DisableDigit(int mydigit)
{
	if (mydigit == 1)
	{
		for (int i = 0; i < 14; i++)
		{
			pixels.setPixelColor(Digit1 + i, pixels.Color(0, 0, 0));
		}
	}
	if (mydigit == 2)
	{
		for (int i = 0; i < 14; i++)
		{
			pixels.setPixelColor(Digit2 + i, pixels.Color(0, 0, 0));
		}
	}
	if (mydigit == 3)
	{
		for (int i = 0; i < 14; i++)
		{
			pixels.setPixelColor(Digit3 + i, pixels.Color(0, 0, 0));
		}
	}
	if (mydigit == 4)
	{
		for (int i = 0; i < 14; i++)
		{
			pixels.setPixelColor(Digit4 + i, pixels.Color(0, 0, 0));
		}
	}
	pixels.show();
}

/*
 * Function: DrawDigit
 * Lights up segments depending on the value
 * Parameters:
 * - offset: position on the clock (1-4)
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 * - n: value to be drawn (0-9)
 */
void DrawDigit(int offset, int r, int g, int b, int n) {
	if (n == 2 || n == 3 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) //MIDDLE
	{
		pixels.setPixelColor(0 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(0 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 1 || n == 2 || n == 3 || n == 4 || n == 7 || n == 8 || n == 9) //TOP RIGHT
	{
		pixels.setPixelColor(1 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(1 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) //TOP
	{
		pixels.setPixelColor(2 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(2 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) //TOP LEFT
	{
		pixels.setPixelColor(3 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(3 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 2 || n == 6 || n == 8) //BOTTOM LEFT
	{
		pixels.setPixelColor(4 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(4 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 8 || n == 9) //BOTTOM
	{
		pixels.setPixelColor(5 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(5 + offset, pixels.Color(0, 0, 0));
	}
	if (n == 0 || n == 1 || n == 3 || n == 4 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) //BOTTOM RIGHT
	{
		pixels.setPixelColor(6 + offset, pixels.Color(r, g, b));
	}
	else
	{
		pixels.setPixelColor(6 + offset, pixels.Color(0, 0, 0));
	}
	pixels.show();
}

/*
 * Function: ShiftAlarmLeds
 * Shifts rgb values, in a similar manner as HSV/HSL
 * Called by Alarm(), TimerAlarm(), ModeFade()
 * Parameters: amount: speed of the fade effect
 */
void ShiftAlarmLeds(int amount)
{
	if (ad == 0) {
		ag = ag - amount; ab = ab - amount; 
    if (ag <= 0 && ab <= 0)ad++; if (ag < 0) {
			ag = 0;
			ab = 0;
		}
	}
	if (ad == 1) {
		ag = ag + amount; if (ag >= 255)ad++; 
    if (ag > 255)ag = 255;
	}
	if (ad == 2) {
		ar = ar - amount; if (ar <= 0)ad++; 
    if (ar < 0)ar = 0;
	}
	if (ad == 3) {
		ab = ab + amount; if (ab >= 255)ad++; 
    if (ab > 255)ab = 255;
	}
	if (ad == 4) {
		ag = ag - amount; if (ag <= 0)ad++; 
    if (ag < 0)ag = 0;
	}
	if (ad == 5) {
		ar = ar + amount; if (ar >= 255)ad++; 
    if (ar > 255)ar = 255;
	}
	if (ad == 6) {
		ab = ab - amount; if (ab <= 0)ad++; 
    if (ab < 0)ab = 0;
	}
	if (ad == 7) {
		ab = ab + amount; ag = ag + amount; 
    if (ab >= 255)ad = 0; if (ab > 255) {
			ab = 255;
			ag = 255;
		}
	}
}

/*
 * Function: RequestTimeUpdate
 * Request a time update via mqtt
 * Called by Connecting/Reconnecting to the wifi
 * Parameters: none
 */
void RequestTimeUpdate()
{
	DEBUG_PRINTLN("Requesting Time Update!");
	client.publish(mqtt_request_topic, "update", true);
}


/* ----------------------------------------------FUNCTIONS END---------------------------------------------- */

/* ----------------------------------------------COMMUNICATION---------------------------------------------- */
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
      DEBUG_PRINTLN("connected");
			client.subscribe(mqtt_base);
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str());
      RequestTimeUpdate();
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// void reconnect() {
	// while (!client.connected()) {
		// DEBUG_PRINT("Attempting MQTT connection...");
		// if (mqtt_auth == 1)
		// {
			// if (client.connect(device_name, mqtt_user, mqtt_password)) {
				// DEBUG_PRINTLN("connected");
				// client.subscribe(mqtt_topic);
			// }
			// else {
				// DEBUG_PRINT(client.state());
				// DEBUG_PRINTLN(" try again in 5 seconds");
				// DEBUG_PRINT("failed, rc=");
				// delay(5000);
			// }
		// }
		// else
		// {
			// if (client.connect(device_name)) {
				// DEBUG_PRINTLN("connected");
				// client.subscribe(mqtt_topic);
			// }
			// else {
				// DEBUG_PRINT(client.state());
				// DEBUG_PRINTLN(" try again in 5 seconds");
				// DEBUG_PRINT("failed, rc=");
				// delay(5000);
			// }
		// }
	// }
	// RequestTimeUpdate();
// }
/* ----------------------------------------------COMUNICATION END---------------------------------------------- */

/* ----------------------------------------------MQTT CALLBACK---------------------------------------------- */
void callback(char* topic, byte* payload, unsigned int length) {
	DEBUG_PRINT("Message arrived [");
	DEBUG_PRINT(topic);
	DEBUG_PRINT("] ");
	int i = 0;
	for (i = 0; i < length; i++) {
		receivedChars[i] = (char)payload[i];
	}
	receivedChars[i] = '\0';
	Serial.printf(": %s\n", receivedChars);
	mqttdata = 1;
}
/* ----------------------------------------------MQTT CALLBACK END---------------------------------------------- */

/* ----------------------------------------------SETUP---------------------------------------------- */
void setup()
{
  
  //********** CHANGE PIN FUNCTION  TO GPIO **********
//GPIO 1 (TX) swap the pin to a GPIO.
//pinMode(1, FUNCTION_3); 
//GPIO 3 (RX) swap the pin to a GPIO.
 // pinMode(3, FUNCTION_3); 
//**************************************************
  
  SERIAL_BEGIN(115200);
  DEBUG_PRINTLN();
  DEBUG_PRINTLN("Clock is booting up!");
  
  WiFi.printDiag(Serial);
  
  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;
  DEBUG_PRINT("Boot-Mode: ");
  DEBUG_PRINTLN(_reset_reason);
  heartBeat = _reset_reason;
  /*
  REASON_DEFAULT_RST             = 0      normal startup by power on 
  REASON_WDT_RST                 = 1      hardware watch dog reset 
  REASON_EXCEPTION_RST           = 2      exception reset, GPIO status won't change 
  REASON_SOFT_WDT_RST            = 3      software watch dog reset, GPIO status won't change 
  REASON_SOFT_RESTART            = 4      software restart ,system_restart , GPIO status won't change 
  REASON_DEEP_SLEEP_AWAKE        = 5      wake up from deep-sleep 
  REASON_EXT_SYS_RST             = 6      external system reset 
  */
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

	WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
  
  DEBUG_PRINTLN(_ip);
  DEBUG_PRINTLN(_gw);
  DEBUG_PRINTLN(_sn);

  //wifiManager.setConfigPortalTimeout(60); 
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

	DEBUG_PRINTLN("Clock is now ready!");


#ifdef ota
  ArduinoOTA.setHostname(HOSTNAMEOTA);
  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("Start updating ");
  });
  ArduinoOTA.onEnd([]() {
   DEBUG_PRINTLN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
#ifdef DEBUG_SERIAL
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
#ifdef DEBUG_SERIAL
    Serial.printf("Error[%u]: ", error);
#endif
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
#endif

	type = 'c';
	pixels.begin();
  

#ifdef STARTUP_ANIMATION
	// Startup animation
	for (int i = 0; i < NUMPIXELS; i++)	{
		pixels.setPixelColor(i, pixels.Color(255, 0, 0));
		pixels.show();
		delay(10);
	}
	for (int i = 0; i < NUMPIXELS; i++)	{
		pixels.setPixelColor(i, pixels.Color(0, 255, 0));
		pixels.show();
		delay(10);
	}
	for (int i = 0; i < NUMPIXELS; i++)	{
		pixels.setPixelColor(i, pixels.Color(0, 0, 255));
		pixels.show();
		delay(10);
	}
	for (int i = 0; i < NUMPIXELS; i++)	{
		pixels.setPixelColor(i, pixels.Color(255, 255, 255));
		pixels.show();
		delay(10);
	}
	for (int i = 0; i < NUMPIXELS; i++)	{
		pixels.setPixelColor(i, pixels.Color(0, 0, 0));
		pixels.show();
		delay(10);
	}
#endif // STARTUP_ANIMATION

	RequestTimeUpdate();
	SetDots(1, 1);
	SetBrightness(STARTUP_BRIGHTNESS);
	type = 'c';
}
/* ----------------------------------------------SETUP END---------------------------------------------- */



/* ----------------------------------------------LOOP---------------------------------------------- */
void loop()
{
	if (!client.connected()) {
		reconnect();
	}
	client.loop();
  
	ModeClock();
  
	if (mqttdata > 0) serialNew();
	else if (type == 't') ModeTimerDyn();
	else if (type == 'w') ModeWeather();
	else if (type == '1') TimerAlarm();
	else if (type == '!') Alarm();
	if (mymode2 == '*') ModeFade();
  
#ifdef ota
  ArduinoOTA.handle();
#endif

}
/* ----------------------------------------------LOOP END---------------------------------------------- */