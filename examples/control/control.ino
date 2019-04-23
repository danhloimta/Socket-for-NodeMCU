#include <SocketIOClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define POWER D3
#define LED_PIN D4

const char* ssid = "Tang-4";
const char* password = "!@#$%^&*o9";

const char HexLookup[17] = "0123456789ABCDEF";

String host = "iot-farm.vn";
int port = 3000;
uint8_t ledState = 0;
uint8_t r = 0;
uint8_t g = 0;
uint8_t b = 0;
uint8_t ra = 0;
uint8_t ga = 0;
uint8_t ba = 0;

SocketIOClient socket;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

void setupNetwork() {
    WiFi.begin(ssid, password);
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
    if (i == 21) {
        while (1) delay(500);
    }
}

void setLed() {
    if (ledState) {
        ra = r;
        ga = g;
        ba = b;
    } else {
        ra = 0;
        ga = 0;
        ba = 0;
    }

    pixels.setPixelColor(0, pixels.Color(ra, ga, ba));
    pixels.show();
}

void emitStatus() {
    String emitString = "{\"status\":";
    emitString += ledState;
    emitString += ",\"color\":{\"r\":";
    emitString += r;
    emitString += ",\"g\":";
    emitString += g;
    emitString += ",\"b\":";
    emitString += b;
    emitString += ",\"a\":1}}";

    Serial.println(emitString);
    socket.emit("led-status", emitString);
}

void changeLedState(String data) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonArray& root = jsonBuffer.parse(data);
    if (!root.success()) {
        Serial.println("Parse json failed");
        return;
    }

    Serial.println("[changeLed] " + data);
    r = root[1]["color"]["r"];
    g = root[1]["color"]["g"];
    b = root[1]["color"]["b"];
    ledState = root[1]["status"];

    Serial.print("[changeLed] status: ");
    Serial.println(ledState);
    Serial.print("[changeLed] r: ");
    Serial.println(r);
    Serial.print("[changeLed] g: ");
    Serial.println(g);
    Serial.print("[changeLed] b: ");
    Serial.println(b);
    setLed();
    emitStatus();
}

void setup() {

    pinMode(LED_PIN, OUTPUT);

    Serial.begin(115200);

    setupNetwork();

    // This initializes the NeoPixel library.
    pixels.begin();

    // socket.setChannel("feed-the-fish");
    socket.on("led-change", changeLedState);
    socket.connect(host, port);
    emitStatus();
}

void loop() {
    socket.monitor();
}
