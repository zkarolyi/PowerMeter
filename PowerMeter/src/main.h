const int BUFFER_SIZE = 20;
const int MAX_STRING_LENGTH = 128;

void saveWiFiCredentials(const char* ssid, const char* password);
boolean readWiFiCredentials(char* ssid, char* password);

void handleRoot();
void handleConnect();

void connectToRouter(String ssid, String password);
void createAP();