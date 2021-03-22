#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <timer.h>
#include <Ticker.h>
#include <DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
#include "Sender.h"
#include <PubSubClient.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Adafruit_NeoPixel.h>

//SW name & version
#define     SW_NAME                       "ClockBig"
#define     VERSION                       "1.03"

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
  #define DEBUG_PRINT(x)                      Serial.print (x)
  #define DEBUG_PRINTDEC(x)                   Serial.print (x, DEC)
  #define DEBUG_PRINTLN(x)                    Serial.println (x)
  #define DEBUG_PRINTF(x, y)                  Serial.printf (x, y)
  #define PORTSPEED 115200              
  #define DEBUG_WRITE(x)                      Serial.write (x)
  #define DEBUG_PRINTHEX(x)                   Serial.print (x, HEX)
  #define SERIAL_BEGIN                        Serial.begin(PORTSPEED)
  #define DEBUG_PRINTF(str, ...)              Serial.printf(str, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
  #define DEBUG_WRITE(x)
  #define DEBUG_PRINTF(str, ...)
#endif 


// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 2
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define CONFIG_PORTAL_TIMEOUT 60 //jak dlouho zustane v rezimu AP nez se cip resetuje
#define CONNECT_TIMEOUT 120 //jak dlouho se ceka na spojeni nez se aktivuje config portal

static const char* const      mqtt_server                    = "192.168.1.56";
static const uint16_t         mqtt_port                      = 1883;
static const char* const      mqtt_username                  = "datel";
static const char* const      mqtt_key                       = "hanka12";
static const char* const      mqtt_base                      = "/home/ClockBig";
static const char* const      mqtt_topic_weather             = "/home/Meteo/Temperature";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_request             = "request";

#define SENDSTAT_DELAY                       60000  //poslani statistiky kazdou minutu

uint32_t              connectDelay                = 30000; //30s
uint32_t              lastConnectAttempt          = 0;  


/* ----------------------------------------------LED STRIP CONFIG---------------------------------------------- */
#define       NUMPIXELS                   30
//offsets
#define       Digit1                      0
#define       Digit2                      7
#define       Digit3                      16
#define       Digit4                      23

#define       PIN                         5 // Pin of the led strip, default 2 (that is D4 on the wemos)

// uncomment the line below to enable the startup animation
#define       STARTUP_ANIMATION

#endif
