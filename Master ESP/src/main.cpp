#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>

#define WIFI_SSID "Gajera's"
#define WIFI_PASS "Gajera@037"

void setup() {
    Serial.begin(9600);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("connecting");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("connected: ");
    Serial.println(WiFi.localIP());
}

void loop() { 
	
}