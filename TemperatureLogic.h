#pragma once

#include <stdint.h>

constexpr int MAX_SENSORS = 10;

constexpr float DEFAULT_MIN_TEMP = 15.0f;
constexpr float DEFAULT_MAX_TEMP = 18.0f;

// Fixed hysteresis around each configured limit.
// Example:
//   min = 6.0 C
//   cold alarm turns ON below 6.0 C
//   cold alarm clears above 6.3 C
//
//   max = 15.0 C
//   hot alarm turns ON above 15.0 C
//   hot alarm clears below 14.7 C
constexpr float ALARM_HYSTERESIS = 0.3f;

constexpr float VALID_MIN_TEMP = -10.0f;
constexpr float VALID_MAX_TEMP = 60.0f;

constexpr float SENSOR_DISCONNECTED_C = -127.0f;

enum class AlarmState : uint8_t {
  OK,
  TOO_COLD,
  TOO_HOT,
  SENSOR_ERROR
};

inline uint32_t hashRomAddress(const uint8_t* address) {
  uint32_t hash = 2166136261UL;

  for (int i = 0; i < 8; i++) {
    hash ^= address[i];
    hash *= 16777619UL;
  }

  return hash;
}

inline bool isValidTemperature(float temperature) {
  if (temperature == SENSOR_DISCONNECTED_C) {
    return false;
  }

  if (temperature < VALID_MIN_TEMP || temperature > VALID_MAX_TEMP) {
    return false;
  }

  return true;
}

template <typename SensorLike>
void updateSensorAlarmState(SensorLike& sensor) {
  if (!sensor.valid) {
    sensor.alarmState = AlarmState::SENSOR_ERROR;
    return;
  }

  switch (sensor.alarmState) {
    case AlarmState::TOO_COLD:
      if (sensor.temperature > sensor.minTemp + ALARM_HYSTERESIS) {
        sensor.alarmState = AlarmState::OK;
      }
      break;
    case AlarmState::TOO_HOT:
      if (sensor.temperature < sensor.maxTemp - ALARM_HYSTERESIS) {
        sensor.alarmState = AlarmState::OK;
      }
      break;
    case AlarmState::SENSOR_ERROR:
    case AlarmState::OK:
    default:
      if (sensor.temperature < sensor.minTemp) {
        sensor.alarmState = AlarmState::TOO_COLD;
      } else if (sensor.temperature > sensor.maxTemp) {
        sensor.alarmState = AlarmState::TOO_HOT;
      } else {
        sensor.alarmState = AlarmState::OK;
      }
      break;
  }
}

inline const char* alarmStateToText(AlarmState state) {
  switch (state) {
    case AlarmState::TOO_COLD:
      return "TOO COLD";
    case AlarmState::TOO_HOT:
      return "TOO HOT";
    case AlarmState::SENSOR_ERROR:
      return "SENSOR ERROR";
    case AlarmState::OK:
    default:
      return "OK";
  }
}

inline const char* stateCssClass(AlarmState state) {
  switch (state) {
    case AlarmState::TOO_COLD:
      return "cold";
    case AlarmState::TOO_HOT:
      return "hot";
    case AlarmState::SENSOR_ERROR:
      return "error";
    case AlarmState::OK:
    default:
      return "ok";
  }
}
