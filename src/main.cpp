#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"
#include "main.h"
#include "cJSON.h"
#include "crc16.h"

WebServer server(80); // A WebServer objektum inicializálása, 80-as porton
char ssid[32];
char password[64];

String buffer[BUFFER_SIZE];

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

const char *getP1Text(const char *key)
{
  cJSON *root = cJSON_Parse(txtP1Rows);
  cJSON *value = cJSON_GetObjectItem(root, key);
  const char *strValue = cJSON_GetStringValue(value);
  cJSON_Delete(root);
  return strValue;
}

void onSerialData()
{
  if (Serial2.available())
  {
    String data = Serial2.readStringUntil('\n') + "\n";
    Serial.print("Serial data: ");
    Serial.println(data);
    if (data.startsWith("/"))
    {
      crc16Value = CRC16(0x0000, (unsigned char *)data.c_str(), data.length());
      write_index = 0;
      count = 0;
    }
    else if (data.startsWith("!"))
    {
      crc16Value = CRC16(crc16Value, (unsigned char *)"!", 1);
      if (data.length() > 5 && data.substring(1, 5).equalsIgnoreCase(String(crc16Value, HEX)))
      {
        data = "CRC (" + String(crc16Value, HEX) + " OK)";
      }
      else
      {
        data = "CRC (CRC error)";
      }
    }
    else
    {
      crc16Value = CRC16(crc16Value, (unsigned char *)data.c_str(), data.length());
    }
    buffer[write_index] = data;
    data.trim();
    sendMQTTMessage(data);
    write_index = (write_index + 1) % BUFFER_SIZE;
    if (count < BUFFER_SIZE)
    {
      count++;
    }
  }
}

void onRequest()
{
  String html = "<html>\n<head>\n<meta http-equiv='Content-Type' content='text/html;charset=UTF-8'>\n</head>\n<body>\n";
  for (int i = 0; i < count; i++)
  {
    int firstOpenParenth = buffer[i].indexOf("(");
    if (firstOpenParenth != -1)
    {
      String key = buffer[i].substring(0, firstOpenParenth);
      html += getP1Text(key.c_str());
      html += ": ";
    }
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
{
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
{
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  connectToRouter(ssid, password);
}

void connectToRouter(String ssid, String password)
{
  Serial.print(ssid);
  Serial.print(password);
  Serial.println("--");
  WiFi.disconnect();
  WiFi.setHostname("PowerMeter");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str()); // A WiFi hálózathoz való kapcsolódás
  int countDown = 10;
  Serial.print("Connecting to router ");
  while (WiFi.status() != WL_CONNECTED && countDown-- > 0)
  {
    Serial.print(".");
    delay(1000);
  }
  if (countDown < 1)
  {
    Serial.println("Can not connect. Back to AP mode.");
    createAP();
    return;
  }

  apActiveMillis = 0;
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
  apActiveMillis = millis();
}

void timer()
{
  unsigned long currentMillis = millis(); // aktuális idő lekérdezése

  if (currentMillis - previousMillis >= interval)
  {                                 // ha eltelt a megadott idő
    previousMillis = currentMillis; // frissítjük az előző időt
    esp_task_wdt_reset();
    flashLed();
  }
  if(apActiveMillis > 0 && currentMillis - apActiveMillis >= apInterval)
  {
    Serial.println("WIFI AP mode timed out. Restarting...");
    ESP.restart();
  }
}

void flashLed()
{
  digitalWrite(ledPin, !digitalRead(ledPin)); // LED állapotának megfordítása
}

void sendMQTTMessage(String payload)
{
  if (payload.length() > MQTT_MAX_PACKET_SIZE)
  {
    return;
  }
  int firstOpenParenth = payload.indexOf("(");
  int firstStar = payload.indexOf("*", firstOpenParenth);
  int lastCloseParenth = payload.lastIndexOf(")");
  String mqttTopic = topic + String("Dump");
  String mqttPayload = payload;
  if (firstOpenParenth != -1 && lastCloseParenth != -1)
  {
    mqttTopic = topic + payload.substring(0, firstOpenParenth);
    mqttPayload = payload.substring(firstOpenParenth + 1, firstStar != -1 ? firstStar : lastCloseParenth);
  }

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
  if (mqttClient.publish(mqttTopic.c_str(), mqttPayload.c_str()))
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
  Serial2.setRxBufferSize(SERIAL_RX_BUFFER_SIZE);
  Serial2.begin(115200, SERIAL_8N1, 16, -1, true);

  pinMode(ledPin, OUTPUT); // LED lábának beállítása kimenetre

  // Watchdog beállítása 12 másodperces időkorlátra
  esp_task_wdt_init(12, true);
  esp_task_wdt_add(NULL);

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

  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setBufferSize(MQTT_INCREASED_PACKET_SIZE);
  Serial.println("Starting MQTT callback");
  // mqttClient.setCallback(mqttCallback);
  // int tryMqtt = 0;
  // while (!mqttClient.connected() && tryMqtt++ < 5)
  // {
  //   if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
  //   {
  //     mqttClient.subscribe(topic);
  //   }
  //   else
  //   {
  //     Serial.println("Waitinf for MQTT - " + tryMqtt);
  //     delay(1000);
  //   }
  // }
  Serial.println("MQTT callback connected");

  // OTA
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
  //
  // Hostname defaults to esp32-[MAC]
  ArduinoOTA.setHostname("esppowermeter");
  //
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  //
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { 
                    Serial.printf("Progress: %u%%\r", (progress / (total / 100))); 
                    esp_task_wdt_reset();
                  })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
}

void loop()
{
  server.handleClient();
  onSerialData();
  mqttClient.loop();
  ArduinoOTA.handle();
  timer();
}
