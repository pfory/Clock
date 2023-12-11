#include "Configuration.h"

float         temperature                   = -55;
float         pressure                      = 0;
float         humidity                      = 0;
unsigned char ClockPoint                    = 1;
#ifdef TEMPERATURE_PROBE
float         temperatureDS                 = 0.f;
#endif
#ifdef LIGHTSENSOR
int           jas[10];
int           jas_prumer                    = 255;
int           jas_counter                   = 0;
#endif

#ifdef TEMPERATURE_PROBE
OneWire onewire(ONE_WIRE_BUS); // pin for onewire DALLAS bus
DallasTemperature dsSensors(&onewire);
bool                  DS18B20Present      = false;
#endif


const byte numChars = 50;
char receivedChars[numChars]; // an array to store the received data

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
 
  if (strcmp(topic, (String(mqtt_base_weather) + "/" + String(mqtt_topic_temperature)).c_str())==0) {
    DEBUG_PRINT("Temperature from Meteo: ");
    DEBUG_PRINTLN(val.toFloat());
    temperature = val.toFloat();
  } else if (strcmp(topic, (String(mqtt_base_weather) + "/" + String(mqtt_topic_pressure)).c_str())==0) {
    DEBUG_PRINT("Pressure from Meteo: ");
    DEBUG_PRINTLN(val.toFloat());
    pressure = val.toFloat();
  } else if (strcmp(topic, (String(mqtt_base_weather) + "/" + String(mqtt_topic_humidity)).c_str())==0) {
    DEBUG_PRINT("Humidity from Meteo: ");
    DEBUG_PRINTLN(val.toFloat());
    humidity = val.toFloat();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str())==0) {
    Off();
    DEBUG_PRINT("RESTART");
    ESP.restart();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str())==0) {
    DEBUG_PRINT("NET INFO");
    sendNetInfoMQTT();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_load)).c_str())==0) {
    processJson(val);
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str())==0) {
    startConfigPortal();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str())==0) {
    stopConfigPortal();
  }
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

//int Brightness = 0;

// Color of the digits for weather
int wr = 255;
int wg = 255;
int wb = 255;


void setup() {
  preSetup();
  ticker.detach();
    
  pixels.begin();
  startupAnimation();
  type = 'c';
  //Brightness = 255;

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

#ifdef LIGHTSENSOR
  timer.every(SHOW_DISPLAY, change_brightness);
#endif
  timer.every(CONNECT_DELAY, reconnect);
  timer.every(500, TimingISR);
  timer.every(SENDSTAT_DELAY, sendStatisticMQTT);

  client.publish((String(mqtt_base) + "/mqtt_topic_request").c_str(), "setup");
  
  void * a=0;
  reconnect(a);
  sendNetInfoMQTT();
  sendStatisticMQTT(a);
   
  DEBUG_PRINTLN(F("SETUP END."));
}


void loop() {
  timer.tick(); // tick the timer
#ifdef ota
  ArduinoOTA.handle();
#endif
  client.loop();
  wifiManager.process();  
  drd.loop();
}

#ifdef LIGHTSENSOR
bool change_brightness(void *) {
  if (jas_counter < 10) {
    DEBUG_PRINT("Napětí:");
    DEBUG_PRINT(ESP.getVcc());
    int j = map(ESP.getVcc(), 2860, 3500, 1, 255);
    DEBUG_PRINT(" Jas:");
    DEBUG_PRINT(j);
    j = constrain(j,1,255);
    DEBUG_PRINT(" Jas constraint:");
    DEBUG_PRINTLN(j);
    jas[jas_counter] = j;
    jas_counter++;
  } else {
    jas_prumer = 0;
    for (int i=0; i<10; i++) {
      jas_prumer+=jas[i];
      jas[i]=0;
    }
    
    jas_prumer /= 10;
    jas_counter = 0;
    DEBUG_PRINT("Prumer:");
    DEBUG_PRINTLN(jas_prumer);
    SetBrightness(jas_prumer);
  }
  return true;
}
#endif

bool TimingISR(void *) {
  if (type=='0') {
  } else if ((second()%10)<1) {
    changeColorDigitToRandom();
    type = 'w';
    if (Weather(0)==0) { //jeste neni teplota nactena z meteo
      type='c';
    }
#ifdef CLOCK1    
  } else if ((second()%10)<2) {
    changeColorDigitToRandom();
    type = 'w';
    if (Weather(1)==0) { //jeste neni vlhkost nactena z meteo
      type='c';
    }
  } else if ((second()%10)<3) {
    changeColorDigitToRandom();
    type = 'w';
    if (Weather(2)==0) { //jeste neni tlak nacteny z meteo
      type='c';
    }
#endif
  } else {
    type = 'c';
    DrawTime();
    if (ClockPoint) {
      dot1 = dot2 = 0;
    } else {
      dot1 = dot2 = 1;
    }
    DrawDots();
    ClockPoint = (~ClockPoint) & 0x01;
  }
  return true;
}

