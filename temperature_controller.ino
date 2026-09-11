#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

#include "TemperatureLogic.h"

// ============================================================
// USER CONFIGURATION
// ============================================================

const char* AP_SSID = "TEMPERATURE_CONTROLLER";
const char* AP_PASSWORD = "00000000";

// GPIO assignments
constexpr uint8_t ONE_WIRE_PIN = 4;
constexpr uint8_t BLUE_LED_PIN = 5;
constexpr uint8_t RED_LED_PIN = 6;
constexpr uint8_t BUZZER_PIN = 7;
constexpr uint8_t PUMP_PIN = 10;

// Sensor polling
constexpr unsigned long SENSOR_INTERVAL_MS = 30000;

// ============================================================
// GLOBAL OBJECTS
// ============================================================

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);
WebServer server(80);
Preferences preferences;

// ============================================================
// SENSOR MODEL
// ============================================================

struct SensorInfo {
  DeviceAddress address;
  String addressString;

  int assignedNumber;

  float temperature;
  bool valid;

  float minTemp;
  float maxTemp;

  AlarmState alarmState;
};

SensorInfo discoveredSensors[MAX_SENSORS];
int sensorCount = 0;

unsigned long lastSensorRead = 0;

// ============================================================
// ADDRESS / ID HELPERS
// ============================================================

String addressToString(const uint8_t* address) {
  char buffer[24];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
    address[0], address[1], address[2], address[3],
    address[4], address[5], address[6], address[7]);

  return String(buffer);
}

String sensorKeyBase(const uint8_t* address) {
  char buffer[12];

  snprintf(
    buffer,
    sizeof(buffer),
    "s%08lX",
    static_cast<unsigned long>(hashRomAddress(address)));

  return String(buffer);
}

// ============================================================
// NVS / PREFERENCES
// ============================================================

int loadAssignedNumber(const uint8_t* address) {
  String key = sensorKeyBase(address) + "n";
  return preferences.getInt(key.c_str(), -1);
}

float loadMinTemp(const uint8_t* address) {
  String key = sensorKeyBase(address) + "l";
  return preferences.getFloat(key.c_str(), DEFAULT_MIN_TEMP);
}

float loadMaxTemp(const uint8_t* address) {
  String key = sensorKeyBase(address) + "h";
  return preferences.getFloat(key.c_str(), DEFAULT_MAX_TEMP);
}

void saveSensorConfiguration(
  const uint8_t* address,
  int assignedNumber,
  float minTemp,
  float maxTemp) {
  String base = sensorKeyBase(address);

  preferences.putInt((base + "n").c_str(), assignedNumber);
  preferences.putFloat((base + "l").c_str(), minTemp);
  preferences.putFloat((base + "h").c_str(), maxTemp);
}

// ============================================================
// SENSOR DISCOVERY
// ============================================================

void discoverSensors() {
  sensorCount = 0;

  ds18b20.begin();

  int discovered = ds18b20.getDeviceCount();

  Serial.println();
  Serial.printf("Discovered %d 1-Wire device(s)\n", discovered);

  DeviceAddress address;

  for (int i = 0; i < discovered && sensorCount < MAX_SENSORS; i++) {
    if (!ds18b20.getAddress(address, i)) {
      Serial.printf("Could not read address for device index %d\n", i);
      continue;
    }

    SensorInfo& sensor = discoveredSensors[sensorCount];

    memcpy(sensor.address, address, sizeof(DeviceAddress));

    sensor.addressString = addressToString(sensor.address);
    sensor.assignedNumber = loadAssignedNumber(sensor.address);
    sensor.minTemp = loadMinTemp(sensor.address);
    sensor.maxTemp = loadMaxTemp(sensor.address);

    sensor.temperature = 0.0f;
    sensor.valid = false;
    sensor.alarmState = AlarmState::SENSOR_ERROR;

    Serial.printf(
      "Sensor %d: %s | assigned=%d | min=%.2f | max=%.2f\n",
      sensorCount,
      sensor.addressString.c_str(),
      sensor.assignedNumber,
      sensor.minTemp,
      sensor.maxTemp);

    sensorCount++;
  }

  if (discovered > MAX_SENSORS) {
    Serial.printf(
      "WARNING: %d device(s) found but MAX_SENSORS is %d\n",
      discovered,
      MAX_SENSORS);
  }
}

