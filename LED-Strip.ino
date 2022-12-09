#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "ESP8266mDNS.h"
#include "FastLED.h"
#include "ArduinoJson.h"
#include "ESP8266HTTPClient.h"

#define NETWORK "Lydon"
#define PASSWORD "20221209"
#define NUMBER_LEDS 150
#define LED_PIN D1

WiFiClient WLAN_Client;
ESP8266WebServer server(80);
CRGB Leds[NUMBER_LEDS];
boolean blockedLeds[NUMBER_LEDS];

uint8_t Modus = 3;
unsigned long LoopTimer = 0;
uint8_t Brightness = 32;
CRGB Color = CRGB(50, 50, 50);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.print("Connect with WLAN: ");
  Serial.println(NETWORK);

  delay(100);
  WiFi.begin(NETWORK, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //Serial.print(WiFi.status());
    //WiFi.printDiag(Serial);
  }
  Serial.println("");
  Serial.print("Successfully connected. Own IP address: ");
  Serial.println(WiFi.localIP());

  registerController();

  LEDS.addLeds<WS2812, LED_PIN, GRB>(Leds, NUMBER_LEDS);
  LEDS.setBrightness(Brightness);
  unblockLeds();

  // Activate mDNS this is used to be able to connect to the server
  // with local DNS hostmane ledstrip.local
  if (MDNS.begin("ledstrip")) {
    Serial.println("MDNS responder started: ledstrip[.local]");
  }

  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(NETWORK, PASSWORD);
    Serial.println("WLAN connection lost.");
    delay(1000);
  }

  server.handleClient();

  if (millis() > LoopTimer) {
    switch (Modus) {
      case 0:  // Off
        fadeToBlackBy(Leds, NUMBER_LEDS, 5);
        LoopTimer = millis() + 50;
        break;

      case 1:  // constant color
        for (uint8_t i = 1; i < NUMBER_LEDS; i++) {
          if (!blockedLeds[i]) Leds[i] = Color;
        }
        LoopTimer = millis() + 500;
        break;

      case 2:  // waves
        for (uint8_t i = 1; i < NUMBER_LEDS; i++) {
          if (!blockedLeds[i]) {
            byte Faktor = sin8(i + millis() / 50);
            Leds[i] = CRGB(map(Color.red, 0, 255, 0, Faktor),
                           map(Color.green, 0, 255, 0, Faktor),
                           map(Color.blue, 0, 255, 0, Faktor));
          }
        }
        LoopTimer = millis() + 50;
        break;

      case 3:  // rainbow
        for (uint8_t i = 1; i < NUMBER_LEDS; i++) {
          if (!blockedLeds[i]) {
            Leds[i] = CHSV(millis() / 50 + i, 255, 255);
          }
        }
        LoopTimer = millis() + 50;
        break;
    }
    FastLED.show();
  }
}

void restServerRouting() {
  server.on("/", HTTP_GET, []() {
    server.send(200, F("text/html"), F("Welcome to the X-MAS tree LED-strip REST-API."));
  });

  server.on(F("/leds"), HTTP_GET, getLedStatus);

  server.on(F("/modus"), HTTP_PUT, setModus);

  server.on(F("/brightness"), HTTP_PUT, setBrightness);

  server.on(F("/leds"), HTTP_PUT, setLedColor);

  server.on(F("/unblockleds"), HTTP_PUT, setUnblockLeds);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri();
  message += "\nMethod: " + server.method();
  message += "\nArguments: " + server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void unblockLeds() {
  for (uint8_t i = 1; i < NUMBER_LEDS; i++) blockedLeds[i] = false;
}

void getLedStatus() {
  DynamicJsonDocument doc(12400);

  doc["modus"] = Modus;
  doc["brightness"] = Brightness;
  JsonArray LEDS = doc.createNestedArray("leds");
  for (uint8_t i = 1; i <= NUMBER_LEDS; i++) {
    if (blockedLeds[i]) {
      JsonObject led = LEDS.createNestedObject();
      led["led"] = i;
      led["r"] = Leds[i].red;
      led["g"] = Leds[i].green;
      led["b"] = Leds[i].blue;
    }
  }

  sendResponse200(doc);
}

void setModus() {
  String body = server.arg("plain");
  Serial.println(body);

  DynamicJsonDocument doc(32);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    sendResponse400(error);
  } else {
    JsonObject bodyObj = doc.as<JsonObject>();

    if (server.method() == HTTP_PUT) {
      if (bodyObj.containsKey("modus")) {
        Modus = bodyObj["modus"];
        unblockLeds();

        DynamicJsonDocument doc(16);
        doc["modus"] = Modus;

        sendResponse200(doc);
      } else {
        sendErrorIncorrectRequestBody();
      }
    }
  }
}

