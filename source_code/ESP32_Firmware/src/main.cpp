#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h> 
#include <AccelStepper.h>

#define DHTPIN 27 
#define DHTTYPE DHT22     
DHT dht(DHTPIN, DHTTYPE);

#define DOOR_PIN 4 
#define FAN_PIN 26 
#define DS18B20_PIN 14 

#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 21

AccelStepper stepper(AccelStepper::FULL4WIRE, IN1, IN3, IN2, IN4);

const long STEPS_OPEN = 512; 
const long STEPS_CLOSED = 0;
bool doorState = false; 
bool currentCoolingState = false;

typedef enum 
{
  STATE_NORMAL = 0,
  STATE_WARNING = 1,
  STATE_VIOLATION = 2
} ThermalState;

typedef struct 
{
  unsigned long timestamp;
  float temperature;
  float humidity;
  bool doorOpen;
  bool coolingState;            
  float accumulatedExposure;
  ThermalState state;
} TelemetryRecord;

#define BUFFER_SIZE 100

typedef struct 
{
  TelemetryRecord buffer[BUFFER_SIZE];
  int head;
  int tail;
  int count;
} OfflineBuffer;

OfflineBuffer dataBuffer = { .head = 0, .tail = 0, .count = 0 };

bool buffer_push(OfflineBuffer *cb, TelemetryRecord record) 
{
  if (cb->count >= BUFFER_SIZE) {
    cb->tail = (cb->tail + 1) % BUFFER_SIZE; 
  } else {
    cb->count++;
  }
  cb->buffer[cb->head] = record;
  cb->head = (cb->head + 1) % BUFFER_SIZE;
  return true;
}

bool buffer_pop(OfflineBuffer *cb, TelemetryRecord *record) 
{
  if (cb->count == 0) return false;
  *record = cb->buffer[cb->tail];
  cb->tail = (cb->tail + 1) % BUFFER_SIZE;
  cb->count--;
  return true;
}

bool buffer_isEmpty(OfflineBuffer *cb) {
  return cb->count == 0;
}

const float T_MAX = 8.0;
const float T_MIN = 2.0;
const float EXPOSURE_LIMIT = 30.0;
const unsigned long DOOR_ALERT_MS = 60000;

ThermalState currentState = STATE_NORMAL;
float totalExposure = 0.0;
unsigned long lastCalcTime = 0;
unsigned long doorOpenStartTime = 0;

void updateThermalFSM(float currentTemp, bool isDoorOpen) {
  unsigned long currentTime = millis();
  
  if (lastCalcTime == 0) { lastCalcTime = currentTime; return; }

  float dt_minutes = (currentTime - lastCalcTime) / 60000.0;
  if (currentTemp > T_MAX) {
    totalExposure += (currentTemp - T_MAX) * dt_minutes;
  } else if (currentTemp < T_MIN) {
    totalExposure += (T_MIN - currentTemp) * dt_minutes;
  }
  lastCalcTime = currentTime;

  if (isDoorOpen && doorOpenStartTime == 0) {
    doorOpenStartTime = currentTime;
  } else if (!isDoorOpen) {
    doorOpenStartTime = 0;
  }
  bool doorWarning = (isDoorOpen && (currentTime - doorOpenStartTime > DOOR_ALERT_MS));

  switch (currentState) {
    case STATE_NORMAL:
      if (totalExposure >= EXPOSURE_LIMIT) currentState = STATE_VIOLATION;
      else if (currentTemp > T_MAX || currentTemp < T_MIN || doorWarning) currentState = STATE_WARNING;
      break;
    case STATE_WARNING:
      if (totalExposure >= EXPOSURE_LIMIT) currentState = STATE_VIOLATION;
      else if (currentTemp <= T_MAX && currentTemp >= T_MIN && !doorWarning) currentState = STATE_NORMAL;
      break;
    case STATE_VIOLATION:
      break;
  }
}

const char* ssid = "iPhone";        
const char* password = "vanquy1111";        

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "coldchain/nhom07/telemetry";
const char* device_id = "ESP32_COLD_01";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup_wifi() {
  delay(10);
  Serial.printf("\nDang ket noi voi %s\n", ssid);
  WiFi.begin(ssid, password); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); 
    Serial.print(".");
  }
  Serial.printf("\nDa ket noi WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
}

void reconnect_mqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Dang ket noi den EMQX Broker...");
    String clientId = "ESP32Client-" + String(random(0, 1000));
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Ket noi thanh cong!");
    } else {
      Serial.printf("That bai, rc=%d. Thu lai sau 5 giay...\n", mqttClient.state());
      delay(5000);
    }
  }
}