void updateAlarmOutputs() {
  bool anyCold = false;
  bool anyHot = false;
  bool anyError = false;

  for (int i = 0; i < sensorCount; i++) {
    switch (discoveredSensors[i].alarmState) {
      case AlarmState::TOO_COLD:
        anyCold = true;
        break;
      case AlarmState::TOO_HOT:
        anyHot = true;
        break;
      case AlarmState::SENSOR_ERROR:
        anyError = true;
        break;
      case AlarmState::OK:
      default:
        break;
    }
  }

  // Blue means at least one sensor is too cold.
  digitalWrite(BLUE_LED_PIN, anyCold ? HIGH : LOW);

  // Red means at least one sensor is too hot.
  digitalWrite(RED_LED_PIN, anyHot ? HIGH : LOW);

  // Buzzer sounds for any temperature alarm or sensor failure.
  bool buzzerOn = anyCold || anyHot || anyError;
  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);

  // Sensor failure indication:
  // both LEDs ON simultaneously.
  if (anyError) {
    digitalWrite(BLUE_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
  }
}

// ============================================================
// TEMPERATURE READING
// ============================================================

void checkTemperatures() {
  if (sensorCount == 0) {
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  ds18b20.requestTemperatures();

  for (int i = 0; i < sensorCount; i++) {
    SensorInfo& sensor = discoveredSensors[i];
    float temperature = ds18b20.getTempC(sensor.address);

    sensor.temperature = temperature;
    sensor.valid = isValidTemperature(temperature);

    updateSensorAlarmState(sensor);

    Serial.printf(
      "%s | number=%d | ",
      sensor.addressString.c_str(),
      sensor.assignedNumber);

    if (sensor.valid) {
      Serial.printf(
        "%.2f C | range %.2f .. %.2f | %s\n",
        sensor.temperature,
        sensor.minTemp,
        sensor.maxTemp,
        alarmStateToText(sensor.alarmState));
    } else {
      Serial.println("SENSOR ERROR");
    }
  }

  updateAlarmOutputs();
}

// ============================================================
// HTML HELPERS
// ============================================================

String htmlEscape(const String& value) {
  String result = value;

  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("\"", "&quot;");

  return result;
}

bool assignedNumberAlreadyUsed(int number, int exceptIndex) {
  if (number < 0) {
    return false;
  }

  for (int i = 0; i < sensorCount; i++) {
    if (i == exceptIndex) {
      continue;
    }
    if (discoveredSensors[i].assignedNumber == number) {
      return true;
    }
  }

  return false;
}

// ============================================================
// WEB PAGE
// ============================================================

void handleRootEN() {
  String html;
  html.reserve(18000);

  html += R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Temperature Monitor</title>

<style>
body {
  font-family: Arial, sans-serif;
  margin: 0;
  padding: 24px;
  background: #f4f6f8;
  color: #222;
}

.container {
  max-width: 1200px;
  margin: auto;
}

h1 {
  margin-bottom: 4px;
}

.subtitle {
  margin-bottom: 20px;
  color: #666;
}

.summary {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  margin-bottom: 20px;
}

.card {
  background: white;
  padding: 12px 16px;
  border-radius: 8px;
  box-shadow: 0 1px 4px rgba(0,0,0,0.08);
}

table {
  width: 100%;
  border-collapse: collapse;
  background: white;
  border-radius: 10px;
  overflow: hidden;
  box-shadow: 0 1px 4px rgba(0,0,0,0.08);
}

th {
  background: #eceff1;
  text-align: left;
}

th, td {
  padding: 12px;
  border-bottom: 1px solid #ddd;
  vertical-align: middle;
}

.address {
  font-family: monospace;
  font-size: 13px;
}

.temperature {
  font-weight: bold;
  font-size: 18px;
}

.ok {
  color: #2e7d32;
  font-weight: bold;
}

.cold {
  color: #1565c0;
  font-weight: bold;
}

.hot {
  color: #c62828;
  font-weight: bold;
}

.error {
  color: #8e24aa;
  font-weight: bold;
}

.unassigned {
  color: #d97706;
  font-weight: bold;
}

input[type=number] {
  width: 90px;
  padding: 6px;
  font-size: 14px;
}

button {
  padding: 7px 14px;
  cursor: pointer;
}

.actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-top: 18px;
}

.small {
  font-size: 12px;
  color: #666;
}

@media (max-width: 900px) {
  table {
    font-size: 13px;
  }

  th, td {
    padding: 8px;
  }

  input[type=number] {
    width: 72px;
  }
}
</style>
</head>

<body>
<div class="container">

<h1>ESP32 Temperature Monitor</h1>

<div class="subtitle">
Discovered DS18B20 sensors on the 1-Wire bus
</div>
)rawliteral";

  int coldCount = 0;
  int hotCount = 0;
  int errorCount = 0;

  for (int i = 0; i < sensorCount; i++) {

    switch (discoveredSensors[i].alarmState) {
      case AlarmState::TOO_COLD:
        coldCount++;
        break;

      case AlarmState::TOO_HOT:
        hotCount++;
        break;

      case AlarmState::SENSOR_ERROR:
        errorCount++;
        break;

      case AlarmState::OK:
      default:
        break;
    }
  }

  html += "<div class='summary'>";

  html += "<div class='card'>Sensors: <strong>";
  html += String(sensorCount);
  html += "</strong></div>";

  html += "<div class='card cold'>Too cold: ";
  html += String(coldCount);
  html += "</div>";

  html += "<div class='card hot'>Too hot: ";
  html += String(hotCount);
  html += "</div>";

  html += "<div class='card error'>Errors: ";
  html += String(errorCount);
  html += "</div>";

  html += "</div>";

  html += R"rawliteral(
