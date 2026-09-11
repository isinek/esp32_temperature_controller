#include <cassert>
#include <cstring>
#include <iostream>

#include "../TemperatureLogic.h"

struct TestSensor {
  float temperature;
  bool valid;
  float minTemp;
  float maxTemp;
  AlarmState alarmState;
};

TestSensor sensorAt(float temperature, AlarmState state = AlarmState::OK) {
  return TestSensor{
    temperature,
    true,
    DEFAULT_MIN_TEMP,
    DEFAULT_MAX_TEMP,
    state};
}

void testTemperatureValidation() {
  assert(isValidTemperature(VALID_MIN_TEMP));
  assert(isValidTemperature(VALID_MAX_TEMP));
  assert(isValidTemperature(21.5f));

  assert(!isValidTemperature(VALID_MIN_TEMP - 0.1f));
  assert(!isValidTemperature(VALID_MAX_TEMP + 0.1f));
  assert(!isValidTemperature(SENSOR_DISCONNECTED_C));
}

void testAlarmTransitions() {
  TestSensor sensor = sensorAt(16.0f);
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::OK);

  sensor = sensorAt(DEFAULT_MIN_TEMP - 0.1f);
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::TOO_COLD);

  sensor = sensorAt(DEFAULT_MAX_TEMP + 0.1f);
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::TOO_HOT);

  sensor = sensorAt(16.0f);
  sensor.valid = false;
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::SENSOR_ERROR);
}

void testColdAlarmHysteresis() {
  TestSensor sensor = sensorAt(DEFAULT_MIN_TEMP - 1.0f, AlarmState::TOO_COLD);

  sensor.temperature = DEFAULT_MIN_TEMP + ALARM_HYSTERESIS;
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::TOO_COLD);

  sensor.temperature = DEFAULT_MIN_TEMP + ALARM_HYSTERESIS + 0.01f;
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::OK);
}

void testHotAlarmHysteresis() {
  TestSensor sensor = sensorAt(DEFAULT_MAX_TEMP + 1.0f, AlarmState::TOO_HOT);

  sensor.temperature = DEFAULT_MAX_TEMP - ALARM_HYSTERESIS;
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::TOO_HOT);

  sensor.temperature = DEFAULT_MAX_TEMP - ALARM_HYSTERESIS - 0.01f;
  updateSensorAlarmState(sensor);
  assert(sensor.alarmState == AlarmState::OK);
}

void testStateLabels() {
  assert(std::strcmp(alarmStateToText(AlarmState::OK), "OK") == 0);
  assert(std::strcmp(alarmStateToText(AlarmState::TOO_COLD), "TOO COLD") == 0);
  assert(std::strcmp(alarmStateToText(AlarmState::TOO_HOT), "TOO HOT") == 0);
  assert(std::strcmp(alarmStateToText(AlarmState::SENSOR_ERROR), "SENSOR ERROR") == 0);

  assert(std::strcmp(stateCssClass(AlarmState::OK), "ok") == 0);
  assert(std::strcmp(stateCssClass(AlarmState::TOO_COLD), "cold") == 0);
  assert(std::strcmp(stateCssClass(AlarmState::TOO_HOT), "hot") == 0);
  assert(std::strcmp(stateCssClass(AlarmState::SENSOR_ERROR), "error") == 0);
}

void testRomAddressHash() {
  const uint8_t address[] = {0, 1, 2, 3, 4, 5, 6, 7};

  assert(hashRomAddress(address) == 0x6BF6A41D);
}

void testHistoryTemperatureEncoding() {
  assert(HISTORY_SAMPLE_COUNT == 2880);
  assert(encodeHistoryTemperature(21.234f, true) == 2123);
  assert(encodeHistoryTemperature(21.235f, true) == 2124);
  assert(encodeHistoryTemperature(-3.456f, true) == -346);
  assert(encodeHistoryTemperature(25.0f, false) == HISTORY_INVALID_TEMPERATURE);
  assert(decodeHistoryTemperature(2125) == 21.25f);
}

void testHistoryIndexing() {
  assert(historyOldestIndex(12, 12) == 0);
  assert(historyOldestIndex(12, HISTORY_SAMPLE_COUNT) == 12);
  assert(historyPhysicalIndex(HISTORY_SAMPLE_COUNT - 1, 0) == HISTORY_SAMPLE_COUNT - 1);
  assert(historyPhysicalIndex(HISTORY_SAMPLE_COUNT - 1, 1) == 0);
}

int main() {
  testTemperatureValidation();
  testAlarmTransitions();
  testColdAlarmHysteresis();
  testHotAlarmHysteresis();
  testStateLabels();
  testRomAddressHash();
  testHistoryTemperatureEncoding();
  testHistoryIndexing();

  std::cout << "All temperature logic tests passed\n";
  return 0;
}