void setBrightness() {
  String body = server.arg("plain");
  Serial.println(body);

  DynamicJsonDocument doc(32);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    sendResponse400(error);
  } else {
    JsonObject bodyObj = doc.as<JsonObject>();

    if (server.method() == HTTP_PUT) {
      if (bodyObj.containsKey("brightness")) {
        Brightness = bodyObj["brightness"];
        LEDS.setBrightness(Brightness);

        DynamicJsonDocument doc(16);
        doc["brightness"] = Brightness;

        sendResponse200(doc);
      } else {
        sendErrorIncorrectRequestBody();
      }
    }
  }
}

void setLedColor() {
  String body = server.arg("plain");
  Serial.println(body);

  DynamicJsonDocument doc(12400);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    sendResponse400(error);
  } else {
    JsonObject bodyObj = doc.as<JsonObject>();

    if (server.method() == HTTP_PUT) {
      if (bodyObj.containsKey("leds")) {
        for (const auto& led : bodyObj["leds"].as<JsonArray>()) {
          JsonObject ledOjb = led.as<JsonObject>();
          if (ledOjb.containsKey("led")) {
            uint8_t iLed = ledOjb["led"].as<uint8_t>();
            blockedLeds[iLed] = true;
            Leds[iLed] = CRGB(
              ledOjb.containsKey("r") ? ledOjb["r"].as<uint8_t>() : Color.red,
              ledOjb.containsKey("g") ? ledOjb["g"].as<uint8_t>() : Color.green,
              ledOjb.containsKey("b") ? ledOjb["b"].as<uint8_t>() : Color.blue);
          }
        }
        getLedStatus();
      } else {
        sendErrorIncorrectRequestBody();
      }
    }
  }
}

void setUnblockLeds() {
  String body = server.arg("plain");
  Serial.println(body);

  DynamicJsonDocument doc(32);
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    sendResponse400(error);
  } else {
    if (server.method() == HTTP_PUT) {
      unblockLeds();

      DynamicJsonDocument doc(16);
      sendResponse200(doc);
    } else {
      sendErrorIncorrectRequestBody();
    }
  }
}

void sendErrorIncorrectRequestBody() {
  DynamicJsonDocument doc(96);
  doc["status"] = "Error";
  doc["message"] = F("No data found or incorrect!");
  String buf;
  serializeJson(doc, buf);
  server.send(400, F("application/json"), buf);
}

void sendResponse200(DynamicJsonDocument doc) {
  String buf;
  serializeJson(doc, buf);
  server.send(200, F("application/json"), buf);
}

void sendResponse400(DeserializationError error) {
  // if the file didn't open, print an error:
  Serial.print(F("Error while parsing JSON"));
  Serial.println(error.c_str());

  String msg = error.c_str();

  server.send(400, F("text/html"), "Error while parsing json body! <br>" + msg);
}

void registerController() {
  HTTPClient httpClient;

  Serial.print("HTTP -> begin...\n");
  if (httpClient.begin(WLAN_Client, "http://192.168.0.99:8080/register")) {
    /*
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("api-key", "wf19sEt..........fOBWhP8Q");
*/
    String payload = "{}";
    int httpCode = httpClient.POST(payload);

    if (httpCode > 0) {
      Serial.printf("HTTP -> Code: %d\n", httpCode);
    } else {
      Serial.printf("HTTP -> GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
    }

    httpClient.end();
  } else {
    Serial.printf("HTTP -> Unable to connect\n");
  }
}