void changeColorDigitToRandom() {
  int r = 0, g = 0, b = 0;
  while (r<10 && g<10 && b<10) {
    r = random(255);
    g = random(255);
    b = random(255);
  }
  Set1Color(1, r, g, b);
  Set1Color(2, r, g, b);
  r = 0, g = 0, b = 0;
  while (r<10 && g<10 && b<10) {
    r = random(255);
    g = random(255);
    b = random(255);
  }
  Set1Color(3, r, g, b);
  Set1Color(4, r, g, b);
  r = 0, g = 0, b = 0;
  while (r<10 && g<10 && b<10) {
    r = random(255);
    g = random(255);
    b = random(255);
  }
  Set1DotColor(1, r, g, b);
  Set1DotColor(2, r, g, b);
}

void DrawTime() {
 //DEBUG_PRINTLN("Updating Time: %d:%d:%d\n", hours, mins, secs);
 DrawDigit(Digit1, cd[0], cd[1], cd[2], hour() / 10); //Draw the first digit of the hour
 DrawDigit(Digit2, cd[3], cd[4], cd[5], hour() - ((hour() / 10) * 10)); //Draw the second digit of the hour

 DrawDigit(Digit3, cd[6], cd[7], cd[8], minute() / 10); //Draw the first digit of the minute
 DrawDigit(Digit4, cd[9], cd[10], cd[11], minute() - ((minute() / 10) * 10)); //Draw the second digit of the minute
}

void DrawDots() {
 if (dot1)pixels.setPixelColor(Digit3 - 1, pixels.Color(cdo[0], cdo[1], cdo[2]));
 else pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
 if (dot2)pixels.setPixelColor(Digit3 - 2, pixels.Color(cdo[3], cdo[4], cdo[5]));
 else pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
}

