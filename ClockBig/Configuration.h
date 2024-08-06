#ifndef CONFIGURATION_H
#define CONFIGURATION_H

// #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
// #include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
//#include <timer.h>
// #include <arduino-timer.h>
// #include <Ticker.h>
//#include "Sender.h"
// #include <TimeLib.h>
// #include <Timezone.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

//SW name & version
#define     VERSION                       "1.64"

#define ota
#define cas
#define timers
#define verbose
#define wifidebug
//#define serverHTTP

#ifdef serverHTTP
#include <PageBuilder.h>
#include <ESP8266WebServer.h>
#endif

//#define CLOCK1 //v loznici
#define CLOCK2 //v koupelne

#ifdef CLOCK1
#define     SW_NAME                       "ClockBig"
#define     PIN                           D1 // Pin of the led strip, default 2 (that is D4 on the wemos)
#endif
#ifdef CLOCK2
#define     SW_NAME                       "ClockBig2"
#define     PIN                           D5
//#define     SHT40
#define     LIGHTSENSOR
#endif
#ifdef CLOCK3
#define     SW_NAME                       "ClockBig3"
#define     PIN                           D5
//#define     SHT40
#define     LIGHTSENSOR
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
static const char* const      mqtt_base                      = "/home/ClockBig";
static const char* const      mqtt_base_weather              = "/home/Meteo";
static const char* const      mqtt_topic_temperature         = "Temperature";
static const char* const      mqtt_topic_pressure            = "Press";
static const char* const      mqtt_topic_humidity            = "Humidity";
#endif
#ifdef CLOCK2
static const char* const      mqtt_base                      = "/home/ClockBig2";
static const char* const      mqtt_base_humidity             = "/home/zigbee2mqtt/teplotaVlhkost1";
static const char* const      mqtt_topic_humidity2           = "humidity";
static const char* const      mqtt_topic_temperature2        = "temperature";
#endif
#ifdef CLOCK3
static const char* const      mqtt_base                      = "/home/ClockBig3";
#endif
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_request             = "request";
static const char* const      mqtt_topic_netinfo             = "netinfo";
static const char* const      mqtt_topic_load                = "load";
static const char* const      mqtt_config_portal             = "config";
static const char* const      mqtt_config_portal_stop        = "disconfig";

#define SENDSTAT_DELAY                       60000  //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms
#ifdef TEMPERATURE_PROBE
#define MEAS_DELAY                           30000  //mereni teploty
#define MEAS_TIME                            750    //in ms
#endif
#ifdef LIGHTSENSOR
#define SHOW_DISPLAY                         500   //refresh displeje
#endif

/* ----------------------------------------------LED STRIP CONFIG---------------------------------------------- */
#define       NUMPIXELS                     30
//offsets
#define       Digit1                         0
#define       Digit2                         7
#define       Digit3                        16
#define       Digit4                        23

// uncomment the line below to enable the startup animation
#define       STARTUP_ANIMATION

#include <fce.h>

#endif
