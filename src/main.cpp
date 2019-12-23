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
#include "ScrollingPicture.hpp"

#include "ledmap.hpp"

RemoteDebug Debug;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

CRGB leds[NUM_LEDS];

std::vector< std::vector<CRGB> > ledMatrix(LED_MATRIX_ROWS, std::vector<CRGB>(LED_MATRIX_COLS, CRGB::Black) );

#define N_ANIMATIONS 2 // Number of available animations
RunningDots runningDots(ledMatrix, 5, 2);
ScrollingPicture scrollingPicture(ledMatrix);

bool power_is_on = true;
bool change_image_request = false;
bool change_animation_request = false;
int current_animation = 0;
int framerate = DEFAULT_FRAMES_PER_SECOND;
RgbColor requested_color;

void mqttconnect() {
    Serial.print("MQTT connecting... ");

    // Connect to MQTT, with retained last will message "offline"
    if (mqtt.connect(MQTT_ID, MQTT_USER, MQTT_PWD, MQTT_STATUS_TOPIC, 1, 1, "offline")) {
        Serial.println("MQTT connected");

        // Subscribe to the topics with QoS 1
        mqtt.subscribe(MQTT_COLOR_TOPIC, 1);
        mqtt.subscribe(MQTT_POWER_TOPIC, 1);
        mqtt.subscribe(MQTT_CHNGIMG_TOPIC, 1);
        mqtt.subscribe(MQTT_CHNGANIM_TOPIC, 1);

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

    if ( topic_str.compare(MQTT_CHNGIMG_TOPIC) == 0 ) {
        change_image_request = true;
    }

    if ( topic_str.compare(MQTT_CHNGANIM_TOPIC) == 0 ) {
        change_animation_request = true;
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

    // Load a picture
    scrollingPicture.loadNextBMP();
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

    // Process requests
    if ( change_image_request ) {
        scrollingPicture.loadNextBMP();
        change_image_request = false;
    }

    if ( change_animation_request ) {
        current_animation = current_animation < N_ANIMATIONS-1 ? current_animation+1 : 0;
        change_animation_request = false;
    }

    // Only compute animations if power is set to ON
    if ( power_is_on ) {
        int hue = RgbToHsv(requested_color).h;

        // Compute animation step
        switch ( current_animation ) {
            case 0:
                runningDots.setHue( hue );
                runningDots.nextFrame();
                framerate = FRAMES_PER_SECOND_RUNNINGDOTS;
                break;

            case 1:
                scrollingPicture.nextFrame();
                framerate = FRAMES_PER_SECOND_SCROLLINGPICTURE;
                break;
        }
        
        // Map leds
        for (uint row = 0; row < ledMatrix.size(); row++) {
            for (uint col = 0; col < ledMatrix[row].size(); col++) {
                leds[ ledmap_vertical[row][col] ] = ledMatrix[row][col];
                // leds[ ledmap_vertical[row][col] ] = (row % 2) == 0 ? CRGB::Red : CRGB::Green; // Used to align the columns when installing the LED strips
            }
        }
    }

// Used for testing without a LED strip (displays the last column on the serial port)
/*
    for ( auto row : ledMatrix ) {
        if ( row[14].getLuma() > 10 ){ 
            Serial.print(".");
        }
        else {
            Serial.print(" ");
        }
    }
    Serial.println();
*/
    FastLED.show();

    long neededDelay = (1000 / framerate) - (millis() - loopStartMillis);
    if ( neededDelay > 0 ) {
        FastLED.delay( neededDelay );
    }
}