void DrawDigit(int offset, int r, int g, int b, int n) {
#ifdef CLOCK1
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
#endif
#ifdef CLOCK2
  if (n == 0 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) { //TOP LEFT
  pixels.setPixelColor(0 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(0 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) { //TOP
  pixels.setPixelColor(1 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(1 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 0 || n == 1 || n == 2 || n == 3 || n == 4 || n == 7 || n == 8 || n == 9) { //TOP RIGHT
  pixels.setPixelColor(2 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(2 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 0 || n == 1 || n == 3 || n == 4 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) { //BOTTOM RIGHT
  pixels.setPixelColor(3 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(3 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 8 || n == 9) { //BOTTOM
  pixels.setPixelColor(4 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(4 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 0 || n == 2 || n == 6 || n == 8) { //BOTTOM LEFT
  pixels.setPixelColor(5 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(5 + offset, pixels.Color(0, 0, 0));
  }
  if (n == 2 || n == 3 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) { //MIDDLE
  pixels.setPixelColor(6 + offset, pixels.Color(r, g, b));
  } else {
  pixels.setPixelColor(6 + offset, pixels.Color(0, 0, 0));
  }
#endif
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


void Off() {
  DEBUG_PRINTLN("Clock is turning OFF");
  SetBrightness(0);
}

void SetBrightness(int br) {
  DEBUG_PRINTF("Setting brightness to %d%\n", br);
  pixels.setBrightness(br);
  //Brightness = br;
  pixels.show();
}

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
    DEBUG_PRINT("Extracting: "); 
    DEBUG_PRINTLN(vals[p]);
  }
}

void Set1Color(int mydigit, int r, int g, int b) {
  //DEBUG_PRINTF("Setting digit %d to color: r:%d g:%d b:%d\n", mydigit, r, g, b);
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

void Set1DotColor(int dotnr, int r, int g, int b) {
  //DEBUG_PRINTF("Setting dot %d to color: r:%d g:%d b:%d\n", dotnr, r, g, b);
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


int Weather(int type) {
  if (type==0) { //temperature
    int temp  = (int)round(temperature);
    
    if (temp==-55) {
      DEBUG_PRINTLN("No temperature from Meteo unit yet");
      return 0;
    }

    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }

    GetWeatherColor(temp);

    int poziceStupen;
    // °
    if (temp>-10) {
      poziceStupen = Digit3;
    } else {
      poziceStupen = Digit4;
    }
    //znak stupen
    pixels.setPixelColor(poziceStupen + 0, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(poziceStupen + 1, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(poziceStupen + 2, pixels.Color(wr, wg, wb));
#ifdef CLOCK1
    pixels.setPixelColor(poziceStupen + 3, pixels.Color(wr, wg, wb));
#endif    
#ifdef CLOCK2
    pixels.setPixelColor(poziceStupen + 6, pixels.Color(wr, wg, wb));
#endif    
    //znak C
    if (temp>-10) {
#ifdef CLOCK1      
      pixels.setPixelColor(Digit4 + 2, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 3, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 4, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 5, pixels.Color(wr, wg, wb));
#endif
#ifdef CLOCK2
      pixels.setPixelColor(Digit4 + 0, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 1, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 4, pixels.Color(wr, wg, wb));
      pixels.setPixelColor(Digit4 + 5, pixels.Color(wr, wg, wb));
#endif
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
#ifdef CLOCK1      
      pixels.setPixelColor(Digit1 + 0, pixels.Color(wr, wg, wb)); //-
#endif
#ifdef CLOCK2
      pixels.setPixelColor(Digit1 + 6, pixels.Color(wr, wg, wb)); //-
#endif
      t2 = abs(temp) % 10;
      DrawDigit(Digit2, wr, wg, wb, t2);
    } else {
#ifdef CLOCK1      
      pixels.setPixelColor(Digit1 + 0, pixels.Color(wr, wg, wb)); //-
#endif
#ifdef CLOCK2
      pixels.setPixelColor(Digit1 + 6, pixels.Color(wr, wg, wb)); //-
#endif
      t1 = abs(temp) / 10;
      t2 = abs(temp) % 10;
      DrawDigit(Digit2, wr, wg, wb, t1);
      DrawDigit(Digit3, wr, wg, wb, t2);
    }

    //dots off
    pixels.setPixelColor(Digit3 - 2, pixels.Color(0, 0, 0));
    pixels.setPixelColor(Digit3 - 1, pixels.Color(0, 0, 0));
  } else if (type==1) { //humidity
    int hum   = (int)round(humidity);
    if (hum==0) {
      DEBUG_PRINTLN("No humidity from Meteo unit yet");
      return 0;
    }
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }

    //rH
    pixels.setPixelColor(Digit3 + 0, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit3 + 4, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 0, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 1, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 3, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 4, pixels.Color(wr, wg, wb));
    pixels.setPixelColor(Digit4 + 6, pixels.Color(wr, wg, wb));
    int h1, h2;
    h1 = abs(hum) / 10;
    h2 = abs(hum) % 10;
    if (hum==100) {
      DrawDigit(Digit1, wr, wg, wb, 0);
    } else {
      DrawDigit(Digit1, wr, wg, wb, h1);
    }
    DrawDigit(Digit2, wr, wg, wb, h2);
    
  } else { //press
    int press = (int)round(pressure/100);
    if (press==0) {
      DEBUG_PRINTLN("No pressure from Meteo unit yet");
      return 0;
    }
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    }
    int p1, p2, p3, p4;
    p1 = abs(press) / 1000;
    p2 = abs(press) % 1000 / 100;
    p3 = abs(press) % 100 / 10;
    p4 = abs(press) % 10;
    if (p1>0) {
      DrawDigit(Digit1, wr, wg, wb, p1);
    }
    DrawDigit(Digit2, wr, wg, wb, p2);
    DrawDigit(Digit3, wr, wg, wb, p3);
    DrawDigit(Digit4, wr, wg, wb, p4);
  }
 
  pixels.show();
  return 1;
}

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
  
  String e = doc["brightness"];

  DEBUG_PRINTLN(e);

  if (e=="null") {
    Set1Color(1, doc["digit1"][0], doc["digit1"][1], doc["digit1"][2]);
    Set1Color(2, doc["digit2"][0], doc["digit2"][1], doc["digit2"][2]);
    Set1Color(3, doc["digit3"][0], doc["digit3"][1], doc["digit3"][2]);
    Set1Color(4, doc["digit4"][0], doc["digit4"][1], doc["digit4"][2]);
    Set1DotColor(1, doc["dot1"][0], doc["dot1"][1], doc["dot1"][2]);
    Set1DotColor(2, doc["dot2"][0], doc["dot2"][1], doc["dot2"][2]);
  } else {
#ifdef CLOCK1    
    SetBrightness(doc["brightness"]);
#endif
  }
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

bool reconnect(void *) {
  if (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    if (client.connect(mqtt_base, mqtt_username, mqtt_key, (String(mqtt_base) + "/LWT").c_str(), 2, true, "offline", true)) {
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal_stop)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_load)).c_str());
      client.subscribe((String(mqtt_base_weather) + "/" + String(mqtt_topic_temperature)).c_str());
      client.subscribe((String(mqtt_base_weather) + "/" + String(mqtt_topic_pressure)).c_str());
      client.subscribe((String(mqtt_base_weather) + "/" + String(mqtt_topic_humidity)).c_str());
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