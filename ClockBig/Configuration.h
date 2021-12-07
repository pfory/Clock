#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <timer.h>
#include <Ticker.h>
#include <DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
//#include "Sender.h"
#include <PubSubClient.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

//SW name & version
#define     SW_NAME                       "ClockBig"
#define     VERSION                       "1.42"

#define ota
#define time
#define verbose

#define CONFIG_PORTAL_TIMEOUT 60 //jak dlouho zustane v rezimu AP nez se cip resetuje
#define CONNECT_TIMEOUT 5 //jak dlouho se ceka na spojeni nez se aktivuje config portal

static const char* const      mqtt_server                    = "192.168.1.56";
static const uint16_t         mqtt_port                      = 1883;
static const char* const      mqtt_username                  = "datel";
static const char* const      mqtt_key                       = "hanka12";
static const char* const      mqtt_base                      = "/home/ClockBig";
static const char* const      mqtt_topic_weather             = "/home/Meteo";
static const char* const      mqtt_topic_temperature         = "Temperature";
static const char* const      mqtt_topic_pressure            = "Press";
static const char* const      mqtt_topic_humidity            = "Humidity";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_request             = "request";
static const char* const      mqtt_topic_netinfo             = "netinfo";
static const char* const      mqtt_topic_load                = "load";
static const char* const      mqtt_config_portal             = "config";

#define SENDSTAT_DELAY                       60000  //poslani statistiky kazdou minutu

#define CONNECT_DELAY                        5000 //ms

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

#include <fce.h>


#endif
