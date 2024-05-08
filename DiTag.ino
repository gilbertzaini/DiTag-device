#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define DEVICE_ID "D0001"
#define RXPin 16
#define TXPin 17
#define buzzerPin 26
#define GPSBaud 9600

// The TinyGPSPlus object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

// Wi-Fi setup
const char *ssid = "nando";
const char *password = "satuduatiga";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "DiTagCommunicationProtocolMQTTBuzzer";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// API URL
String apiUrl = "https://api.punca.my.id/coordinate/";

// Timer delay
unsigned long timerDelay = 0;

// firstboot flag
int firstboot = 0;

void setup() {
  // serial setup
  Serial.begin(115200);
  // gps serial setup
  ss.begin(GPSBaud);

  // buzzer setup
  pinMode(buzzerPin, OUTPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.print("Connected to WiFi: ");
  Serial.println(ssid);

  // add device id to url
  apiUrl += DEVICE_ID;

  //connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  // Publish and subscribe
  client.publish(topic, "Hi, I'm ESP32 ^^");
  client.subscribe(topic);
  Serial.print(topic);
  Serial.println("subscribed");


  pinMode(LED_BUILTIN, OUTPUT);
  // Serial.println(apiUrl);
}

void loop() {
  client.loop();

  // Subscribe to the topic
  if (!client.connected()) {
    MQTTConnect();
  }

  client.subscribe(topic);
  
  blinkLED();

  float latitude = -6.248348068393909;
  float longitude = 106.62280329846074;

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  } else {
    Serial.println("GPS not connected");
  }

  updateCoordinate(latitude, longitude);

  // ringBuzzer();

  // delay
  // delay(timerDelay);

  if (firstboot == 1) {
    timerDelay = 180000;  // 3 minutes delay
    firstboot = 0;
    Serial.print("timerDelay updated: ");
    Serial.println(timerDelay);
  }
}

void updateCoordinate(float latitude, float longitude) {
  // Initialize HTTPClient
  HTTPClient http;
  http.setTimeout(1000);

  // Make a PATCH request
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");

  // Create a JSON payload with latitude and longitude
  String patchData = "{\"latitude\":" + String(latitude, 6) + ",\"longitude\":" + String(longitude, 6) + "}";

  int httpCode = http.PATCH(patchData);
  if (httpCode > 0) {
    // HTTP request was successful
    Serial.printf("HTTP Code: %d\n", httpCode);

    // Read the response
    String payload = http.getString();
    Serial.println("Response: " + payload);
  } else {
    Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
  }

  // Close the connection
  http.end();
}

void ringBuzzer() {
  tone(buzzerPin, 1000);  // Send 1KHz sound signal...
  delay(5000);            // ...for 1 sec
  return;
}

void muteBuzzer() {
  noTone(buzzerPin);
  return;
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Parse the JSON payload
  const size_t capacity = JSON_OBJECT_SIZE(1) + 20; // Adjust the capacity based on your JSON structure
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, payload);

  // Extract the value associated with the "msg" key
  String receivedMessage = doc["msg"];
  receivedMessage.trim();
  Serial.print("receivedMessage: ");
  Serial.println(receivedMessage);

  String ringString = "Ring " + String(DEVICE_ID);
  String muteString = "Mute " + String(DEVICE_ID);

  // Compare the received message
  if (receivedMessage.equals(ringString)) {
    Serial.println("Ringing...");
    ringBuzzer();
  } else if (receivedMessage.equals(muteString)) {
    Serial.println("Muting...");
    muteBuzzer();
  } else {
    Serial.println("Neither");
  }
}

void MQTTConnect() {
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());

    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
      client.subscribe(topic);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void blinkLED() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
