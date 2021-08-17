#include "Configuration.h"

auto timer = timer_create_default(); // create a timer with default settings
Timer<> default_timer; // save as above

uint32_t      heartBeat                     = 0;
float         temperature                   = -55;
unsigned char ClockPoint                    = 1;
bool          brightness                    = LOW;
bool          prijemDat                     = false;

const byte numChars = 50;
char receivedChars[numChars]; // an array to store the received data

WiFiUDP EthernetUdp;
IPAddress ntpServerIP                       = IPAddress(195, 113, 144, 201);        //tik.cesnet.cz
//Central European Time (Frankfurt, Paris)
TimeChangeRule        CEST                  = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule        CET                   = {"CET", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);
unsigned int          localPort             = 8888;  // local port to listen for UDP packets
time_t getNtpTime();

WiFiClient espClient;
PubSubClient client(espClient);


DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

//for LED status
Ticker ticker;

void tick() {
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}


ADC_MODE(ADC_VCC);

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
}

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  prijemDat = true;
  char * pEnd;
  String val =  String();
  DEBUG_PRINT("\nMessage arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (int i=0;i<length;i++) {
    DEBUG_PRINT((char)payload[i]);
    val += (char)payload[i];
  }
  DEBUG_PRINTLN();
 
  if (strcmp(topic, mqtt_topic_weather)==0) {
    DEBUG_PRINT("Temperature from Meteo: ");
    DEBUG_PRINTLN(val.toFloat());
    temperature = val.toFloat();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str())==0) {
    Off();
    DEBUG_PRINT("RESTART");
    ESP.restart();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str())==0) {
    DEBUG_PRINT("NET INFO");
    sendNetInfoMQTT();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_load)).c_str())==0) {
    processJson(val);

   	// int i = 0;
    // for (i = 0; i < length; i++) {
      // receivedChars[i] = (char)payload[i];
    // }
    // receivedChars[i] = '\0';
    // CallMode(receivedChars[0]);
  }
  prijemDat = false;
}

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Color of the digits
int cd[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
// Colors of the dots
int cdo[] = {255, 255, 255, 255, 255, 255 };

// State of the dots
int dot1 = 1;
int dot2 = 1;

char vals[14][6] = { "", "", "", "", "", "", "", "", "", "", "", "", "", "" };

char type = 'c';  // clock mode

int Brightness = 0;

int wr = 255;
int wg = 255;
int wb = 255;


WiFiManager wifiManager;

void setup() {
  SERIAL_BEGIN;
  DEBUG_PRINT(F(SW_NAME));
  DEBUG_PRINT(F(" "));
  DEBUG_PRINTLN(F(VERSION));

  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(1, tick);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
  wifiManager.setConnectTimeout(CONNECT_TIMEOUT);

  if (drd.detectDoubleReset()) {
    DEBUG_PRINTLN("Double reset detected, starting config portal...");
    ticker.attach(0.2, tick);
    if (!wifiManager.startConfigPortal(HOSTNAMEOTA)) {
      DEBUG_PRINTLN("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }

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

  WiFi.printDiag(Serial);

  if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }   
  
  sendNetInfoMQTT();
  
#ifdef time
  DEBUG_PRINTLN("Setup TIME");
  EthernetUdp.begin(localPort);
  DEBUG_PRINT("Local port: ");
  DEBUG_PRINTLN(EthernetUdp.localPort());
  DEBUG_PRINTLN("waiting for sync");
  while (getNtpTime()==0) {}
  
  setSyncProvider(getNtpTime);
  setSyncInterval(60);
  
  printSystemTime();
#endif


#ifdef ota
  ArduinoOTA.setHostname(HOSTNAMEOTA);

  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("Start updating ");
  });
  ArduinoOTA.onEnd([]() {
   DEBUG_PRINTLN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
#endif

  IPAddress ip = WiFi.localIP();

  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, HIGH);
  
  pixels.begin();
  startupAnimation();
  type = 'c';
  Brightness = 255;


  timer.every(500, TimingISR);
  timer.every(SENDSTAT_DELAY, sendStatisticMQTT);

  SenderClass sender;
  sender.add(mqtt_topic_request,              "setup");
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  
  void * a;
  sendStatisticMQTT(a);
   
  DEBUG_PRINTLN(" Ready");
 
  drd.stop();
  
  DEBUG_PRINTLN(F("Setup end."));
}


void loop() {
  timer.tick(); // tick the timer
#ifdef ota
  ArduinoOTA.handle();
#endif
  checkWiFiConnection();
  reconnect();
  client.loop();
  
  // if ((hour()>=23 || hour() < 7) && brightness == HIGH) {
    // SetBrightness(1);
    // brightness = LOW;
  // } else {
    // if (brightness == LOW) {
      // SetBrightness(255);
      // brightness = HIGH;
    // }
  // }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("RESTARTING WIFI CONNECTION!!!!!");
    if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
      DEBUG_PRINTLN("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }   
  }
}

bool TimingISR(void *) {
  if (type=='0') {
  } else if ((second()%10)<2) {
    if (Weather()==1) {
      type = 'w';
    }
  } else {
    type = 'c';
    Clock();
  }

  if (type=='c') {
    DrawTime();
    if(ClockPoint) {
      dot1 = 0;
      dot2 = 0;
    } else {
      dot1 = 1;
      dot2 = 1;
    }
    ClockPoint = (~ClockPoint) & 0x01;

    DrawDots();
  }

  return true;
}


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  while (EthernetUdp.parsePacket() > 0) ; // discard any previously received packets
  DEBUG_PRINTLN("Transmit NTP Request");
  // get a random server from the pool
  //WiFi.hostByName(ntpServerName, ntpServerIP);
  DEBUG_PRINTLN(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = EthernetUdp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DEBUG_PRINTLN("Receive NTP Response");
      EthernetUdp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      DEBUG_PRINT("Seconds since Jan 1 1900 = " );
      DEBUG_PRINTLN(secsSince1900);

      // now convert NTP time into everyday time:
      DEBUG_PRINT("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      // print Unix time:
      DEBUG_PRINTLN(epoch);
   
      TimeChangeRule *tcr;
      time_t utc;
      utc = epoch;
      
      return CE.toLocal(utc, &tcr);
      //return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  DEBUG_PRINTLN("No NTP Response :-(");
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
  EthernetUdp.beginPacket(address, 123); //NTP requests are to port 123
  EthernetUdp.write(packetBuffer, NTP_PACKET_SIZE);
  EthernetUdp.endPacket();
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding
  // colon and leading 0
  DEBUG_PRINT(":");
  if(digits < 10)
    DEBUG_PRINT('0');
  DEBUG_PRINT(digits);
}


void printSystemTime(){
  DEBUG_PRINT(day());
  DEBUG_PRINT(".");
  DEBUG_PRINT(month());
  DEBUG_PRINT(".");
  DEBUG_PRINT(year());
  DEBUG_PRINT(" ");
  DEBUG_PRINT(hour());
  printDigits(minute());
  printDigits(second());
  DEBUG_PRINTLN();
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (lastConnectAttempt == 0 || lastConnectAttempt + connectDelay < millis()) {
      DEBUG_PRINT("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
        DEBUG_PRINTLN("connected");
        client.subscribe(String(mqtt_topic_weather).c_str());
        client.subscribe((String(mqtt_base) + "/#").c_str());
      } else {
        lastConnectAttempt = millis();
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINTLN(client.state());
      }
    }
  }
}

bool sendStatisticMQTT(void *) {
  if (!prijemDat) {
    //printSystemTime();
    DEBUG_PRINTLN(F("Statistic"));

    SenderClass sender;
    sender.add("VersionSW",                     VERSION);
    sender.add("Napeti",                        ESP.getVcc());
    sender.add("HeartBeat",                     heartBeat++);
    if (heartBeat % 10 == 0) sender.add("RSSI", WiFi.RSSI());
    
    DEBUG_PRINTLN(F("Calling MQTT"));
    
    sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  }
  return true;
}


void sendNetInfoMQTT() {
  //printSystemTime();
  DEBUG_PRINTLN(F("Net info"));

  SenderClass sender;
  sender.add("IP",                            WiFi.localIP().toString().c_str());
  sender.add("MAC",                           WiFi.macAddress());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  return;
}


void DrawTime() {
#ifdef DEBUG_SERIAL
 DEBUG_PRINTLN("Updating Time: %d:%d:%d\n", hours, mins, secs);
#endif
 DrawDigit(Digit1, cd[0], cd[1], cd[2], hour() / 10); //Draw the first digit of the hour
 DrawDigit(Digit2, cd[3], cd[4], cd[5], hour() - ((hour() / 10) * 10)); //Draw the second digit of the hour

 DrawDigit(Digit3, cd[6], cd[7], cd[8], minute() / 10); //Draw the first digit of the minute
 DrawDigit(Digit4, cd[9], cd[10], cd[11], minute() - ((minute() / 10) * 10)); //Draw the second digit of the minute
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
 if (n == 2 || n == 3 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) { //MIDDLE
  pixels.setPixelColor(0 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(0 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 1 || n == 2 || n == 3 || n == 4 || n == 7 || n == 8 || n == 9) { //TOP RIGHT
  pixels.setPixelColor(1 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(1 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) { //TOP
  pixels.setPixelColor(2 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(2 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) { //TOP LEFT
  pixels.setPixelColor(3 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(3 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 2 || n == 6 || n == 8) { //BOTTOM LEFT
  pixels.setPixelColor(4 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(4 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 8 || n == 9) { //BOTTOM
  pixels.setPixelColor(5 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(5 + offset, pixels.Color(0, 0, 0));
 }
 if (n == 0 || n == 1 || n == 3 || n == 4 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) { //BOTTOM RIGHT
  pixels.setPixelColor(6 + offset, pixels.Color(r, g, b));
 } else {
  pixels.setPixelColor(6 + offset, pixels.Color(0, 0, 0));
 }
 pixels.show();
}


#ifdef STARTUP_ANIMATION
void startupAnimation() {
 // Startup animation
 for (int i = 0; i < NUMPIXELS; i++) {
  pixels.setPixelColor(i, pixels.Color(255, 0, 0));
  pixels.show();
  delay(10);
 }
 for (int i = 0; i < NUMPIXELS; i++) {
  pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  pixels.show();
  delay(10);
 }
 for (int i = 0; i < NUMPIXELS; i++) {
  pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  pixels.show();
  delay(10);
 }
 for (int i = 0; i < NUMPIXELS; i++) {
  pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  pixels.show();
  delay(10);
 }
 for (int i = 0; i < NUMPIXELS; i++) {
  pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  pixels.show();
  delay(10);
 }
}
#endif // STARTUP_ANIMATION


/*
 * Function: Off
 * Turns every pixel off and sets brightness to 0
 * Called when mode is '0'
 * Parameters: none
 */
void Off() {
 DEBUG_PRINTLN("Clock is turning OFF");
  // for (int i = 0; i < NUMPIXELS; i++)  {
    // pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  // }
  pixels.setBrightness(0);
  pixels.show();
}

/*
 * Function: Clock
 * Turns every pixel off and sets brightness to 0
 * Called when mode is '0'
 * Parameters: none
 */
void Clock() {
  //DEBUG_PRINTLN("Clock mode");
  // for (int i = 0; i < NUMPIXELS; i++)  {
    // pixels.setPixelColor(i, pixels.Color(r, g, b));
  // }
  pixels.setBrightness(Brightness);
  pixels.show();
}



/*
 * Function: CallMode
 * Function calls the corresponding function to the type character
 * Called when a new message arrives
 * Parameters: mymode: character
 */
void CallMode(char mymode) {
  if (mymode=='0') { //0
    type = '0';
    Off();
  }
  if (mymode=='c') { //c
    type = 'c';
    Clock();
  }
  if (mymode=='w') { // w
    type = 'w';
    Weather();
  }
  if (mymode=='b') {  //b;80
		ExtractValues(2, 1);
    SetBrightness(atoi(vals[0]));
  }
  if (mymode == 'f') { // f;1;8;255;34
    ExtractValues(4,3);
    Set1Color(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
  }
  if (mymode == 'h') { // h;1;255;0;0
    ExtractValues(4,3);
    Set1DotColor(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
  }
}

/*
 * Function: SetBrightness
 * Used to set the brightness of the clock
 * Parameters: brightness (0-255)
 */
void SetBrightness(int brightness) {
  DEBUG_PRINTF("Setting brightness to %d%\n", brightness);
  pixels.setBrightness(brightness);
  Brightness = brightness;
  pixels.show();
}

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
 * Function: Set1Color
 * Used to set the color of a single digit
 * Parameters:
 * - mydigit: determines the digit (0-4), a "0" selects all digits
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 */
void Set1Color(int mydigit, int r, int g, int b) {
  DEBUG_PRINTF("Setting digit %d to color: r:%d g:%d b:%d\n", mydigit, r, g, b);
  if (mydigit == 1 || mydigit == 0) {
    cd[0] = r;
    cd[1] = g;
    cd[2] = b;
  }
  if (mydigit == 2 || mydigit == 0) {
    cd[3] = r;
    cd[4] = g;
    cd[5] = b;
  }
  if (mydigit == 3 || mydigit == 0) {
    cd[6] = r;
    cd[7] = g;
    cd[8] = b;
  }
  if (mydigit == 4 || mydigit == 0) {
    cd[9] = r;
    cd[10] = g;
    cd[11] = b;
  }
  pixels.show();
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
  DEBUG_PRINTF("Setting dot %d to color: r:%d g:%d b:%d\n", dotnr, r, g, b);
  if (dotnr == 1) {
    cdo[0] = r;
    cdo[1] = g;
    cdo[2] = b;
  }
  if (dotnr == 2) {
    cdo[3] = r;
    cdo[4] = g;
    cdo[5] = b;
  }
  pixels.show();
}


/*
   Function: Weather
   Extracts temperature readings and display duration from the message,
   afterwards it draws the temperature symbol(°C) and the temperature
   Temperatures are in CELCIUS
   Called when a type is 'w'
*/
int Weather() {
  //DEBUG_PRINTLN("Weather mode");
  int temp = (int)round(temperature);
  
  if (temp==-55) {
    //DEBUG_PRINTLN("No temperature from Meteo unit yet");
    return 0;
  }

  for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, pixels.Color(0, 0, 0));

  GetWeatherColor(temp);

  int poziceStupen;
  // °
  if (temp>-10) {
    poziceStupen = Digit3;
  } else {
    poziceStupen = Digit4;
  }
  pixels.setPixelColor(poziceStupen + 0, pixels.Color(wr, wg, wb));
  pixels.setPixelColor(poziceStupen + 1, pixels.Color(wr, wg, wb));
  pixels.setPixelColor(poziceStupen + 2, pixels.Color(wr, wg, wb));
  pixels.setPixelColor(poziceStupen + 3, pixels.Color(wr, wg, wb));
  //C
  if (temp>-10) {
    pixels.setPixelColor(Digit4 + 2, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 3, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 4, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 5, pixels.Color(wr, wg, wb));
  }
  
  int t1, t2;

  if (temp >=10) {
    t1 = abs(temp) / 10;
    t2 = abs(temp) % 10;
    DrawDigit(Digit1, wr, wg, wb, t1);
    DrawDigit(Digit2, wr, wg, wb, t2);
  } else if (temp < 10 && temp >= 0) {
    t2 = abs(temp) % 10;
    DrawDigit(Digit2, wr, wg, wb, t2);
  } else if (temp < 0 && temp > -10) {
    pixels.setPixelColor(Digit1 + 0, pixels.Color(wr, wg, wb)); //-
    t2 = abs(temp) % 10;
    DrawDigit(Digit2, wr, wg, wb, t2);
  } else {
    pixels.setPixelColor(Digit1 + 0, pixels.Color(wr, wg, wb)); //-
    t1 = abs(temp) / 10;
    t2 = abs(temp) % 10;
    DrawDigit(Digit2, wr, wg, wb, t1);
    DrawDigit(Digit3, wr, wg, wb, t2);
  }

  //dots off
  pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
  pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
  pixels.show();
  return 1;
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
  if (temp < -40) {
    R = 0; G = 197; B = 229;
  } else if (temp < -30) {
    R = 28; G = 203; B = 204;
  } else if (temp < -20) {
    R = 56; G = 209; B = 180;
  } else if (temp < -10) {
    R = 84; G = 216; B = 156;
  } else if (temp < 0) {
    R = 112; G = 222; B = 131;
  } else if (temp < 4) {
    R = 140; G = 229; B = 107;
  } else if (temp < 8) {
    R = 168; G = 235; B = 83;
  } else if (temp < 14) {
    R = 196; G = 242; B = 58;
  } else if (temp < 18) {
    R = 224; G = 248; B = 34;
  } else if (temp < 22) {
    R = 253, G = 244, B = 10;
  } else if (temp < 26) {
    R = 253, G = 233, B = 10;
  } else if (temp < 30) {
    R = 254, G = 142, B = 10;
  } else if (temp < 34) {
    R = 254, G = 105, B = 10;
  } else {
    R = 255, G = 0, B = 0;
  }
  wr = R; wg = G; wb = B;
}

bool processJson(String message) {
  char json[300];
  message.toCharArray(json, 300);
  DEBUG_PRINTLN(json);

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, json);

  int brightness = doc["brightness"];
  if (brightness>0) {
    SetBrightness(brightness);
  } else {
    Set1Color(1, doc["digit1"][0], doc["digit1"][1], doc["digit1"][2]);
    Set1Color(2, doc["digit2"][0], doc["digit2"][1], doc["digit2"][2]);
    Set1Color(3, doc["digit3"][0], doc["digit3"][1], doc["digit3"][2]);
    Set1Color(4, doc["digit4"][0], doc["digit4"][1], doc["digit4"][2]);
    Set1DotColor(1, doc["dot1"][0], doc["dot1"][1], doc["dot1"][2]);
    Set1DotColor(2, doc["dot2"][0], doc["dot2"][1], doc["dot2"][2]);
  }
  
  // if (mymode=='0') { //0
    // type = '0';
    // Off();
  // }
  // if (mymode=='c') { //c
    // type = 'c';
    // Clock();
  // }
  // if (mymode=='w') { // w
    // type = 'w';
    // Weather();
  // }
  // if (mymode=='b') {  //b;80
		// ExtractValues(2, 1);
    // SetBrightness(atoi(vals[0]));
  // }
  // if (mymode == 'f') { // f;1;8;255;34
    // ExtractValues(4,3);
    // Set1Color(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
  // }
  // if (mymode == 'h') { // h;1;255;0;0
    // ExtractValues(4,3);
    // Set1DotColor(receivedChars[2] - '0', atoi(vals[0]), atoi(vals[1]), atoi(vals[2]));
  // }


  // realRed = doc["red"];
  // DEBUG_PRINTLN(realRed);
  // realBlue = doc["blue"];
  // DEBUG_PRINTLN(realBlue);
  // realGreen = doc["green"];
  // DEBUG_PRINTLN(realGreen);
  // analogWrite(redPin, realRed);
  // analogWrite(bluePin, realBlue);
  // analogWrite(greenPin, realGreen);
  return true;
} 
