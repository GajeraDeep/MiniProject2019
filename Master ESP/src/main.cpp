#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

#define STA_SSID "Gajera's"
#define STA_PASS "Gajera@037"
#define AP_SSID "master"
#define AP_PASS "master123"
#define SECRET "SdLwnMZTXgsbXlvJTiv4jvcDIQOZuA7KQ8TOOlZ8"
#define HOST "mini-project-2-9b3c3.firebaseio.com"
#define rootPath "/controllers/Ln6zD6LbMOPGzSVzq2j"
#define pl(x) Serial.print(x)
#define pln(x) Serial.println(x);

WiFiClientSecure client;
ESP8266WebServer server(80);

int port = 443;
const char *host = "www.darksky.net";
const char fingerprint[] PROGMEM = "EA C3 0B 36 E8 23 4D 15 12 31 9B CE 08 51 27 AE C1 1D 67 2B";

FirebaseData firebaseData;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org"); //, 330000);

int moistureValues[3];
int valveMapping[3];
int requiredMoisture;
long lastPushTime;
bool valvesState[3];
bool isPumpOn = false;
bool isNewDataAvail = false;
bool valvesState_PreValues[3];
bool isPumpOn_PreValue = false;
long rainPossiRecordedTime = 0;
float rainProbability = 0;
bool waitingForRain[3];

float getProbabilityOfRain()
{
    int hours = 2;

    const size_t capacity1 = JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(18) + 310;
    StaticJsonDocument<capacity1> doc;

    if (client.connect(host, port))
    {
        long timestamp = timeClient.getEpochTime() + 3600000 * hours;
        client.print(String("GET ") + "/forecast/9a41ed7b8a1a89491606776763204cb3/23.0395302,72.6742452," + timestamp + "?exclude=minutely,hourly,daily,alerts,flags" + " HTTP/1.1\r\n" +
                     "Host: api.darksky.net" + "\r\n" +
                     "Connection: close\r\n\r\n");
        while (client.connected() || client.available())
        {
            if (client.available())
            {
                String line = client.readString();
                String jsonData = line.substring(line.indexOf('{'));
                deserializeJson(doc, jsonData);
                JsonObject currently = doc["currently"];
                rainPossiRecordedTime = millis();
                rainProbability = (float)currently["precipProbability"];

                return rainProbability;
            }
        }
        client.stop();
        return -1;
    }
    else
    {
        pln("Failed to connect to DarkNet API");
    }

    return 0;
}

bool waitForRain(int sensor)
{
    if (waitingForRain[sensor])
        return true;
    if (millis() - rainPossiRecordedTime < 3600000 * 2)
    {
        waitingForRain[sensor] = true;
    }
    return false;
}

void updateFirebaseValue(String relativePath, String key, int value)
{
    FirebaseJson updateData;
    updateData.addInt(key, value);
    String path = rootPath + relativePath;
    if (!Firebase.updateNodeSilent(firebaseData, path, updateData))
    {
        pln("Upload Failed");
        pln(firebaseData.errorReason());
    }
}

void pushNewData()
{
    for (int i = 0; i < 3; i++)
    {
        String path = String("/moistureMeasures/v") + String(valveMapping[i] + 1);
        String key = String("sensor") + String(i + 1);
        pl(path);
        pl(" ");
        pln(key);
        updateFirebaseValue(path, key, moistureValues[i]);
    }

    for (int i = 0; i < 3; i++)
    {
        if (valvesState[i] != valvesState_PreValues[i])
            updateFirebaseValue("/valvesState", String("v") + String(i + 1), valvesState[i] ? 1 : 0);
        valvesState_PreValues[i] = valvesState[i];
    }

    if (isPumpOn != isPumpOn_PreValue)
        updateFirebaseValue("", "pump", isPumpOn ? 1 : 0);
    isPumpOn_PreValue = isPumpOn;

    isNewDataAvail = false;
}

void streamCallback(StreamData data)
{
    pln("Stream Data...");
    pln(data.streamPath());
    pln(data.dataPath());
    pln(data.dataType());
}

