//ESP8266-01 FLASH SIZE 1M  BUILDIN_LED to 1 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//WEMOS 4MB

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include "TM1637Display.h"
#include <timer.h>
#include <Ticker.h>
#include <DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
#include "Sender.h"
#include <PubSubClient.h>
#include <TimeLib.h>
#include <Timezone.h>

//SW name & version
#define     VERSION                       "2.20"

#define ota
#define time
#define verbose

#define AUTOCONNECTNAME     HOSTNAMEOTA
#define AUTOCONNECTPWD      "password"
#define HOSTNAMEOTA         SW_NAME VERSION

#ifdef ota
#include <ArduinoOTA.h>
#endif

#ifdef time
#include <TimeLib.h>
#include <Timezone.h>
#endif

#ifdef serverHTTP
#include <ESP8266WebServer.h>
#endif

#define CFGFILE "/config.json"


#ifdef verbose
  #define DEBUG_PRINT(x)                     Serial.print (x)
  #define DEBUG_PRINTDEC(x)                  Serial.print (x, DEC)
  #define DEBUG_PRINTLN(x)                   Serial.println (x)
  #define DEBUG_PRINTF(x, y)                 Serial.printf (x, y)
  #define PORTSPEED 115200             
  #define DEBUG_WRITE(x)                     Serial.write (x)
  #define DEBUG_PRINTHEX(x)                  Serial.print (x, HEX)
  #define SERIAL_BEGIN                       Serial.begin(PORTSPEED)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
  #define DEBUG_WRITE(x)
#endif 


// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 2
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define CONFIG_PORTAL_TIMEOUT 60 //jak dlouho zustane v rezimu AP nez se cip resetuje
#define CONNECT_TIMEOUT 5 //jak dlouho se ceka na spojeni nez se aktivuje config portal

//#define CLOCK1 //v obyvaku
//#define CLOCK2 //nahore v loznici
//#define CLOCK3 //v dilne ESP8266-01
#define CLOCK3

#ifdef CLOCK1
#define     SW_NAME                       "Clock1"
#define     CLK D2//pins//pins definitions for TM1637 and can be changed to other ports
#define     DIO D3
#define     TEMPERATURE_PROBE
#define     ONE_WIRE_BUS  D7
#endif
#ifdef CLOCK2
#define     SW_NAME                       "Clock2"
#define     CLK D2//pins//pins definitions for TM1637 and can be changed to other ports
#define     DIO D3
#define     TEMPERATURE_PROBE
#define     ONE_WIRE_BUS  D7
#endif
#ifdef CLOCK3
#define     SW_NAME                       "Clock3"
#define     CLK 2//pins//pins definitions for TM1637 and can be changed to other ports
#define     DIO 0
#endif

#ifdef CLOCK3
#else
#endif

#ifdef TEMPERATURE_PROBE
#include <Wire.h>
#include <DallasTemperature.h>
#endif

static const char* const      mqtt_server                    = "192.168.1.56";
static const uint16_t         mqtt_port                      = 1883;
static const char* const      mqtt_username                  = "datel";
static const char* const      mqtt_key                       = "hanka12";
#ifdef CLOCK1
static const char* const      mqtt_base                      = "/home/Clock1";
#endif
#ifdef CLOCK2
static const char* const      mqtt_base                      = "/home/Clock2";
#endif
#ifdef CLOCK3
static const char* const      mqtt_base                      = "/home/Clock3";
#endif
static const char* const      mqtt_topic_weather             = "/home/Meteo/Temperature";
static const char* const      mqtt_brightness                = "brightness";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_config_portal             = "config";
static const char* const      mqtt_topic_netinfo             = "netinfo";

#define SENDSTAT_DELAY                       60000  //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms

#define SHOWTEMP_DELAY                       10000  //zobrazeni teploty na displeji
#define MSECSHOWTEMP                         1500   //na jak dlouho se teplota zobrazi v milisec
#ifdef TEMPERATURE_PROBE
#define MEAS_DELAY                           30000  //mereni teploty
#define MEAS_TIME                            750    //in ms
#endif

#endif