<table>
<tr>
  <th>ROM address</th>
  <th>Number</th>
  <th>Temperature</th>
  <th>Min</th>
  <th>Max</th>
  <th>Status</th>
  <th>Configuration</th>
</tr>
)rawliteral";

  for (int i = 0; i < sensorCount; i++) {

    SensorInfo& sensor = discoveredSensors[i];

    html += "<tr>";

    // Address
    html += "<td class='address'>";
    html += htmlEscape(sensor.addressString);
    html += "</td>";

    // Assigned number
    html += "<td>";

    if (sensor.assignedNumber >= 0) {
      html += String(sensor.assignedNumber);
    } else {
      html += "<span class='unassigned'>Unassigned</span>";
    }

    html += "</td>";

    // Temperature
    html += "<td>";

    if (sensor.valid) {
      html += "<span class='temperature'>";
      html += String(sensor.temperature, 1);
      html += " &deg;C</span>";
    } else {
      html += "<span class='error'>ERROR</span>";
    }

    html += "</td>";

    // Min
    html += "<td>";
    html += String(sensor.minTemp, 1);
    html += " &deg;C</td>";

    // Max
    html += "<td>";
    html += String(sensor.maxTemp, 1);
    html += " &deg;C</td>";

    // Status
    html += "<td class='";
    html += stateCssClass(sensor.alarmState);
    html += "'>";
    html += alarmStateToText(sensor.alarmState);
    html += "</td>";

    // Configuration form
    html += "<td>";

    html += "<form method='POST' action='/configure'>";

    html += "<input type='hidden' name='index' value='";
    html += String(i);
    html += "'>";

    html += "No. ";
    html += "<input type='number' name='number' min='0' max='999' step='1'";

    if (sensor.assignedNumber >= 0) {
      html += " value='";
      html += String(sensor.assignedNumber);
      html += "'";
    }

    html += "> ";

    html += "Min ";
    html += "<input type='number' name='minTemp' step='0.1' value='";
    html += String(sensor.minTemp, 1);
    html += "'> ";

    html += "Max ";
    html += "<input type='number' name='maxTemp' step='0.1' value='";
    html += String(sensor.maxTemp, 1);
    html += "'> ";

    html += "<button type='submit'>Save</button>";

    html += "</form>";
    html += "</td>";

    html += "</tr>";
  }

  html += "</table>";

  html += R"rawliteral(
