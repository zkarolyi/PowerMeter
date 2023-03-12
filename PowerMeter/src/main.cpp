#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);  // A WebServer objektum inicializálása, 80-as porton

void handleRoot() {  // A függvény, ami a főoldalt kiszolgálja
  server.send(200, "text/html", "<form method='POST' action='/connect'><label>SSID: </label><input type='text' name='ssid'><br><label>Password: </label><input type='password' name='password'><br><input type='submit' value='Connect'></form>");  // A form létrehozása az SSID és jelszó megadására
}

void handleRootConnected(){ // A függvény, ami a főoldalt kiszolgálja routerhez kapcsolódás után
  server.send(200, "text/html", "<p1>Kapcsolat rendben</p1>");
}

void handleConnect() {  // A függvény, ami a kapcsolatot kezeli
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  WiFi.begin(ssid.c_str(), password.c_str());  // A WiFi hálózathoz való kapcsolódás
  while (WiFi.status() != WL_CONNECTED) {  // Várakozás a kapcsolódásra
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  server.send(200, "text/html", "Connected to WiFi!");  // A kapcsolódás visszajelzése
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);  // AP módban indítás
  WiFi.softAP("ESP32-AP", "alma");  // Az AP beállítása
  server.on("/", handleRoot);  // A főoldalhoz tartozó kezelő függvény
  server.on("/connect", handleConnect);  // Az adatok beküldésekor meghívott függvény
  server.begin();  // A WebServer elindítása
}

void loop() {
  server.handleClient();  // A klienst kezelő függvény hívása
  Serial.println("Loop...");
  delay(2000);
}
