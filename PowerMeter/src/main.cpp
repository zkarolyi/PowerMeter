#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include "main.h"

WebServer server(80); // A WebServer objektum inicializálása, 80-as porton
char ssid[32];
char password[64];

char buffer[BUFFER_SIZE][MAX_STRING_LENGTH]; // MAX_STRING_LENGTH a legnagyobb várható string hossza

int write_index = 0; // a következő írás helye
int count = 0;       // az aktuális adatok száma a bufferben

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // handle incoming messages here
  Serial.print("Received something on MQTT (");
  Serial.print(topic);
  Serial.print(") -> ");
  String strPayload = String((char *)payload).substring(0, length);
  Serial.println(strPayload);
}

void onSerialData()
{
  if (Serial2.available())
  {
    String data = Serial2.readStringUntil('\n');
    if (data.length() > 0)
    {
      Serial.print("Serial data: ");
      Serial.println(data);
      if (data.startsWith("/"))
      {
        write_index = 0;
        count = 0;
      }
      if (data.length() > MAX_STRING_LENGTH)
      {
        strcpy(buffer[write_index], data.substring(0, MAX_STRING_LENGTH - 1).c_str());
        sendMQTTMessage(data.substring(0, MAX_STRING_LENGTH - 1));
      }
      else
      {
        strcpy(buffer[write_index], data.c_str());
        sendMQTTMessage(data);
      }
      write_index = (write_index + 1) % BUFFER_SIZE;
      if (count < BUFFER_SIZE)
      {
        count++;
      }
    }
  }
}

void onRequest()
{
  String html = "<html>\n<body>\n";
  for (int i = 0; i < count; i++)
  {
    html += buffer[i];
    html += "<br/>\n";
  }
  html += "</body>\n</html>\n";
  server.send(200, "text/html", html);
}

void onNotFound()
{
  server.send(404, "text/html", "<p1>Nem nyert</p1>");
}

void saveWiFiCredentials(const char *ssid, const char *password)
{
  // Wi-Fi hálózati adatok mentése az SPIFFS fájlrendszerbe
  File file = SPIFFS.open("/wifi.txt", FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.println(ssid);
  file.println(password);
  file.close();
}

boolean readWiFiCredentials(char *ssid, char *password)
{
  // Wi-Fi hálózati adatok olvasása az SPIFFS fájlrendszerből
  if (!SPIFFS.exists("/wifi.txt"))
  {
    Serial.println("No wifi file found");
    return false;
  }
  File file = SPIFFS.open("/wifi.txt", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return false;
  }
  file.readStringUntil('\n').toCharArray(ssid, 32);
  ssid[strlen(ssid) - 1] = '\0';
  file.readStringUntil('\n').toCharArray(password, 64);
  password[strlen(password) - 1] = '\0';
  file.close();
  Serial.println("Wifi read from file");
  return true;
}

void handleRoot()
{ // A függvény, ami a főoldalt kiszolgálja
  // Serial.print("Wifi mode: ");
  // Serial.println(WiFi.getMode());
  if (WiFi.getMode() == WIFI_STA)
  {
    server.send(200, "text/html", "<p1>Kapcsolat rendben</p1>");
  }
  else
  {
    server.send(200, "text/html", "<form method='POST' action='/connect'><label>SSID: </label><input type='text' name='ssid'><br><label>Password: </label><input type='password' name='password'><br><input type='submit' value='Connect'></form>"); // A form létrehozása az SSID és jelszó megadására
  }
}

void handleConnect()
{ // A függvény, ami a kapcsolatot kezeli
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  connectToRouter(ssid, password);
  // server.send(200, "text/html", "Connected to WiFi!");  // A kapcsolódás visszajelzése
}

void connectToRouter(String ssid, String password)
{
  Serial.print(ssid);
  Serial.print(password);
  Serial.println("--");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str()); // A WiFi hálózathoz való kapcsolódás
  int countDown = 10;
  Serial.print("Connecting to router ");
  while (WiFi.status() != WL_CONNECTED && countDown-- > 0)
  { // Várakozás a kapcsolódásra
    Serial.print(".");
    // Serial.println(WiFi.status());
    delay(1000);
  }
  if (countDown < 1)
  {
    Serial.println("Can not connect. Back to AP mode.");
    createAP();
    return;
  }

  Serial.println("Connected to WiFi");
  Serial.print("Kapott IP-cím: ");
  Serial.println(WiFi.localIP());
}

void createAP()
{
  WiFi.mode(WIFI_AP);                  // AP módban indítás
  WiFi.softAP("ESP32-AP", "almaalma"); // Az AP beállítása
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP-cím: ");
  Serial.println(myIP);
}

void timer()
{
  unsigned long currentMillis = millis(); // aktuális idő lekérdezése

  if (currentMillis - previousMillis >= interval)
  {                                 // ha eltelt a megadott idő
    previousMillis = currentMillis; // frissítjük az előző időt
    flashLed();
  }
}

void flashLed()
{
  digitalWrite(ledPin, !digitalRead(ledPin)); // LED állapotának megfordítása
}

void sendMQTTMessage(String mqttPayload)
{
  if (!mqttClient.connected())
  {
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying later");
    }
  }
  if (mqttClient.publish(topic, mqttPayload.c_str()))
  {
    Serial.println("Message published successfully.");
  }
  else
  {
    Serial.print("Error publishing message. Error code: ");
    Serial.println(mqttClient.state());
  }
}

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, -1, true);

  pinMode(ledPin, OUTPUT); // LED lábának beállítása kimenetre

  // SPIFFS inicializálása
  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to initialize SPIFFS");
    return;
  }

  Serial.println("SPIFFS initialized");

  if (readWiFiCredentials(ssid, password))
  {
    Serial.println("Connect to router");
    connectToRouter(ssid, password);
  }
  else
  {
    Serial.println("Start AP mode");
    createAP();
  }
  server.on("/", handleRoot);           // A főoldalhoz tartozó kezelő függvény
  server.on("/connect", handleConnect); // Az adatok beküldésekor meghívott függvény
  server.on("/dump", onRequest);
  server.serveStatic("/", SPIFFS, "/");
  server.onNotFound(onNotFound);
  server.begin(); // A WebServer elindítása

  Serial.println("Starting MQTT");
  mqttClient.setServer(mqttBroker, mqttPort);
  // mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected())
  {
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      mqttClient.subscribe(topic);
    }
    else
    {
      Serial.println("Waitinf for MQTT");
      delay(5000);
    }
  }
  Serial.println("MQTT connected.");
}

void loop()
{
  server.handleClient(); // A klienst kezelő függvény hívása
  // Serial.println("Loop...");
  onSerialData();
  mqttClient.loop();
  timer();
  // Serial.print("Wifi mode: ");
  // Serial.println(WiFi.getMode());
  // Serial.print("Kapott IP-cím: ");
  // Serial.println(WiFi.localIP());
  // Serial.print("AP IP-cím: ");
  // Serial.println(WiFi.softAPIP());
  // delay(1000);
}