<div class="actions">
  <form method="POST" action="/rescan">
    <button type="submit">Rescan 1-Wire bus</button>
  </form>
</div>

<p class="small">
Blue LED = at least one sensor too cold.<br>
Red LED = at least one sensor too hot.<br>
Both LEDs = sensor read failure.<br>
Buzzer = any cold, hot, or sensor-error condition.
</p>

</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleRootHR() {
  String html;
  html.reserve(18000);

  html += R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Temperaturni senzori</title>

<style>
body {
  font-family: Arial, sans-serif;
  margin: 0;
  padding: 24px;
  background: #f4f6f8;
  color: #222;
}

.container {
  max-width: 1200px;
  margin: auto;
}

h1 {
  margin-bottom: 4px;
}

.subtitle {
  margin-bottom: 20px;
  color: #666;
}

.summary {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  margin-bottom: 20px;
}

.card {
  background: white;
  padding: 12px 16px;
  border-radius: 8px;
  box-shadow: 0 1px 4px rgba(0,0,0,0.08);
}

table {
  width: 100%;
  border-collapse: collapse;
  background: white;
  border-radius: 10px;
  overflow: hidden;
  box-shadow: 0 1px 4px rgba(0,0,0,0.08);
}

th {
  background: #eceff1;
  text-align: left;
}

th, td {
  padding: 12px;
  border-bottom: 1px solid #ddd;
  vertical-align: middle;
}

.address {
  font-family: monospace;
  font-size: 13px;
}

.temperature {
  font-weight: bold;
  font-size: 18px;
}

.ok {
  color: #2e7d32;
  font-weight: bold;
}

.cold {
  color: #1565c0;
  font-weight: bold;
}

.hot {
  color: #c62828;
  font-weight: bold;
}

.error {
  color: #8e24aa;
  font-weight: bold;
}

.unassigned {
  color: #d97706;
  font-weight: bold;
}

input[type=number] {
  width: 90px;
  padding: 6px;
  font-size: 14px;
}

button {
  padding: 7px 14px;
  cursor: pointer;
}

.actions {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-top: 18px;
}

.small {
  font-size: 12px;
  color: #666;
}

@media (max-width: 900px) {
  table {
    font-size: 13px;
  }

  th, td {
    padding: 8px;
  }

  input[type=number] {
    width: 72px;
  }
}
</style>
</head>

<body>
<div class="container">

<h1>Temperaturni senzori</h1>

<div class="subtitle">
Spojeni DS18B20 temperaturni senzori
</div>
)rawliteral";

  int coldCount = 0;
  int hotCount = 0;
  int errorCount = 0;

  for (int i = 0; i < sensorCount; i++) {
    switch (discoveredSensors[i].alarmState) {
      case AlarmState::TOO_COLD:
        coldCount++;
        break;
      case AlarmState::TOO_HOT:
        hotCount++;
        break;
      case AlarmState::SENSOR_ERROR:
        errorCount++;
        break;
      case AlarmState::OK:
      default:
        break;
    }
  }

  html += "<div class='summary'>";

  html += "<div class='card'>Senzori: <strong>";
  html += String(sensorCount);
  html += "</strong></div>";

  html += "<div class='card cold'>Prehladno: ";
  html += String(coldCount);
  html += "</div>";

  html += "<div class='card hot'>Pretoplo: ";
  html += String(hotCount);
  html += "</div>";

  html += "<div class='card error'>Greske: ";
  html += String(errorCount);
  html += "</div>";

  html += "</div>";

  html += R"rawliteral(
