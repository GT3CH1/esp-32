
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <aREST.h>
#include "secret.h"

int status = WL_IDLE_STATUS;
aREST rest = aREST();
WiFiServer server(80);
int LIGHT_PIN = 2;
int PUMP_PIN = 3;
bool fishtank_light;
bool fishtank_pump;


void handleSketchDownload() {

  const char* PATH = "/ota/arduino/update-%s-%d.bin";
  const unsigned long CHECK_INTERVAL = 10000;

  static unsigned long previousMillis;

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis < CHECK_INTERVAL)
    return;
  previousMillis = currentMillis;

  WiFiClient transport;
  HttpClient client(transport, UPDATE_SERVER, SERVER_PORT);
  Serial.println(String(UPDATE_SERVER) + String(PATH));
  char buff[50];
  snprintf(buff, sizeof(buff), PATH, DEVICE_TYPE, VERSION + 1);

  Serial.print("Check for update file ");
  Serial.println(buff);

  client.get(buff);

  int statusCode = client.responseStatusCode();
  Serial.print("Update status code: ");
  Serial.println(statusCode);
  if (statusCode != 200) {
    client.stop();
    return;
  }

  long length = client.contentLength();
  if (length == HttpClient::kNoContentLengthHeader) {
    client.stop();
    Serial.println("Server didn't provide Content-length header. Can't continue with update.");
    return;
  }
  Serial.print("Server returned update file of size ");
  Serial.print(length);
  Serial.println(" bytes");

  if (!InternalStorage.open(length)) {
    client.stop();
    Serial.println("There is not enough space to store the update. Can't continue with update.");
    return;
  }
  byte b;
  while (length > 0) {
    if (!client.readBytes(&b, 1)) // reading a byte with timeout
      break;
    InternalStorage.write(b);
    length--;
  }
  InternalStorage.close();
  client.stop();
  if (length > 0) {
    Serial.print("Timeout downloading update file at ");
    Serial.print(length);
    Serial.println(" bytes. Can't continue with update.");
    return;
  }

  Serial.println("Sketch update apply and reset.");
  Serial.flush();
  InternalStorage.apply(); // this doesn't return
}

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  // put your setup code here, to run once:
  server.begin();
  rest.function("on", setOn);
  rest.function("off", setOff);
}

void checkAndConnectWifi(){
  status = WiFi.status();
  if(status != WL_CONNECTED){
    Serial.print("WiFi disconnected, connecting to wifi.");
    while(status != WL_CONNECTED){
      status = WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
      Serial.print(".");
      delay(3000);
    }
    Serial.println("");
    Serial.println("Connected");
    printIP();
    bool last_state = getLastState(LIGHT_GUID);
    checkGuid(LIGHT_GUID, last_state);
  }
}

void printIP(){
    IPAddress ip = WiFi.localIP();
    Serial.println("IP Address: ");
    Serial.println(ip);
}
  
void loop() {
  checkAndConnectWifi();
  WiFiClient client = server.available();
  rest.handle(client);
  Serial.println("In loop");
  handleSketchDownload();
}

int setOn(String guid){
    checkGuid(guid,true);
    return 1;
}

int setOff(String guid){
    checkGuid(guid,false);
    return 1;
}

void checkGuid(String guid, bool state){
  if (guid == LIGHT_GUID){
    pinMode(2,OUTPUT);
    digitalWrite(2, state);
    Serial.println("Got light guid");
  }
  sendUpdate(guid,state); 
}

void sendUpdate(String guid, bool state){
  WiFiClient wifi;
  HttpClient httpClient = HttpClient(wifi, UPDATE_SERVER, SERVER_PORT);

  String contentType = "application/x-www-form-urlencoded";
  String data = "guid=" + guid + "&ip=" + IpAddress2String(WiFi.localIP()) + "&state=" + state + "&sw_version=" + String(VERSION);
  Serial.println(data);
  httpClient.put("/smarthome/update",contentType,data);
  int statusCode = httpClient.responseStatusCode();

  Serial.print("Update status code: ");
  Serial.println(statusCode);
}

bool getLastState(String guid){
  WiFiClient wifi;
  HttpClient httpClient = HttpClient(wifi, UPDATE_SERVER, SERVER_PORT);
  httpClient.get("/smarthome/device/"+guid);
  String response = httpClient.responseBody();
  return response == "true";
}


String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}
