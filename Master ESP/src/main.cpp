#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define WIFI_SSID "Gajera's"
#define WIFI_PASS "Gajera@037"

#define SOFT_SSID "master"
#define SOFT_PASS "master123"

#define SECRET "SdLwnMZTXgsbXlvJTiv4jvcDIQOZuA7KQ8TOOlZ8"
#define HOST "mini-project-2-9b3c3.firebaseio.com"

const char *C_PATH = "/controllers/Ln6zD6LbMOPGzSVzq2j";

ESP8266WebServer server(80);

String waterMeasures = "";
long lastPushTime = 0;
int moistureToMaintain = 0;

String sensorToValveMap[3][2] = {
    {"sensor1", "v1"},
    {"sensor2", "v2"},
    {"sensor3", "v3"}};

int valveStatus[3];
int prevValveStatus[3];

int sensorMeasures[3];

struct Sensor
{
    String id;
    short measure;
} s;

void setupAccessPoint();
void setupServer();
void handleRequest();
void setupStationMode();
void beginFirebase();
void pushMeasures();
bool isPushRequired();
bool getNextMeasure();
short getShort(String s);
void resetMoistureToMaintain();

void setup()
{
    Serial.begin(9600);
    WiFi.mode(WIFI_AP_STA);
    lastPushTime = millis();
    resetMoistureToMaintain();
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
    Serial.println("\nStarting server");
    server.on("/", handleRequest);
    server.begin();
}

void setupStationMode()
{
    Serial.println("Setting Station mode");
    WiFi.disconnect();

    WiFi.hostname("ESP8266 Dev");
    IPAddress staticIP(192, 168, 1, 90);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 1, 1);

    WiFi.config(staticIP, gateway, subnet, dns);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
}

int getIndexForID(String sid)
{
    for (int i = 0; i < 3; i++)
    {
        if (sid.equalsIgnoreCase(sensorToValveMap[i][0]))
        {
            return i;
        }
    }
    return -1;
}

void handleRequest()
{
    int index = getIndexForID(server.arg("id"));
    if (index == -1)
    {
        server.send(200, "text/http", "Invalid server id");
        return;
    }

    short m = getShort(server.arg("m"));
    if (m < moistureToMaintain - 5 && valveStatus[index] == 0)
    {
        Serial.print("Set valve " + sensorToValveMap[index][1] + " to on");
        valveStatus[index] = 1;
    }
    else if (valveStatus[index] == 1 && m >= moistureToMaintain + 5)
    {
        Serial.print("Set valve " + sensorToValveMap[index][1] + " to off");
        valveStatus[index] = 0;
    }

    waterMeasures += server.arg("id") + ":" + server.arg("m") + ",";
    server.send(200, "text/http", "recorder");
}

void pushMeasures()
{
    beginFirebase();
    setupStationMode();

    while (getNextMeasure())
    {
        int valveIndex = getIndexForID(s.id);
        
        String path = "/valves/" + sensorToValveMap[valveIndex][1] + "/moisture_measures/" + s.id;
        Firebase.setInt(C_PATH + path, s.measure);

        path = "/valves/" + sensorToValveMap[valveIndex][1] + "/status";
        if (valveStatus[valveIndex] != prevValveStatus[valveIndex]) 
            Firebase.setInt(C_PATH + path, valveStatus[valveIndex]);

        if (Firebase.failed())
        {
            Serial.println("Error uploading data");
            return;
        }
        prevValveStatus[valveIndex] = valveStatus[valveIndex]; 
        delay(100);
    }
    waterMeasures = "";

    setupAccessPoint();
}

bool isPushRequired()
{
    if (millis() - lastPushTime > 60000)
    {
        if (waterMeasures != "")
        {
            lastPushTime = millis();
            return true;
        }
    }
    return false;
}

int lastSeparatorIndex = 0;

bool getNextMeasure()
{
    int strIndex[2] = {0, -1};
    int splitIndex = 0;
    int maxIndex = waterMeasures.length() - 1;
    char separator = ',';

    if (lastSeparatorIndex == maxIndex)
    {
        lastSeparatorIndex = 0;
        return false;
    }

    if (lastSeparatorIndex != 0)
        lastSeparatorIndex++;

    char c;
    for (int i = lastSeparatorIndex; i <= maxIndex; i++)
    {
        c = waterMeasures.charAt(i);
        if (c == separator)
        {
            strIndex[0] = lastSeparatorIndex;
            strIndex[1] = i - 1;
            lastSeparatorIndex = i;
            break;
        }
        if (c == ':')
            splitIndex = i;
    }

    s.id = waterMeasures.substring(strIndex[0], splitIndex);
    s.measure = getShort(waterMeasures.substring(splitIndex + 1, strIndex[1] + 1));
    return true;
}

short getShort(String s)
{
    short no = 0;
    for (unsigned int i = 0; i < s.length(); i++)
    {
        no = no * 10 + s.charAt(i) - '0';
    }
    return no;
}

void beginFirebase()
{
    Firebase.begin(HOST, SECRET);
    Firebase.stream(C_PATH);

    if (Firebase.success())
        Serial.println("Success");
}

long lastResetTime = 0;
bool isADayPassed()
{
    if (millis() - lastResetTime > 60000 * 60 * 24)
    {
        lastResetTime = millis();
        return true;
    }
    return false;
}

void resetMoistureToMaintain()
{
    setupStationMode();
    beginFirebase();

    String loc = "/moisture_to_maintain";
    moistureToMaintain = Firebase.getInt(C_PATH + loc);
    Serial.println(moistureToMaintain);
    setupAccessPoint();
}

void loop()
{
    server.handleClient();

    if (isPushRequired())
        pushMeasures();

    if (isADayPassed())
        resetMoistureToMaintain();
}