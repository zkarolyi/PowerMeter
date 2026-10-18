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
char ssid[SSID_SIZE];
char password[PASSWORD_SIZE];

String buffer[BUFFER_SIZE];

int write_index = 0; // a következő írás helye
int count = 0;       // az aktuális adatok száma a bufferben

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String getP1Text(const char *key)
{
  static cJSON *p1Texts = cJSON_Parse(txtP1Rows); // csak az első híváskor parse-oljuk, utána a memóriában marad
  const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(p1Texts, key));
  return text ? String(text) : String();
}

String htmlEscape(const String &text)
{
  String result;
  result.reserve(text.length());
  for (unsigned int i = 0; i < text.length(); i++)
  {
    char c = text[i];
    switch (c)
    {
    case '&': result += "&amp;"; break;
    case '<': result += "&lt;"; break;
    case '>': result += "&gt;"; break;
    case '"': result += "&quot;"; break;
    case '\'': result += "&#39;"; break;
    default: result += c;
    }
  }
  return result;
}

void onSerialData()
{
  if (Serial2.available())
  {
    String data = Serial2.readStringUntil('\n') + "\n";
    Serial.print("Serial data: ");
    Serial.println(data);
    bool telegramEnd = false;
    bool crcOk = false;
    if (data.startsWith("/"))
    {
      crc16Value = CRC16(0x0000, (unsigned char *)data.c_str(), data.length());
      write_index = 0;
      count = 0;
    }
    else if (data.startsWith("!"))
    {
      crc16Value = CRC16(crc16Value, (unsigned char *)"!", 1);
      char crcHex[5];
      snprintf(crcHex, sizeof(crcHex), "%04X", crc16Value); // vezető nullákkal, pl. 0ABC
      telegramEnd = true;
      crcOk = data.length() > 5 && data.substring(1, 5).equalsIgnoreCase(crcHex);
      if (crcOk)
      {
        data = "CRC (" + String(crcHex) + " OK)";
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
    if (count == BUFFER_SIZE)
    {
      Serial.println("Telegram longer than buffer, oldest line dropped");
    }
    buffer[write_index] = data;
    write_index = (write_index + 1) % BUFFER_SIZE;
    if (count < BUFFER_SIZE)
    {
      count++;
    }
    if (telegramEnd)
    {
      publishTelegram(crcOk);
    }
  }
}

// A teljes táviratot csak sikeres CRC ellenőrzés után küldjük MQTT-re, hibás CRC esetén csak a CRC sort
void publishTelegram(bool crcOk)
{
  int first = crcOk ? 0 : count - 1;
  int start = count < BUFFER_SIZE ? 0 : write_index; // a legrégebbi sor helye
  for (int i = first; i < count; i++)
  {
    String line = buffer[(start + i) % BUFFER_SIZE];
    line.trim();
    if (line.length() > 0)
    {
      sendMQTTMessage(line);
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
      String text = getP1Text(key.c_str());
      if (text.length() > 0)
      {
        html += htmlEscape(text);
        html += ": ";
      }
    }
    html += htmlEscape(buffer[i]);
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
  esp_task_wdt_reset();
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
  esp_task_wdt_reset();
  File file = SPIFFS.open("/wifi.txt", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return false;
  }
  String fileSsid = file.readStringUntil('\n');
  String filePassword = file.readStringUntil('\n');
  file.close();
  // println() "\r\n"-t ír, a '\r'-t le kell vágni (más szóközt nem, a jelszó tartalmazhat ilyet)
  if (fileSsid.endsWith("\r"))
    fileSsid.remove(fileSsid.length() - 1);
  if (filePassword.endsWith("\r"))
    filePassword.remove(filePassword.length() - 1);
  if (fileSsid.length() == 0)
  {
    Serial.println("Empty SSID in wifi file");
    return false;
  }
  fileSsid.toCharArray(ssid, SSID_SIZE);
  filePassword.toCharArray(password, PASSWORD_SIZE);
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
  String newSsid = server.arg("ssid");
  String newPassword = server.arg("password");
  if (newSsid.length() == 0 || newSsid.length() >= SSID_SIZE || newPassword.length() >= PASSWORD_SIZE)
  {
    server.send(400, "text/html", "<p1>Hibás SSID vagy jelszó</p1>");
    return;
  }
  saveWiFiCredentials(newSsid.c_str(), newPassword.c_str());
  newSsid.toCharArray(ssid, SSID_SIZE);
  newPassword.toCharArray(password, PASSWORD_SIZE);
  server.send(200, "text/html", "<p1>Csatlakozás: " + htmlEscape(newSsid) + "</p1>");
  wifiConnectRequested = true; // a loop() kapcsolódik, miután a válasz kiment
}

void connectToRouter(String ssid, String password)
{
  Serial.print("SSID: ");
  Serial.println(ssid);
  WiFi.disconnect();
  delay(100);
  WiFi.setHostname("PowerMeter");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str()); // A WiFi hálózathoz való kapcsolódás
  Serial.println("Connecting to router...");
  // Nem várunk itt: a checkWiFiConnection() figyeli az eredményt a loop()-ban
  apActiveMillis = 0;
  wifiConnecting = true;
  wifiConnectStartMillis = millis();
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

void checkWiFiConnection()
{
  unsigned long currentMillis = millis();

  if (wifiConnecting)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      wifiConnecting = false;
      lastWiFiCheckMillis = currentMillis;
      Serial.println("Connected to WiFi");
      Serial.print("Kapott IP-cím: ");
      Serial.println(WiFi.localIP());
    }
    else if (currentMillis - wifiConnectStartMillis >= wifiConnectTimeout)
    {
      wifiConnecting = false;
      Serial.println("Can not connect. Back to AP mode.");
      createAP();
    }
    return;
  }

  // Csak akkor ellenőrizzük, ha STA módban vagyunk, nincs AP mód aktív, és eltelt az ellenőrzési időköz
  if (WiFi.getMode() == WIFI_STA && apActiveMillis == 0 && currentMillis - lastWiFiCheckMillis >= wifiCheckInterval)
  {
    lastWiFiCheckMillis = currentMillis;
    
    // Ha nincs WiFi kapcsolat
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi connection lost. Attempting to reconnect...");
      connectToRouter(String(ssid), String(password));
      // Ha a kapcsolódás időtúllépéssel végződik, createAP() beállítja apActiveMillis-t
      // és ezzel megakadályozza a további újracsatlakozási kísérleteket
    }
  }
}

