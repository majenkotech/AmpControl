#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#pragma board esp8266-d1-mini-pro

#define USER "mqttusername"
#define PASS "mqttpassword"
#define LOCATION "office"
#define UNIT "mixer"
#define DEVICE "Mixer"
#define OTAPASS "otapassword"

//#define DEBUG

ESP8266WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient client(espClient);

#ifdef DEBUG

#define DebugPrint(...) Serial.print(__VA_ARGS__)
#define DebugPrintln(...) Serial.println(__VA_ARGS__)
#define DebugBegin(...) Serial.begin(__VA_ARGS__)

#else

#define DebugPrint(...) 
#define DebugPrintln(...) 
#define DebugBegin(...)

#endif
#define SDO D7
#define SCK D6
#define CS D5

int vol[5] = {0, 0, 0, 0, 0};
int configured[5] = {0, 0, 0, 0, 0};

int wifiConnected = 0;
int mqttConnected = 0;

void callback(char *topic, byte *payload, unsigned int length) {

    char *user = strtok(topic, "/");
    char *location = strtok(NULL, "/");
    char *unit = strtok(NULL, "/");
    char *channel = strtok(NULL, "/");
    char *control = strtok(NULL, "/");
    char *operation = strtok(NULL, "/");

    if (!operation) return;
    if (!isdigit(channel[0])) return;

    int val = atoi((char *)payload);


    DebugPrint("User: "); DebugPrintln(user);
    DebugPrint("Location: "); DebugPrintln(location);
    DebugPrint("Unit: "); DebugPrintln(unit);
    DebugPrint("Channel: "); DebugPrintln(channel);
    DebugPrint("Control: "); DebugPrintln(control);
    DebugPrint("Operation: "); DebugPrintln(operation);

    if (!strcmp(user, USER)) {
        if (!strcmp(location, LOCATION)) {
            if (!strcmp(unit, UNIT)) {
                int c = atoi(channel);

                if ((c < 0) || (c > 4)) return;
                
                if (!strcmp(operation, "set")) {
                    configured[c] = 1;
                    vol[c] = val;
                    if (vol[c] < 0) vol[c] = 0;
                    if (vol[c] > 100) vol[c] = 100;
                    setVolumes();
                    char t[30];
                    char v[5];
                    sprintf(t, USER "/" LOCATION "/" UNIT "/%d/vol/get", c, true);
                    sprintf(v, "%d", vol[c]);
                    client.publish(t, v, true);                
                } else if (!strcmp(operation, "adj")) {
                    vol[c] += val;
                    if (vol[c] < 0) vol[c] = 0;
                    if (vol[c] > 100) vol[c] = 100;
//                    setVolumes();
                    char t[30];
                    char v[5];
                    sprintf(t, USER "/" LOCATION "/" UNIT "/%d/vol/set", c, true);
                    sprintf(v, "%d", vol[c]);
                    client.publish(t, v, true);
//                    sprintf(t, USER "/" LOCATION "/" UNIT "/%d/vol/get", c, true);
//                    sprintf(v, "%d", vol[c]);
//                    client.publish(t, v, true);                
                }                
            }
        }
    }
}

void transfer(uint8_t v) {
    shiftOut(SDO, SCK, MSBFIRST, v);
}

void setVolumes() {
    uint8_t cmd = 0b00010000;

    digitalWrite(CS, LOW);

    for (int i = 0; i < 5; i++) {
        transfer(cmd | 0b11);
        transfer(vol2res(vol[i]));
    }

    digitalWrite(CS, HIGH);
}

uint8_t vol2res(int v) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return map(v, 0, 100, 255, 0);
}

void setup() {
    DebugBegin(115200);
    
    WiFi.mode(WIFI_STA);

    pinMode(SCK, OUTPUT);
    pinMode(SDO, OUTPUT);
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);

    client.setServer("mqtt.server.com", 1883);
    client.setCallback(callback);

    wifiMulti.addAP("SSID", "PSK");
    
    ArduinoOTA.setHostname(DEVICE);
    ArduinoOTA.setPassword(OTAPASS);
    ArduinoOTA.begin(); 
    delay(1000);
    DebugPrintln("System ready");
}

void loop() {
    static uint32_t ts = millis();
    if (wifiMulti.run() == WL_CONNECTED) {
        if (!wifiConnected) {
            DebugPrintln("WiFi Connected");
            wifiConnected = 1;
        }
        if (!client.connected()) {
            if (mqttConnected) {
                DebugPrintln("MQTT Disconnected");
                mqttConnected = 0;
            }
            delay(100);
            if (client.connect(DEVICE, USER, PASS)) {
                DebugPrintln("MQTT Connected");
                mqttConnected = 1;
                client.subscribe(USER "/" LOCATION "/" UNIT "/0/vol/set");
                client.subscribe(USER "/" LOCATION "/" UNIT "/0/vol/adj");
                
                client.subscribe(USER "/" LOCATION "/" UNIT "/1/vol/set");
                client.subscribe(USER "/" LOCATION "/" UNIT "/1/vol/adj");
                
                client.subscribe(USER "/" LOCATION "/" UNIT "/2/vol/set");
                client.subscribe(USER "/" LOCATION "/" UNIT "/2/vol/adj");
                
                client.subscribe(USER "/" LOCATION "/" UNIT "/3/vol/set");
                client.subscribe(USER "/" LOCATION "/" UNIT "/3/vol/adj");
                
                client.subscribe(USER "/" LOCATION "/" UNIT "/4/vol/set");
                client.subscribe(USER "/" LOCATION "/" UNIT "/4/vol/adj");
                
            }

        } else {
            if (millis() - ts >= 5000) {
                ts = millis();

                for (int i = 0; i < 5; i++) {
                    if (!configured[i]) continue;
                    char t[30];
                    char v[5];
                    sprintf(t, USER "/" LOCATION "/" UNIT "/%d/vol/get", i, true);
                    sprintf(v, "%d", vol[i]);
                    client.publish(t, v, true);                    
                    DebugPrint(t); DebugPrint("="); DebugPrintln(v);
                }
            }
        }
        client.loop();
    } else {
        if (wifiConnected) {
            wifiConnected = 0;
            DebugPrintln("WiFi Disconnected");
        }
    }
}