bool isMqttConnected(void) { return mqttClient.connected(); }

void sendToMqtt(TelemetryRecord *rec, bool isBuffered) {
  char jsonPayload[256];
  snprintf(jsonPayload, sizeof(jsonPayload),
           "{\"device_id\": \"%s\", \"timestamp\": %lu, \"temperature\": %.1f, \"humidity\": %.1f, \"door_open\": %s, \"cooling_state\": %s}",
           device_id, rec->timestamp, rec->temperature, rec->humidity,
           rec->doorOpen ? "true" : "false",
           rec->coolingState ? "true" : "false");

  if (mqttClient.publish(mqtt_topic, jsonPayload)) {
    Serial.printf("[MQTT] Da gui (%s): %s\n", isBuffered ? "Luu Dem" : "Truc tiep", jsonPayload);
  } else {
    Serial.println("[MQTT] Gui THAT BAI!");
  }
}

float smoothedTemp = -999.0;

float readTemperature(void) {
  float rawTemp = dht.readTemperature();
  
  if (isnan(rawTemp)) {
    Serial.println("[CANH BAO] Loi doc cam bien DHT22!");
    return (smoothedTemp == -999.0) ? 0.0 : smoothedTemp;
  }

  float offset_temp = 0.0; 
  float calTemp = rawTemp + offset_temp;

  if (smoothedTemp == -999.0) {
    smoothedTemp = calTemp; 
  } else {
    smoothedTemp = (calTemp * 0.2) + (smoothedTemp * 0.8);
  }
  return smoothedTemp;
}

float readHumidity(void) {
  float h = dht.readHumidity();
  return isnan(h) ? 60.0 : h; 
}

void handleDoorControl(void) 
{
  bool switchState = (digitalRead(DOOR_PIN) == LOW); 

  if (switchState && !doorState) {
    doorState = true;
    
    digitalWrite(FAN_PIN, HIGH); 
    currentCoolingState = false;
    
    stepper.enableOutputs();
    stepper.moveTo(STEPS_OPEN);
    Serial.println("[CUA] GAT CONG TAC -> MO CUA (Quay tien 90 do)");
  } 
  else if (!switchState && doorState) {
    doorState = false;
    
    digitalWrite(FAN_PIN, HIGH); 
    currentCoolingState = false;
    
    stepper.enableOutputs();
    stepper.moveTo(STEPS_CLOSED);
    Serial.println("[CUA] GAT NGUOC LAI -> DONG CUA (Quay lui ve 0 do)");
  }

  if (stepper.distanceToGo() != 0) {
    stepper.run();
  } else {
    stepper.disableOutputs(); 
  }
}

unsigned long getUnixTime(void) { 
  return (millis() / 1000) + 1773700000; 
} 

unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 5000; 

void setup() {
  Serial.begin(115200);
  
  dht.begin();
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH); 

  stepper.setMaxSpeed(600.0); 
  stepper.setAcceleration(300.0); 
  stepper.setCurrentPosition(0); 
  stepper.disableOutputs();

  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  Serial.println("--- ESP32 COLD CHAIN: SYSTEM READY ---");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconnect_mqtt();
    mqttClient.loop(); 
  }

  handleDoorControl();

  unsigned long currentTime = millis();
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentTime;

    float t = readTemperature();
    float h = readHumidity();
    bool door = doorState;
    
    if (t > 8.0 && !currentCoolingState && stepper.distanceToGo() == 0) {
      digitalWrite(FAN_PIN, LOW); 
      currentCoolingState = true;
    }
    else if ((t <= 8.0 || stepper.distanceToGo() != 0) && currentCoolingState) {
      digitalWrite(FAN_PIN, HIGH); 
      currentCoolingState = false;
    }

    updateThermalFSM(t, door);

    TelemetryRecord record;
    record.timestamp = getUnixTime();
    record.temperature = t;
    record.humidity = h;
    record.doorOpen = door;
    record.coolingState = currentCoolingState; 
    record.accumulatedExposure = totalExposure;
    record.state = currentState;

    if (isMqttConnected()) {
      sendToMqtt(&record, false);
      while (!buffer_isEmpty(&dataBuffer)) {
        TelemetryRecord oldRecord;
        if (buffer_pop(&dataBuffer, &oldRecord)) {
          sendToMqtt(&oldRecord, true);
          delay(50); 
        }
      }
    } else {
      buffer_push(&dataBuffer, record);
      Serial.printf("[OFFLINE] Luu vao bo dem. (Data ket: %d/%d)\n", 
                    dataBuffer.count, BUFFER_SIZE);
    }
  }
}