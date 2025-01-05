#include "Configuration.h"

const uint8_t SEG_DONE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
};

const uint8_t SEG_CONF[] = {
	SEG_D | SEG_E | SEG_G,                           // c
	SEG_C | SEG_D | SEG_E | SEG_G,                   // o
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_E | SEG_F | SEG_G                    // F
};

const uint8_t SEG_DEG[] = {
	SEG_A | SEG_B | SEG_F | SEG_G                    //degree 
};

const uint8_t SEG_MINUS[] = {
	SEG_G,                                            //-
	SEG_G,                                            //-
	SEG_G,                                            //-
	SEG_G                                             //-
};


/////////////////////
//  A
//F   B
//  G 
//E   C
//  D

const uint8_t SEG_NONE[] = {
  0,
  0,
  0,
  0
};

int8_t        TimeDisp[]                    = {0x00,0x00,0x00,0x00};
unsigned char ClockPoint                    = 1;
float         temperature                   = -55;
float         temperatureDS                 = 0.f;
bool          zhasnuto                      = false;

TM1637Display tm1637(CLK,DIO);

#ifdef TEMPERATURE_PROBE
OneWire onewire(ONE_WIRE_BUS); // pin for onewire DALLAS bus
DallasTemperature dsSensors(&onewire);
bool                  DS18B20Present      = false;
#endif

ADC_MODE(ADC_VCC);

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  String val =  String();
  DEBUG_PRINT("\nMessage arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (unsigned int i=0;i<length;i++) {
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
    if ((int)val.toFloat()==0) {
      zhasnuto = true;
    } else {
      zhasnuto = false;
      tm1637.setBrightness(round((int)val.toFloat()));
    }
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str())==0) {
    DEBUG_PRINTLN("NET INFO");
    sendNetInfoMQTT();    
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str())==0) {
    startConfigPortal();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal_stop)).c_str())==0) {
    stopConfigPortal();
  }
}

void setup() {
  preSetup();


  ticker.detach();
  //keep LED on
  digitalWrite(LED_BUILTIN, HIGH);

  tm1637.setBrightness(0x0f);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  tm1637.showNumberDecEx(8888, 0b11100000, true, 4, 0);

  //show ip on display
  IPAddress ip = WiFi.localIP();
  for (byte i=0; i<4; i++) {
    dispIP(ip, i);
    delay(500);
  }

  tm1637.setSegments(SEG_DONE);
  
#ifdef TEMPERATURE_PROBE
  Wire.begin();
  DEBUG_PRINT("Temperature probe DS18B20: ");
  dsSensors.begin(); 
  if (dsSensors.getDeviceCount()>0) {
    DEBUG_PRINTLN("Sensor found.");
    DS18B20Present = true;
    dsSensors.setResolution(12);
    dsSensors.setWaitForConversion(false);
  } else {
    DEBUG_PRINTLN("Sensor missing!!!!");
  }

  timer.every(MEAS_DELAY, meass);
#endif

  timer.every(CONNECT_DELAY, reconnect);
  timer.every(500, TimingISR);
  //timer.every(SENDSTAT_DELAY, sendStatisticMQTT);
  timer.every(SHOWTEMP_DELAY, showTemperature);
#ifdef TEMPERATURE_PROBE
  timer.every(SENDSTAT_DELAY, sendTemperatureMQTT);
#endif
  void * a=0;
  reconnect(a);
  sendNetInfoMQTT();
  sendStatisticMQTT(a);
    
  DEBUG_PRINTLN(" Ready");
 
  drd->stop();

  DEBUG_PRINTLN(F("SETUP END......................."));
}


void loop()
{
  timer.tick(); // tick the timer
#ifdef ota
  ArduinoOTA.handle();
#endif
  client.loop();
  wifiManager.process();
  drd->loop();
}

bool TimingISR(void *) {
  //printSystemTime();
  //if (false) {
  if (timeStatus() == timeNotSet) {
    tm1637.setSegments(SEG_MINUS);
  } else {
    int t = hour() * 100 + minute();

    if (zhasnuto) {
      tm1637.setSegments(SEG_NONE);
    } else {
      if(ClockPoint) {
        tm1637.showNumberDecEx(t, 0, t<60?true:false, 4, 0);
      } else {
        tm1637.showNumberDecEx(t, 0b01000000, t<60?true:false, 4, 0);
      }
    }
    ClockPoint = (~ClockPoint) & 0x01;
  }
  return true;
}

void dispIP(IPAddress ip, byte index) {
  tm1637.clear();
  tm1637.showNumberDec(ip[index], false, 3, 0);
}

bool showTemperature(void *) {
  if (zhasnuto || temperature==-55) {
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

#ifdef TEMPERATURE_PROBE
bool meass(void *) {
  digitalWrite(LED_BUILTIN, LOW);
  
  if (DS18B20Present) {
    dsSensors.requestTemperatures(); // Send the command to get temperatures
    delay(MEAS_TIME);
    if (dsSensors.getCheckForConversion()==true) {
      temperatureDS = dsSensors.getTempCByIndex(0);
    }
    DEBUG_PRINTLN("-------------");
    DEBUG_PRINT("Temperature DS18B20: ");
    DEBUG_PRINT(temperatureDS); 
    DEBUG_PRINTLN(" *C");
  } else {
    temperatureDS = 0.0; //dummy
  }
  
  digitalWrite(LED_BUILTIN, HIGH);

  return true;
}
#endif

#ifdef TEMPERATURE_PROBE
bool sendTemperatureMQTT(void *) {
  digitalWrite(LED_BUILTIN, LOW);
  DEBUG_PRINTLN(F("Temperature"));

  if (!client.connected()) {
    DEBUG_PRINTLN(F("disconnected."));
  } else {
    client.publish((String(mqtt_base) + "/Temperature").c_str(), String(temperatureDS).c_str());
  }
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}
#endif

bool reconnect(void *) {
  if (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    if (client.connect(mqtt_base, mqtt_username, mqtt_key, (String(mqtt_base) + "/LWT").c_str(), 2, true, "offline", true)) {
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal_stop)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_brightness)).c_str());
      client.subscribe(String(mqtt_topic_weather).c_str());
      client.publish((String(mqtt_base) + "/LWT").c_str(), "online", true);
      DEBUG_PRINTLN("connected");
    } else {
      DEBUG_PRINT("disconected.");
      DEBUG_PRINT(" Wifi status:");
      DEBUG_PRINT(WiFi.status());
      DEBUG_PRINT(" Client status:");
      DEBUG_PRINTLN(client.state());
    }
  }
  return true;
}