<table>
<tr>
  <th>ROM adresa</th>
  <th>Broj senzora</th>
  <th>Temperatura</th>
  <th>Min</th>
  <th>Max</th>
  <th>Status</th>
  <th>Postavke</th>
</tr>
)rawliteral";

  for (int i = 0; i < sensorCount; i++) {
    SensorInfo& sensor = discoveredSensors[i];

    html += "<tr>";

    // Address
    html += "<td class='address'>";
    html += htmlEscape(sensor.addressString);
    html += "</td>";

    // Assigned number
    html += "<td>";
    if (sensor.assignedNumber >= 0) {
      html += String(sensor.assignedNumber);
    } else {
      html += "<span class='unassigned'>?</span>";
    }
    html += "</td>";

    // Temperature
    html += "<td>";
    if (sensor.valid) {
      html += "<span class='temperature'>";
      html += String(sensor.temperature, 1);
      html += " &deg;C</span>";
    } else {
      html += "<span class='error'>GRESKA</span>";
    }
    html += "</td>";

    // Min
    html += "<td>";
    html += String(sensor.minTemp, 1);
    html += " &deg;C</td>";

    // Max
    html += "<td>";
    html += String(sensor.maxTemp, 1);
    html += " &deg;C</td>";

    // Status
    html += "<td class='";
    html += stateCssClass(sensor.alarmState);
    html += "'>";
    html += alarmStateToText(sensor.alarmState);
    html += "</td>";

    // Configuration form
    html += "<td>";
    html += "<form method='POST' action='/configure'>";

    html += "<input type='hidden' name='index' value='";
    html += String(i);
    html += "'>";

    html += "Broj ";
    html += "<input type='number' name='number' min='0' max='999' step='1'";
    if (sensor.assignedNumber >= 0) {
      html += " value='";
      html += String(sensor.assignedNumber);
      html += "'";
    }
    html += "> ";

    html += "Min ";
    html += "<input type='number' name='minTemp' step='0.1' value='";
    html += String(sensor.minTemp, 1);
    html += "'> ";

    html += "Max ";
    html += "<input type='number' name='maxTemp' step='0.1' value='";
    html += String(sensor.maxTemp, 1);
    html += "'> ";

    html += "<button type='submit'>Spremi</button>";

    html += "</form>";
    html += "</td>";
    html += "</tr>";
  }

  html += "</table>";

  html += R"rawliteral(
<div class="actions">
  <form method="POST" action="/rescan">
    <button type="submit">Pronadi senzore</button>
  </form>
</div>

<p class="small">
Plava LED = barem jedna temperatura je preniska<br>
Crvena LED = barem jedna temperatura je previsoka<br>
Obje LED = greska sa citanjem senzora<br>
Buzzer = bilo koji alarm
</p>

</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// CONFIGURATION ENDPOINT
// ============================================================

