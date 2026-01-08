const int BUFFER_SIZE = 50;

void saveWiFiCredentials(const char* ssid, const char* password);
boolean readWiFiCredentials(char* ssid, char* password);
unsigned long apActiveMillis = 0; // mennyi ideje van AP módban a WIFI. 0: nem AP módban van
const long apInterval = 600000; // AP mód timeout
unsigned long lastWiFiCheckMillis = 0; // utolsó WiFi ellenőrzés ideje
const long wifiCheckInterval = 10000; // WiFi ellenőrzés gyakorisága (10 másodperc)

void handleRoot();
void handleConnect();

void connectToRouter(String ssid, String password);
void createAP();
void checkWiFiConnection();

const int ledPin = 2; // beépített LED lába
unsigned long previousMillis = 0; // változó az időzítéshez
const long interval = 1000; // az időköz, amelyenként villog a LED
void flashLed();

const int SERIAL_RX_BUFFER_SIZE = 3500;

const char* mqttBroker = "192.168.1.120";
const int mqttPort = 1883;
const char* clientId = "PowerMeter";
const char* mqttUsername = "openhabian";
const char* mqttPassword = "openhabian";
const char* topic = "PowerMeter/";
int crc16Value = 0;
const int MQTT_INCREASED_PACKET_SIZE = 256;
void sendMQTTMessage(String mqttPayload);

const char* txtP1Rows = "{\
\"0-0:1.0.0\":\"Idő\",\
\"0-0:42.0.0\":\"COSEM logikai készüléknév\",\
\"0-0:96.1.0\":\"Mérő gyáriszám\",\
\"0-0:96.14.0\":\"Aktuális tarifa\",\
\"0-0:96.50.68\":\"Megszakító státusz\",\
\"0-0:17.0.0\":\"Limiter határérték\",\
\"1-0:1.8.0\":\"Hatásos import energia (+A)\",\
\"1-0:1.8.1\":\"Hatásos import energia (+A) - tarifa 1\",\
\"1-0:1.8.2\":\"Hatásos import energia (+A) - tarifa 2\",\
\"1-0:1.8.3\":\"Hatásos import energia (+A) - tarifa 3\",\
\"1-0:1.8.4\":\"Hatásos import energia (+A) - tarifa 4\",\
\"1-0:2.8.0\":\"Hatásos export energia (-A)\",\
\"1-0:2.8.1\":\"Hatásos export energia (-A) - tarifa 1\",\
\"1-0:2.8.2\":\"Hatásos export energia (-A) - tarifa 2\",\
\"1-0:2.8.3\":\"Hatásos export energia (-A) - tarifa 3\",\
\"1-0:2.8.4\":\"Hatásos export energia (-A) - tarifa 4\",\
\"1-0:3.8.0\":\"Import meddő energia (+R)\",\
\"1-0:4.8.0\":\"Export meddő energia (-R)\",\
\"1-0:5.8.0\":\"Meddő energia (QI)\",\
\"1-0:6.8.0\":\"Meddő energia (QII)\",\
\"1-0:7.8.0\":\"Meddő energia (QIII)\",\
\"1-0:8.8.0\":\"Meddő energia (QIV)\",\
\"1-0:15.8.0\":\"Hatásos energia kombinált (|+A|+|-A|)\",\
\"1-0:32.7.0\":\"Pillanatnyi fázis feszültség L1\",\
\"1-0:52.7.0\":\"Pillanatnyi fázis feszültség L2\",\
\"1-0:72.7.0\":\"Pillanatnyi fázis feszültség L3\",\
\"1-0:31.7.0\":\"Pillanatnyi áram L1\",\
\"1-0:51.7.0\":\"Pillanatnyi áram L2\",\
\"1-0:71.7.0\":\"Pillanatnyi áram L3\",\
\"1-0:13.7.0\":\"Pillanatnyi teljesítmény tényező\",\
\"1-0:33.7.0\":\"Pillanatnyi teljesítmény tényező L1\",\
\"1-0:53.7.0\":\"Pillanatnyi teljesítmény tényező L2\",\
\"1-0:73.7.0\":\"Pillanatnyi teljesítmény tényező L3\",\
\"1-0:14.7.0\":\"Frekvencia\",\
\"1-0:1.7.0\":\"Pillanatnyi import teljesítmény (+A)\",\
\"1-0:2.7.0\":\"Pillanatnyi export teljesítmény (-A)\",\
\"1-0:5.7.0\":\"Pillanatnyi meddő teljesítmény (QI)\",\
\"1-0:6.7.0\":\"Pillanatnyi meddő teljesítmény (QII)\",\
\"1-0:7.7.0\":\"Pillanatnyi meddő teljesítmény (QIII)\",\
\"1-0:8.7.0\":\"Pillanatnyi meddő teljesítmény (QIV)\",\
\"1-0:31.4.0\":\"Áram korlátozás határérték 1\",\
\"1-0:51.4.0\":\"Áram korlátozás határérték 2\",\
\"1-0:71.4.0\":\"Áram korlátozás határérték 3\",\
\"0-0:98.1.0\":\"Hónap végi tárolt adatok (utolsó havi)\",\
\"0-0:96.13.0\":\"Áramszolgáltatói szöveges üzenet\"\
}";
