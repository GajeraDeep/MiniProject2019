#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#define SENSOR_ID "2"

const char *ssid = "master";
const char *password = "master123";

String serverHost = "192.168.4.1";
String data;

int sleepInterval = 1;

int failConnectRetryInterval = 1;
int counter = 0;

const size_t cap = JSON_OBJECT_SIZE(1) + 10;

float h;
float t;
long value;

void sendHttpRequest();
void buildDataStream();
void hibernate(double pInterval);

IPAddress ip(192, 168, 4, 4);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiClient client;

void sendData()
{
    WiFiClient client;
    if (client.connect(serverHost, 80))
    {
        client.println("POST /values  HTTP/1.1");
        client.println("Host: server_name");
        client.println("Accept: */*");
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(data.length());
        client.println();
        client.print(data);

        unsigned long timeout = millis();
        while (client.available() == 0)
        {
            if (millis() - timeout > 5000)
            {
                Serial.println(">>> Client Timeout !");
                client.stop();
                return;
            }
        }

        while (client.available())
        {
            String line = client.readStringUntil('\r');
            Serial.print(line);
        }
    }
    else
    {
        Serial.println("Failed to connect");
    }
}

void buildDataStream()
{
    StaticJsonDocument<cap> doc;
    doc[SENSOR_ID] = value;
    serializeJson(doc, data);
    Serial.println("Data stream: " + data);
}

void hibernate(double pInterval)
{
    WiFi.disconnect();
    ESP.deepSleep(10 * 600000 * pInterval, WAKE_RFCAL);
    delay(100);
}

void setup()
{
    Serial.begin(9600);
    // Serial.println("dasdasd");

    // system_rtc_mem_read(65, &value, sizeof(long));
    // Serial.println("Read" + value);
    // Serial.flush();

    // long newValue = map(analogRead(A0), 0, 1024, 100, 0);
    // if ((long)value == newValue)
    // {
    //     hibernate(sleepInterval);
    // }

    // value = newValue;
    // system_rtc_mem_write(65, &value, sizeof(long));

    value = map(analogRead(A0), 0, 1024, 100, 0);

    WiFi.persistent(false);

    delay(500);
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to wifi");
    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        if (counter > 20)
        {
            Serial.println("Can't connect, going to sleep");
            hibernate(failConnectRetryInterval);
        }
        delay(500);
        Serial.print(".");
        counter++;
    }

    Serial.println("Wifi connected");

    buildDataStream();

    sendData();

    hibernate(sleepInterval);
}

void loop() {}