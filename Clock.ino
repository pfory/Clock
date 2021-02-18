#include "Configuration.h"

auto timer = timer_create_default(); // create a timer with default settings
Timer<> default_timer; // save as above

const uint8_t SEG_DONE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
	};

const uint8_t SEG_DEG[] = {
	SEG_A | SEG_B | SEG_F | SEG_G           // 
	};

int8_t        TimeDisp[]                    = {0x00,0x00,0x00,0x00};
unsigned char ClockPoint                    = 1;
uint32_t      heartBeat                     = 0;
float         temperature                   = -55;

TM1637Display tm1637(CLK,DIO);

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
  tm1637.showNumberDec(0000, false, 4, 3);
}

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
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

    // showTemperature(-12);
    // showTemperature(-9);
    // showTemperature(-1);
    // showTemperature(0);
    // showTemperature(5);
    // showTemperature(10);
    // showTemperature(25);
    
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str())==0) {
    DEBUG_PRINT("RESTART");
    ESP.restart();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_brightness)).c_str())==0) {
    tm1637.setBrightness(round((int)val.toFloat()));
  }
}

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

  tm1637.setBrightness(7);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  tm1637.showNumberDecEx(8888, 0b11100000, true, 4, 0);

  //show ip on display
  for (byte i=0; i<4; i++) {
    dispIP(ip, i);
    delay(500);
  }

  tm1637.setSegments(SEG_DONE);
  delay(1000);

  timer.every(500, TimingISR);
  timer.every(SENDSTAT_DELAY, sendStatisticMQTT);
  timer.every(SHOWTEMP_DELAY, showTemperature);

    
  DEBUG_PRINTLN(" Ready");
 
  drd.stop();

  DEBUG_PRINTLN(F("Setup end."));
}


void loop()
{
  timer.tick(); // tick the timer
#ifdef ota
  ArduinoOTA.handle();
#endif
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

bool TimingISR(void *) {
  //printSystemTime();
  int t = hour() * 100 + minute();

  if(ClockPoint) {
    tm1637.showNumberDecEx(t, 0, true, 4, 0);
  } else {
    tm1637.showNumberDecEx(t, 0b01000000, true, 4, 0);
  }
  ClockPoint = (~ClockPoint) & 0x01;
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

void dispIP(IPAddress ip, byte index) {
  tm1637.clear();
  tm1637.showNumberDec(ip[index], false, 3, 0);
}


void reconnect() {
  // Loop until we're reconnected
  if (!client.connected()) {
    if (lastConnectAttempt == 0 || lastConnectAttempt + connectDelay < millis()) {
      DEBUG_PRINT("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
        DEBUG_PRINTLN("connected");
        client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str());
        client.subscribe((String(mqtt_base) + "/" + String(mqtt_brightness)).c_str());
        client.subscribe(String(mqtt_topic_weather).c_str());
      } else {
        lastConnectAttempt = millis();
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINTLN(client.state());
      }
    }
  }
}

bool showTemperature(void *) {
  if (temperature==-55) {
    return true;
  }
  
  tm1637.clear();
  // 10°
  //  5°
  //  0°
  // -1°
  //-10°
  // void TM1637Display::showNumberDec(int num, bool leading_zero, uint8_t length, uint8_t pos)
  int t = round(temperature);

  if (t<=-10.f) {
    tm1637.showNumberDec(t, false, 3, 0);
  } else if (t>-10.f && t<=0.f) {
    tm1637.showNumberDec(t, false, 2, 1);
  } else if (t>0.f && t<10.f) {
    tm1637.showNumberDec(t, false, 1, 2);
  } else {
    tm1637.showNumberDec(t, false, 2, 1);
  }
  
  tm1637.setSegments(SEG_DEG, 1, 3);
  
  delay(MSECSHOWTEMP);
  return true;
}

bool sendStatisticMQTT(void *) {
  //printSystemTime();
  DEBUG_PRINTLN(F("Statistic"));

  SenderClass sender;
  sender.add("VersionSW",              VERSION);
  sender.add("Napeti",  ESP.getVcc());
  sender.add("HeartBeat",                     heartBeat++);
  if (heartBeat % 10 == 0) sender.add("RSSI", WiFi.RSSI());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  sendNetInfoMQTT();
  return true;
}


void sendNetInfoMQTT() {
  //printSystemTime();
  DEBUG_PRINTLN(F("Net info"));

  SenderClass sender;
  sender.add("IP",              WiFi.localIP().toString().c_str());
  sender.add("MAC",             WiFi.macAddress());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  return;
}
