#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define S_ID "sensor1"

const char *ssid = "master";
const char *password = "master123";

String serverHost = "192.168.4.1";
String data;

int sleepInterval = 1;

int failConnectRetryInterval = 1;
int counter = 0;

float h;
float t;

void sendHttpRequest();
void buildDataStream();
void hibernate(double pInterval);

IPAddress ip(192, 168, 4, 4);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiClient client;

void setup()
{
    ESP.eraseConfig();
    WiFi.persistent(false);
    Serial.begin(9600);

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

    sendHttpRequest();

    hibernate(sleepInterval);
}

void sendHttpRequest()
{
    if (client.connect(serverHost, 80))
    {
        String getStr = data;

        client.print(String("GET ") + data + " HTTP/1.1\r\n" + "Host: " + serverHost + "\r\n" + "Connection: close\r\n\r\n");

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
        Serial.println();
        Serial.println("closing connection");
    }
    else
    {
        Serial.println("Error connecting");
    }
    client.stop();
}

void buildDataStream()
{
    data = "/?id=";
    data += S_ID;
    data += "&m=";
    data += map(analogRead(A0), 0, 1024, 100, 0);
    Serial.println("Data stream: " + data);
}

void hibernate(double pInterval)
{
    WiFi.disconnect();
    ESP.deepSleep(10 * 600000 * pInterval, WAKE_RFCAL);
    delay(100);
}

void loop() {}