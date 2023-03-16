const int BUFFER_SIZE = 50;
const int MAX_STRING_LENGTH = 128;

void saveWiFiCredentials(const char* ssid, const char* password);
boolean readWiFiCredentials(char* ssid, char* password);

void handleRoot();
void handleConnect();

void connectToRouter(String ssid, String password);
void createAP();

const int ledPin = 2; // beépített LED lába
unsigned long previousMillis = 0; // változó az időzítéshez
const long interval = 1000; // az időköz, amelyenként villog a LED
void flashLed();

const char* mqttBroker = "192.168.1.120";
const int mqttPort = 1883;
const char* clientId = "PowerMeter";
const char* mqttUsername = "openhabian";
const char* mqttPassword = "openhabian";
const char* topic = "pwr";
void sendMQTTMessage(String mqttPayload);
