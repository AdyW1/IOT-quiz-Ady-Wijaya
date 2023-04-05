#include <Arduino.h> 
#include <Ticker.h> 
#include <PubSubClient.h> 
#include <WiFi.h> 
#include <Wire.h> 
#include "BH1750.h" 
#include "DHTesp.h" 

#define LED_RED 4
#define LED_GREEN 5
#define LED_YELLOW 15
#define PIN_DHT 18
#define PIN_SDA 21
#define PIN_SCL 22
#define LED_BUILTIN_OFF 21
#define LED_BUILTIN_ON 22

  #define LED_COUNT 2 
  const uint8_t arLed[LED_COUNT] = {LED_RED, LED_GREEN}; 
 
const char* ssid = "Hydra (2)"; 
const char* password = "Hydra11/9"; 
 
#define MQTT_BROKER  "broker.emqx.io" 
#define MQTT_TOPIC_PUBLISH   "esp32_ady/data" 
#define MQTT_TOPIC_SUBSCRIBE "esp32_ady/cmd"  
#define MQTT_TOPIC_PUBLISH "esp32_ady/TEMP"
#define MQTT_TOPIC_PUBLISH "esp32_ady/LUX"
#define MQTT_TOPIC_PUBLISH "esp32_ady/HUMIDITY"
WiFiClient wifiClient; 
PubSubClient  mqtt(wifiClient);
Ticker timerPublish, ledOff; 
DHTesp dht; 
BH1750 lightMeter; 
 
char g_szDeviceId[30]; 
void WifiConnect(); 
boolean mqttConnect(); 
void onPublishMessage(); 
 
const char* LED = "null";
void setup() { 
  Serial.begin(115200); 
  delay(100); 
  pinMode(LED_BUILTIN, OUTPUT); 
  for (uint8_t i=0; i<LED_COUNT; i++) 
    pinMode(arLed[i], OUTPUT); 
  pinMode(LED_YELLOW, OUTPUT); 
 
  dht.setup(PIN_DHT, DHTesp::DHT11); 
  Wire.begin(PIN_SDA, PIN_SCL); 
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire); 
 
  Serial.printf("Free Memory: %d\n", ESP.getFreeHeap()); 
  WifiConnect(); 
  mqttConnect(); 
   timerPublish.attach_ms(1000, onPublishMessage); 
    
} 

 
void loop() { 
    mqtt.loop(); 
} 
 
//Message arrived [esp32_test/cmd/led1]: 0 
bool isLEDOn = false;

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();
  if (strcmp(topic, MQTT_TOPIC_SUBSCRIBE) == 0)
  {
    payload[len] = '\0';
    Serial.printf("LED3: %s\n", payload);
    LED = (char *)payload;
    if (strcmp(LED, "ON") == 0)
    {
      // Ubah status LED menjadi menyala
      isLEDOn = true;
       digitalWrite(LED_YELLOW, HIGH);
    }
    else if (strcmp(LED, "OFF") == 0)
    {
      // Ubah status LED menjadi mati
      isLEDOn = false;
      digitalWrite(LED_YELLOW, LOW);
    }
  }
}


 
void onPublishMessage() 
{ 
  unsigned long lastTempPublish = 0;
  unsigned long lastHumidPublish = 0;
  unsigned long lastLuxPublish = 0;
  const int Temp_Publish_interval = 7000;
  const int Humid_Publish_interval = 5000;
  const int Lux_Publish_interval = 4000;
  delay(5000);
  char szTopic[50];
  char szData[10];
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  float lux = lightMeter.readLightLevel();
  if (dht.getStatus() == DHTesp::ERROR_NONE)
  {
    if (millis() - lastTempPublish >= 7000) { // publish temperature data every 7 seconds
      lastTempPublish = millis();
      sprintf(szTopic, "%s/temperature", MQTT_TOPIC_PUBLISH);
      sprintf(szData, "%.2f", temperature);
      mqtt.publish(szTopic, szData);
      Serial.printf("Publish message Temperature: %s\n", szData);
    }
    
    if (millis() - lastHumidPublish >= 5000) { // publish humidity data every 5 seconds
      lastHumidPublish = millis();
      sprintf(szTopic, "%s/humidity", MQTT_TOPIC_PUBLISH);
      sprintf(szData, "%.2f", humidity);
      mqtt.publish(szTopic, szData);
      Serial.printf("Publish message Humidity: %s\n", szData);
    }
  }
  
  if (millis() - lastLuxPublish >= 4000) { // publish lux data every 4 seconds
    lastLuxPublish = millis();
    sprintf(szTopic, "%s/light", MQTT_TOPIC_PUBLISH);
    sprintf(szData, "%.2f", lux);
    mqtt.publish(szTopic, szData);
    Serial.printf("Publish message Light: %s\n", szData);
  }
  
  // Check lux level and set MTreg accordingly
  if (lux < 0) {
    Serial.println(F("Error condition detected"));
  } else {
    if (lux >= 400) {
      // reduce measurement time - needed in direct sun light
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);
      if (lightMeter.setMTreg(32)) {
        Serial.println(F("The Door is Open"));
      } else {
        Serial.println(F("Error, Sensor is not detected"));
      }
    } else {
      if (lux < 400) {
        // typical light environment
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, HIGH);
        if (lightMeter.setMTreg(69)) {
          Serial.println(F("The Door Is Close"));
        } else {
          Serial.println(F("Error, Sensor is not detected"));
        }
      } 
    }
  }
}
 
boolean mqttConnect() {  
  sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());  
  mqtt.setServer(MQTT_BROKER, 1883); 
  mqtt.setCallback(mqttCallback); 
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId); 
 
 // Connect to MQTT Broker 
  // Or, if you want to authenticate MQTT: 
  boolean status = mqtt.connect(g_szDeviceId); 
 
  if (status == false) { 
    Serial.print(" fail, rc="); 
    Serial.print(mqtt.state()); 
    return false; 
  } 
  Serial.println(" success"); 
 
  mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE); 
  Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE); 
  onPublishMessage(); 
  return mqtt.connected(); 
} 
 
void WifiConnect() 
{ 
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password); 
  while (WiFi.waitForConnectResult() != WL_CONNECTED) { 
    Serial.println("Connection Failed! Rebooting..."); 

    delay(5000); 
    ESP.restart(); 
  }   
  Serial.print("System connected with IP address: "); 
  Serial.println(WiFi.localIP()); 
  Serial.printf("RSSI: %d\n", WiFi.RSSI()); 
} 


