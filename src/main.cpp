#include <Arduino.h>
#include "config.hpp"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <ArduinoOTA.h>
#include <RemoteDebug.h>
#include <FastLED.h>
#include <vector>

#include "rgbhsv.hpp"

#include "RunningDots.hpp"

#include "ledmap.hpp"

RemoteDebug Debug;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

CRGB leds[NUM_LEDS];

std::vector< std::vector<CRGB> > ledMatrix(LED_MATRIX_ROWS, std::vector<CRGB>(LED_MATRIX_COLS, CRGB::Black) );

RunningDots runningDots(ledMatrix, 5, 2);

bool power_is_on = true;
RgbColor requested_color;

void mqttconnect() {
    Serial.print("MQTT connecting... ");

    // Connect to MQTT, with retained last will message "offline"
    if (mqtt.connect(MQTT_ID, MQTT_USER, MQTT_PWD, MQTT_STATUS_TOPIC, 1, 1, "offline")) {
        Serial.println("MQTT connected");

        // Subscribe to the topics with QoS 1
        mqtt.subscribe(MQTT_COLOR_TOPIC, 1);
        mqtt.subscribe(MQTT_POWER_TOPIC, 1);

        // Update status, message is retained
        mqtt.publish(MQTT_STATUS_TOPIC, "online", true);
    }
    else {
        Serial.print("failed, status code =");
        Serial.print(mqtt.state());
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert topic and payload to strings
    std::string topic_str(topic);
    std::string payload_str;
    for ( uint i = 0; i < length; i++ ) {
        payload_str += (char)payload[i];
    }

    if ( topic_str.compare(MQTT_COLOR_TOPIC) == 0 ) {
        // Convert hex string to integer
        unsigned long color = strtoul(payload_str.substr(1, 6).c_str(), nullptr, 16);
        requested_color.r = color >> 16;
        requested_color.g = color >> 8;
        requested_color.b = color;
    }

    if ( topic_str.compare(MQTT_POWER_TOPIC) == 0 ) {
        if ( payload_str.compare("ON") == 0 ) {
            power_is_on = true;
        }
        else if ( payload_str.compare("OFF") == 0 ){
            power_is_on = false;
        }
    }
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness( DEFAULT_BRIGHTNESS );
    FastLED.setMaxRefreshRate( MAX_REFRESH_RATE ); // Avoids flickering
    FastLED.setCorrection(TypicalLEDStrip);

    WiFi.mode(WIFI_STA);
    WiFi.hostname( WIFI_HOSTNAME );
    WiFi.begin( WIFI_SSID, WIFI_PWD );
    Serial.println("Connecting to WiFi...");
    int wifiResult = WiFi.waitForConnectResult( WIFI_TIMEOUT_MS );
    Serial.print("WiFi connection status = ");
    Serial.println(wifiResult);
    Serial.println("IP: " + WiFi.localIP().toString() );
    
    // Use telnet to read debug messages
    Debug.begin( WIFI_HOSTNAME );
    Debug.setResetCmdEnabled(true);
    Debug.showProfiler(true);
    Debug.showColors(true);

    ArduinoOTA.setPassword( OTA_PWD );
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("OTA update progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA ERROR %u", error);
        });
    ArduinoOTA.begin();

    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // Start with some violet
    requested_color.r = 0x9A;
    requested_color.g = 0x03;
    requested_color.b = 0xAC;
}

void loop() {
    unsigned long loopStartMillis = millis();

    ArduinoOTA.handle();
    Debug.handle();

    if ( WiFi.status() == WL_CONNECTED && !mqtt.connected() ) {
        mqttconnect();
    }
    mqtt.loop();

    // Reset all LEDs
    for (uint i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }

    // Only compute animations if power is set to ON
    if ( power_is_on ) {
        // Compute animation step
        int hue = RgbToHsv(requested_color).h;
        runningDots.setHue( hue );
        runningDots.nextFrame();

        // Map leds
        for (uint row = 0; row < ledMatrix.size(); row++) {
            for (uint col = 0; col < ledMatrix[row].size(); col++) {
                leds[ ledmap_vertical[row][col] ] = ledMatrix[row][col];
                // leds[ ledmap_vertical[row][col] ] = (row % 2) == 0 ? CRGB::Red : CRGB::Green; // Used to align the columns when installing the LED strips
            }
        }
    }
    
    FastLED.show();

    long neededDelay = (1000 / FRAMES_PER_SECOND) - (millis() - loopStartMillis);
    if ( neededDelay > 0 ) {
        FastLED.delay( neededDelay );
    }
}