void streamTimeoutCallback(bool timeout)
{
}

void setValveMapping()
{
    const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + 40;
    DynamicJsonDocument doc(capacity);

    if (Firebase.getJSON(firebaseData, rootPath + String("/moistureMeasures")))
    {
        deserializeJson(doc, firebaseData.jsonData());
        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair jp : obj)
        {
            for (JsonPair sjp : jp.value().as<JsonObject>())
            {
                int key = String(sjp.key().c_str()).charAt(6) - '0' - 1;
                int value = String(jp.key().c_str()).charAt(1) - '0' - 1;
                pl(key);
                pl(" ");
                pln(value);
                valveMapping[key] = value;
            }
        }
    }
}

void resetPumpState()
{
    for (int i = 0; i < 3; i++)
    {
        if (valvesState[i] && !isPumpOn)
        {
            pln("Pump is on");
            isPumpOn = true;
            return;
        }
    }

    if (isPumpOn)
        pln("Pump is off");
    isPumpOn = false;
}

void handleRequest()
{
    if (!server.hasArg("plain"))
    {
        server.send(200, "text/plain", "No data received");
        return;
    }

    server.send(200, "text/plain", "recorded");
    String jsonData = server.arg("plain");

    const size_t size = JSON_OBJECT_SIZE(1) + 20;
    StaticJsonDocument<size> doc;
    deserializeJson(doc, jsonData);

    JsonObject docObj = doc.as<JsonObject>();

    for (JsonPair jp : docObj)
    {
        isNewDataAvail = true;

        int index = String(jp.key().c_str()).toInt() - 1;
        int value = jp.value().as<int>();
        moistureValues[index] = value;

        pl("New Entry: ");
        pl(index);
        pl(" ");
        pln(value);

        int valveNo = valveMapping[index];

        if (value < requiredMoisture && !valvesState[valveNo])
        {
            valvesState[valveNo] = true;
        }
        else if (value >= requiredMoisture && valvesState[valveNo])
        {
            valvesState[valveNo] = false;
        }
        else
        {
            return;
        }
        pl("Valve ");
        pl(index);
        pl(" is set to ");
        pln(valvesState[valveNo]);
        resetPumpState();
    }
}

void fetchRequiredMoisture()
{
    if (Firebase.getInt(firebaseData, rootPath + String("/req_moisture")))
    {
        requiredMoisture = firebaseData.intData();
        pl("Moisture; ");
        pln(requiredMoisture);
    }
    else
    {
        pln("Req. Moisture request failed");
        pln(firebaseData.errorReason());
    }
}

bool isPushRequired()
{
    if (millis() - lastPushTime > 60000)
    {
        if (isNewDataAvail)
        {
            lastPushTime = millis();
            isNewDataAvail = false;
            return true;
        }
    }
    return false;
}

void setup()
{
    Serial.begin(9600);
    WiFi.mode(WIFI_AP_STA);

    pln("Setting AP");
    String res = WiFi.softAP(AP_SSID, AP_PASS) ? "Ready" : "Failed";
    pln(res);

    pln("Connecting to wifi");
    WiFi.begin(STA_SSID, STA_PASS);

    while (WiFi.status() != WL_CONNECTED)
    {
        pl(".");
        delay(500);
    }

    pln("\nConnected with IP: ");
    pln(WiFi.localIP());

    client.setFingerprint(fingerprint);
    client.setTimeout(15000);
    delay(1000);

    lastPushTime = millis();

    timeClient.begin();
    timeClient.update();

    Firebase.begin(HOST, SECRET);
    Firebase.setStreamCallback(firebaseData, streamCallback, streamTimeoutCallback);

    // if (!Firebase.beginStream(firebaseData, "/controllers/Ln6zD6LbMOPGzSVzq2j"))
    // {
    // 	pln(firebaseData.errorReason());
    // }

    pln("Setting mapping");
    setValveMapping();
    fetchRequiredMoisture();

    server.on("/values", handleRequest);
    server.begin();
}

void loop()
{
    if (isPushRequired())
    {
        pushNewData();
    }

    server.handleClient();
}
