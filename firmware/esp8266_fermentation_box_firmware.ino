#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <PID_v1.h>

#define RELAY D2
#define ONE_WIRE_BUS D3
#define LED_PIN D4

#define MODE_T 0
#define MODE_P 1

const char* ssid     = "";
const char* password = "";

ESP8266WebServer server(80);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress thermometer = { 0x28, 0xFF, 0x96, 0x0A, 0x63, 0x16, 0x04, 0x1E };

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);
double white  = strip.Color( 255, 255, 255 );
double red    = strip.Color( 255,   0,   0 );
double green  = strip.Color(   0, 255,   0 );
double blue   = strip.Color(   0,   0, 255 );
double orange = strip.Color( 255, 165,   0 );
double black  = strip.Color(   0,   0,   0 );

double Setpoint = 23.5;
double Input, Output;
PID myPID(&Input, &Output, &Setpoint,1.5,5,1, DIRECT);

unsigned int WindowSize = 20000;
unsigned long windowStartTime;

float temp = -127.00;

int logic_mode = MODE_T;

void printStatus() {
  String response;
  response += String(temp)+"C";
  response += " ("+String(Setpoint)+"C) - ";
  response += String(Output)+"ms ";
  float percent = Output * 100 / WindowSize;
  response += "("+String(percent)+"%) Mode: ";
  response += logic_mode == MODE_T ? "T" : "P";
  response += '\n';
  server.send(200, "text/plain", response );
}

void printJSON() {
  String json;
  String modeString = logic_mode ? "P" : "T";
  json += "{\"target\":"+String( Setpoint )+",";
  json += "\"windowSize\":"+String( WindowSize )+",";
  json += "\"rate\":"+String( Output )+",";
  json += "\"temperature\":"+String( temp )+",";
  json += "\"mode\":\""+modeString+"\"}";
  server.send(200, "application/json", json );
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void logic_mode_pid( float temperature ) {
  Input = temperature;
  myPID.Compute();

  unsigned long now = millis();
  if(now - windowStartTime>WindowSize)
  {
    windowStartTime += WindowSize;
  }
  if(Output > now - windowStartTime) {
    strip.setPixelColor( 0, red );
    digitalWrite(RELAY,HIGH);
  } else {
    strip.setPixelColor( 0, green );
    digitalWrite(RELAY,LOW);
  }
  strip.show();
}

void logic_mode_thermostat( float temperature ) {
  if( temperature > Setpoint ) {
    digitalWrite(RELAY, LOW);
    strip.setPixelColor( 0, blue );
    strip.show();
  } else if( temperature < Setpoint ) {
    digitalWrite(RELAY, HIGH);
    strip.setPixelColor( 0, orange );
    strip.show();
  }
}

void setup()
{
  Serial.begin(9600);

  SPIFFS.begin();

  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds
    delay(500);
  }
  if(i == 21){
    Serial.print("Could not connect to");
    Serial.println(ssid);
    while(1) delay(500);
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/status",  HTTP_GET, printStatus);
  server.on("/json",    HTTP_GET, printJSON);  
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  Serial.println("HTTP server started");

  pinMode(RELAY, OUTPUT);

  windowStartTime = millis();
  
  myPID.SetOutputLimits(0, WindowSize);
  myPID.SetMode(AUTOMATIC);

  sensors.begin();
  sensors.setResolution(thermometer, 12);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  strip.begin();
  strip.setPixelColor( 0, black );
  strip.show();
}

void loop()
{
  server.handleClient();

  if( sensors.isConversionAvailable(thermometer) ) {
    temp = sensors.getTempC(thermometer);
    sensors.requestTemperatures();
    Serial.println(temp);
  }

  if( temp > 40 || temp < 0 ) { // sanity check
    digitalWrite(RELAY, LOW);
    strip.setPixelColor( 0, white );
    strip.show();
    return;
  }
  if( logic_mode == MODE_T && abs(Setpoint - temp) < .1 ) {
    logic_mode = MODE_P;
  }
  if( logic_mode == MODE_P && abs(Setpoint - temp) >= .5 ) {
    logic_mode = MODE_T;
  }
  switch( logic_mode ) {
    case MODE_T:
      logic_mode_thermostat( temp );
      break;
    case MODE_P:
      logic_mode_pid( temp );
      break;
  }
}

