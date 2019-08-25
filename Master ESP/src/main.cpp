#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <string.h>

#define WIFI_SSID "Gajera's"
#define WIFI_PASS "Gajera@037"

#define SOFT_SSID "master"
#define SOFT_PASS "master123"

#define SECRET "SdLwnMZTXgsbXlvJTiv4jvcDIQOZuA7KQ8TOOlZ8"
#define HOST "mini-project-2-9b3c3.firebaseio.com"
#define C_PATH "/controllers/Ln6zD6LbMOPGzSVzq2j"

ESP8266WebServer server(80);

String waterMeasures = "";
long lastPushTime = 0;

int count = 0;

void setupAccessPoint();
void setupServer();
void handleRequest();
void setupStationMode();
void beginFirebase();
void pushMeasures();
bool isPushRequired();
String getValue(String data, char separator, int index);

void setup()
{
    Serial.begin(9600);
    WiFi.mode(WIFI_AP_STA);

    setupAccessPoint();

    lastPushTime = millis();
}

void setupAccessPoint()
{
    Serial.println("Setting AP");
    WiFi.disconnect();

    Serial.println("Starting AP as " + String(SOFT_SSID));
    WiFi.softAP(SOFT_SSID, SOFT_PASS);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address is :");
    Serial.print(myIP);

    setupServer();
}

void setupServer()
{
    Serial.println("Starting server");

    server.on("/", handleRequest);

    server.begin();
}

void handleRequest()
{
    waterMeasures += server.arg("id") + ":" + server.arg("m") + ",";
    server.send(200, "text/http", "recorder");
}

void pushMeasures()
{
    beginFirebase();
    String s = "";
    int x = 0;
    while ((s = getValue(waterMeasures, ',', x++)) != "")
    {
        Serial.println(s);
    }
    waterMeasures = "";
}

bool isPushRequired()
{
    if (millis() - lastPushTime > 60000)
    {
        if (waterMeasures != "") {
            lastPushTime = millis();
            return true;
        }
    }
    return false;
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setupStationMode()
{
    Serial.println("Setting Station mode");
    WiFi.disconnect();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("Connecting to router");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
}

void beginFirebase()
{
    Firebase.begin(HOST, SECRET);
    Firebase.stream(C_PATH);

    if (Firebase.success())
        Serial.println("Success");
}

void loop()
{
    server.handleClient();
    if (isPushRequired())
        pushMeasures();
}