void timer()
{
  unsigned long currentMillis = millis(); // aktuális idő lekérdezése

  if (currentMillis - previousMillis >= interval)
  {                                 // ha eltelt a megadott idő
    previousMillis = currentMillis; // frissítjük az előző időt
    esp_task_wdt_reset();
    if (apActiveMillis == 0)
    {
      flashLed(); // normál mód: 1 másodpercenként villog
    }
  }
  if (apActiveMillis > 0 && currentMillis - previousMillisLed >= 200)
  {
    previousMillisLed = currentMillis;
    flashLed(); // AP mód: 200ms-enként villog
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
  if (WiFi.status() != WL_CONNECTED)
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

  // fejléc (MQTT_MAX_HEADER_SIZE) + 2 bájt topic hossz + topic + payload
  if (MQTT_MAX_HEADER_SIZE + 2 + mqttTopic.length() + mqttPayload.length() > mqttClient.getBufferSize())
  {
    Serial.print("MQTT message too long, skipped: ");
    Serial.println(mqttTopic);
    return;
  }

  if (!mqttClient.connected())
  {
    unsigned long currentMillis = millis();
    if (currentMillis - lastMqttConnectAttemptMillis < mqttReconnectInterval)
    {
      return;
    }
    lastMqttConnectAttemptMillis = currentMillis;
    esp_task_wdt_reset();
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying later");
      return;
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
  // hiba esetén sem állunk meg: AP módban a web szerver és az OTA továbbra is elérhető
  bool spiffsOk = SPIFFS.begin(true);
  Serial.println(spiffsOk ? "SPIFFS initialized" : "Failed to initialize SPIFFS");

  if (spiffsOk && readWiFiCredentials(ssid, password))
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
  server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico"); // csak a favicon, a /wifi.txt ne legyen elérhető
  server.onNotFound(onNotFound);
  server.begin(); // A WebServer elindítása

  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setBufferSize(MQTT_INCREASED_PACKET_SIZE);
  mqttClient.setSocketTimeout(mqttSocketTimeout);

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
  if (wifiConnectRequested)
  {
    wifiConnectRequested = false;
    connectToRouter(ssid, password);
  }
  onSerialData();
  mqttClient.loop();
  ArduinoOTA.handle();
  checkWiFiConnection();
  timer();
}
