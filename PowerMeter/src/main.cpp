#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "main.h"

WebServer server(80); // A WebServer objektum inicializálása, 80-as porton
char ssid[32];
char password[64];

char buffer[BUFFER_SIZE][MAX_STRING_LENGTH]; // MAX_STRING_LENGTH a legnagyobb várható string hossza

int write_index = 0;  // a következő írás helye
int read_index = 0;   // a következő olvasás helye
int count = 0;        // az aktuális adatok száma a bufferben

void onSerialData() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    if (data.length() > 0) {
      // csak akkor írjuk a bufferbe, ha van adat
      strcpy(buffer[write_index], data.c_str());
      write_index = (write_index + 1) % BUFFER_SIZE;
      if (count < BUFFER_SIZE) {
        count++;
      }
    }
  }
}

void onRequest() {
  String html = "<html><body>";
  for (int i = 0; i < count; i++) {
    int index = (read_index + i) % BUFFER_SIZE;
    html += buffer[index];
    html += "<br/>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void onNotFound(){
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
  ssid[strlen(ssid)-1] = '\0';
  file.readStringUntil('\n').toCharArray(password, 64);
  password[strlen(password)-1] = '\0';
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
    //Serial.println(WiFi.status());
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
  WiFi.mode(WIFI_AP);                   // AP módban indítás
  WiFi.softAP("ESP32-AP", "almaalma");  // Az AP beállítása
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP-cím: ");
  Serial.println(myIP);
}

void setup()
{
  Serial.begin(115200);

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
  server.onNotFound(onNotFound);
  server.begin();                       // A WebServer elindítása
}

void loop()
{
  server.handleClient(); // A klienst kezelő függvény hívása
  Serial.println("Loop...");
  onSerialData();
  // Serial.print("Wifi mode: ");
  // Serial.println(WiFi.getMode());
  // Serial.print("Kapott IP-cím: ");
  // Serial.println(WiFi.localIP());
  // Serial.print("AP IP-cím: ");
  // Serial.println(WiFi.softAPIP());
  delay(2000);
}
