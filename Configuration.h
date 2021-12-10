#ifndef CONFIGURATION_H
#define CONFIGURATION_H

//ESP8266-01 FLASH SIZE 1M Fs None !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//WEMOS 4MB

#include <ArduinoJson.h>
#include <TM1637Display.h>

//SW name & version
#define     VERSION                       "2.32"

#define ota
#define time
#define timers
#define verbose
#define wifidebug


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
#define BUILTIN_LED 1
#endif
static const char* const      mqtt_topic_weather             = "/home/Meteo/Temperature";
static const char* const      mqtt_brightness                = "brightness";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_config_portal             = "config";
static const char* const      mqtt_topic_netinfo             = "netinfo";
static const char* const      mqtt_config_portal_stop        = "disconfig";

#define SENDSTAT_DELAY                       60000  //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms

#define SHOWTEMP_DELAY                       10000  //zobrazeni teploty na displeji
#define MSECSHOWTEMP                         1500   //na jak dlouho se teplota zobrazi v milisec
#ifdef TEMPERATURE_PROBE
#define MEAS_DELAY                           30000  //mereni teploty
#define MEAS_TIME                            750    //in ms
#endif


#include <fce.h>


#endif