void handleConfigure() {
  if (
    !server.hasArg("index") || !server.hasArg("number") || !server.hasArg("minTemp") || !server.hasArg("maxTemp")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  int index = server.arg("index").toInt();
  int number = server.arg("number").toInt();
  float minTemp = server.arg("minTemp").toFloat();
  float maxTemp = server.arg("maxTemp").toFloat();

  if (index < 0 || index >= sensorCount) {
    server.send(400, "text/plain", "Invalid sensor index");
    return;
  }

  if (number < 0 || number > 999) {
    server.send(400, "text/plain", "Sensor number must be between 0 and 999");
    return;
  }

  if (assignedNumberAlreadyUsed(number, index)) {
    server.send(400, "text/plain", "That sensor number is already assigned");
    return;
  }

  if (minTemp >= maxTemp) {
    server.send(
      400,
      "text/plain",
      "Minimum temperature must be lower than maximum temperature");
    return;
  }

  if (
    minTemp < VALID_MIN_TEMP || minTemp > VALID_MAX_TEMP || maxTemp < VALID_MIN_TEMP || maxTemp > VALID_MAX_TEMP) {
    server.send(400, "text/plain", "Configured temperature range is invalid");
    return;
  }

  SensorInfo& sensor = discoveredSensors[index];

  sensor.assignedNumber = number;
  sensor.minTemp = minTemp;
  sensor.maxTemp = maxTemp;

  saveSensorConfiguration(
    sensor.address,
    sensor.assignedNumber,
    sensor.minTemp,
    sensor.maxTemp);

  // Re-evaluate immediately using current reading.
  updateSensorAlarmState(sensor);
  updateAlarmOutputs();

  Serial.printf(
    "Saved sensor %s -> number=%d min=%.2f max=%.2f\n",
    sensor.addressString.c_str(),
    sensor.assignedNumber,
    sensor.minTemp,
    sensor.maxTemp);

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// ============================================================
// RESCAN ENDPOINT
// ============================================================

void handleRescan() {
  discoverSensors();
  checkTemperatures();

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// ============================================================
// JSON API
// ============================================================

void handleApi() {
  String json;
  json.reserve(6000);

  json += "{";
  json += "\"sensorCount\":";
  json += String(sensorCount);
  json += ",\"sensors\":[";

  for (int i = 0; i < sensorCount; i++) {
    if (i > 0) {
      json += ",";
    }

    const SensorInfo& sensor = discoveredSensors[i];

    json += "{";

    json += "\"address\":\"";
    json += sensor.addressString;
    json += "\",";

    json += "\"number\":";

    if (sensor.assignedNumber >= 0) {
      json += String(sensor.assignedNumber);
    } else {
      json += "null";
    }

    json += ",\"valid\":";
    json += sensor.valid ? "true" : "false";

    if (sensor.valid) {
      json += ",\"temperature\":";
      json += String(sensor.temperature, 2);
    }

    json += ",\"minTemp\":";
    json += String(sensor.minTemp, 2);

    json += ",\"maxTemp\":";
    json += String(sensor.maxTemp, 2);

    json += ",\"status\":\"";
    json += alarmStateToText(sensor.alarmState);
    json += "\"";

    json += "}";
  }

  json += "]";
  json += "}";

  server.send(200, "application/json", json);
}

// ============================================================
// OPTIONAL NOT-FOUND HANDLER
// ============================================================

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================
// SETUP
// ============================================================

void setupWifiAccessPoint() {
  size_t passwordLength = strlen(AP_PASSWORD);
  if (passwordLength > 0 && passwordLength < 8) {
    Serial.println("Failed to start Wi-Fi AP: password must be empty/open or at least 8 characters");
    return;
  }

  WiFi.mode(WIFI_AP);

  IPAddress localIP(192, 168, 10, 1);
  IPAddress gateway(192, 168, 10, 1);
  IPAddress subnet(255, 255, 255, 0);

  if (!WiFi.softAPConfig(localIP, gateway, subnet)) {
    Serial.println("AP IP configuration failed");
    return;
  }

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Failed to start Wi-Fi AP");
    return;
  }

  IPAddress ip = WiFi.softAPIP();

  Serial.println();
  Serial.print("Web interface: http://");
  Serial.println(ip);

  Serial.print("JSON API: http://");
  Serial.print(ip);
  Serial.println("/api/sensors");

  // HTTP endpoints
  // server.on("/", HTTP_GET, handleRootEN);
  server.on("/", HTTP_GET, handleRootHR);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/rescan", HTTP_POST, handleRescan);
  server.on("/api/sensors", HTTP_GET, handleApi);

  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP32 DS18B20 Temperature Monitor");
  Serial.println("---------------------------------");

  // Outputs
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(BLUE_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  // Open NVS namespace
  preferences.begin("tempsensors", false);

  // DS18B20
  ds18b20.begin();

  // 11-bit gives 0.125 C resolution with faster conversion than 12-bit.
  ds18b20.setResolution(11);

  discoverSensors();
  checkTemperatures();

  setupWifiAccessPoint();

  lastSensorRead = millis();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    checkTemperatures();
  }
}
