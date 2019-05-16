#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <timer.h>
auto timer = timer_create_default(); // create a timer with default settings
Timer<> default_timer; // save as above

#include "TM1637.h"
#include <PubSubClient.h>

const char* mqtt_server = "192.168.1.56";       // Enter the IP-Address of your Raspberry Pi
const int   mqtt_port = 1883;                   // port


#define mqtt_auth 1                                         // Set this to 0 to disable authentication
#define mqtt_user               "datel"                     // Username for mqtt, not required if auth is disabled
#define mqtt_password           "hanka12"                   // Password for mqtt, not required if auth is disabled

#define mqtt_topic              "/home/Clock1"           // here you have to set the topic for mqtt
#define mqtt_topic_weather      "/home/Meteo/Temperature"

int8_t        TimeDisp[]                    = {0x00,0x00,0x00,0x00};
unsigned char ClockPoint                    = 1;

#define CLK D2//pins//pins definitions for TM1637 and can be changed to other ports
#define DIO D3
 
TM1637 tm1637(CLK,DIO);

#define ota
#ifdef ota
#include <ArduinoOTA.h>
#define HOSTNAMEOTA   "clock"
#endif

#define verbose
#ifdef verbose
  #define DEBUG_PRINT(x)         Serial.print (x)
  #define DEBUG_PRINTDEC(x)      Serial.print (x, DEC)
  #define DEBUG_PRINTLN(x)       Serial.println (x)
  #define DEBUG_PRINTF(x, y)     Serial.printf (x, y)
  #define PORTSPEED 115200
  #define SERIAL_BEGIN           Serial.begin(PORTSPEED) 
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
#endif 

#include <TimeLib.h>
#include <Timezone.h>
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

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  tm1637.display(0x00,0x6D);
  tm1637.display(0x01,0x4F);
  tm1637.display(0x02,0x78);
  tm1637.display(0x03,0x00);
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
    int v = abs(round(val.toFloat()));
    char cstr[5];
    itoa(v, cstr, 10);
    // DEBUG_PRINTLN(cstr);
    String temp = cstr;

    DEBUG_PRINTLN(temp);
    
    tm1637.clearDisplay();
    TimeDisp[0] = 0x7f;
    TimeDisp[1] = 0x7f;
    TimeDisp[2] = 0x7f;
    TimeDisp[3] = 0x7f; //1100011
    tm1637.point(POINT_OFF);
    
    if (temp.length()==1) {
      TimeDisp[2] = temp.substring(0,1).toInt();
    } else if (temp.length()==2) {
      TimeDisp[2] = temp.substring(0,1).toInt();
      TimeDisp[1] = temp.substring(1,2).toInt();
    } else {
      TimeDisp[2] = temp.substring(0,1).toInt();
      TimeDisp[1] = temp.substring(1,2).toInt();
      TimeDisp[0] = temp.substring(2,3).toInt();
    }  

    tm1637.display(TimeDisp);
    tm1637.display(3,18); // put degree

    delay(2000);
  } else if (strcmp(topic, "/home/Clock1/restart")==0) {
    DEBUG_PRINT("RESTART");
    ESP.restart();
  }
}



void setup() {
  SERIAL_BEGIN;
  DEBUG_PRINTLN("CLOCK");
  tm1637.init();
  tm1637.set(7);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;

  tm1637.display(0x00,8);
  tm1637.display(0x01,8);
  tm1637.display(0x02,8);
  tm1637.display(0x03,8);

  setupWifi();
  
  setupOTA();

  DEBUG_PRINTLN("Setup TIME");
  EthernetUdp.begin(localPort);
  DEBUG_PRINT("Local port: ");
  DEBUG_PRINTLN(EthernetUdp.localPort());
  DEBUG_PRINTLN("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  
  printSystemTime();

  timer.every(1000, TimingISR);
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
  DEBUG_PRINTLN();
  printSystemTime();
  ClockPoint = (~ClockPoint) & 0x01;
  TimeUpdate();
  tm1637.display(TimeDisp);
}

void TimeUpdate(void) {
  if(ClockPoint)tm1637.point(POINT_ON);
  else tm1637.point(POINT_OFF);
  TimeDisp[0] = hour() / 10;
  TimeDisp[1] = hour() % 10;
  TimeDisp[2] = minute() / 10;
  TimeDisp[3] = minute() % 10;
}


void setupWifi() {
  DEBUG_PRINTLN("-------WIFI Setup---------");
  WiFi.printDiag(Serial);
    
  //WiFiManager
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  
  wifiManager.setTimeout(30);
  wifiManager.setConnectTimeout(30); 
  //wifiManager.setConfigPortalTimeout(60);
  
  if (!wifiManager.autoConnect("Clock", "password")) { 
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 
  
  //if you get here you have connected to the WiFi
  DEBUG_PRINTLN("CONNECTED");
  DEBUG_PRINT("Local ip : ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINTLN(WiFi.subnetMask());
  
  IPAddress ip = WiFi.localIP();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  for (byte i=0; i<4; i++) {
    dispIP(ip, i);
    delay(500);
  }
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
}

void setupOTA() {
#ifdef ota
  //OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAMEOTA);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    // String type;
    // if (ArduinoOTA.getCommand() == U_FLASH)
      // type = "sketch";
    // else // U_SPIFFS
      // type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //DEBUG_PRINTLN("Start updating " + type);
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
}


void dispIP(IPAddress ip, byte index) {
  tm1637.clearDisplay();
  TimeDisp[0] = 0x7f;
  TimeDisp[1] = 0x7f;
  TimeDisp[2] = 0x7f;
  TimeDisp[3] = 0x7f;
  if (String(ip[index]).length()==1) {
    TimeDisp[3] = String(ip[index]).substring(0,1).toInt();
  } else if (String(ip[index]).length()==2) {
    TimeDisp[2] = String(ip[index]).substring(0,1).toInt();
    TimeDisp[3] = String(ip[index]).substring(1,2).toInt();
  } else {
    TimeDisp[1] = String(ip[index]).substring(0,1).toInt();
    TimeDisp[2] = String(ip[index]).substring(1,2).toInt();
    TimeDisp[3] = String(ip[index]).substring(2,3).toInt();
  }
  tm1637.display(TimeDisp);
}

void reconnect() {
  while (!client.connected()) {
    DEBUG_PRINT("\nAttempting MQTT connection...");
    if (mqtt_auth == 1) {
      if (client.connect("Clock1", mqtt_user, mqtt_password)) {
        DEBUG_PRINTLN("connected");
        client.subscribe(mqtt_topic);
        client.subscribe(mqtt_topic_weather);
      } else {
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINT(client.state());
        DEBUG_PRINTLN(" try again in 5 seconds");
        delay(5000);
        setupWifi();
      }
    }
  }
}
