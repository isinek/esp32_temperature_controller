# ESP32 Temperature Controller

ESP32-based temperature controller for DS18B20 sensors on a 1-Wire bus.

The device starts a Wi-Fi access point and serves a small web interface for
viewing discovered sensors, assigning sensor numbers, and configuring min/max
temperature limits. It continuously checks each sensor against its configured
range and drives local alarm outputs when any sensor is too cold, too hot, or
unavailable.

## Hardware

Default target:

- Seeed Studio XIAO ESP32-C3
- DS18B20 temperature sensors
- Blue LED on GPIO 5
- Red LED on GPIO 6
- Buzzer on GPIO 7
- Reserved pump/output relay pin on GPIO 10
- 1-Wire data bus on GPIO 4

Default Wi-Fi access point:

- SSID: `TEMPERATURE_CONTROLLER`
- Password: `00000000`
- Web UI: `http://192.168.10.1/`
- JSON API: `http://192.168.10.1/api/sensors`

## Behavior

- Sensors are discovered from the DS18B20 1-Wire bus at startup or from the
  web UI rescan button.
- Each sensor can be assigned a display number and min/max temperature limits.
- Configuration is stored in ESP32 NVS preferences.
- Sensor readings are refreshed every 30 seconds.
- Blue LED turns on when at least one sensor is too cold.
- Red LED turns on when at least one sensor is too hot.
- Both LEDs turn on when there is a sensor read failure.
- Buzzer turns on for any cold, hot, or sensor-error condition.
- GPIO 10 is initialized as an output and left off. It is reserved for future
  relay/pump control logic.

Temperature readings outside `-10.0 C` to `60.0 C` are treated as invalid.
Alarm clearing uses `0.3 C` hysteresis.
Default limits for newly discovered sensors are `15.0 C` minimum and `18.0 C`
maximum.

## Web Interface

Connect to the controller access point and open `http://192.168.10.1/`.

The web interface shows:

- discovered sensor ROM addresses
- assigned sensor numbers
- current temperatures
- configured min/max limits
- current alarm state

The active root page is currently the Croatian interface. An English page exists
in the firmware and can be enabled by switching the root handler in
`setupWifiAccessPoint()`.

The JSON API at `/api/sensors` returns the same sensor state for external tools
or integrations.

## Build

This project uses `arduino-cli`.

```sh
make compile
```

## Upload

The default upload port is `/dev/ttyACM0`.

```sh
make upload
```

## Serial Monitor

```sh
make monitor
```

## Tests

Temperature validation and alarm logic are extracted into `TemperatureLogic.h` and covered
by a small host-side C++ test binary.

```sh
make test
```

The tests cover temperature validation, disconnected-sensor handling, alarm
transitions, hysteresis behavior, display labels, and ROM address hash stability.

## DS-1311WN Smart Switch Note

The DS-1311WN mini smart switch can be used as an external power switch for the
device being controlled. It is not the board running this firmware.

For stock DS-1311WN firmware, public information does not document a simple BLE
on/off control characteristic. Wi-Fi control is the more realistic path:

- flash the switch with OpenBeken and control it locally over HTTP or MQTT, or
- use Home Assistant/Tuya/Cozylife as a bridge and let this ESP32 call that
  bridge.

If OpenBeken is used, this firmware can later add relay/pump control over HTTP
or MQTT instead of using the reserved local `PUMP_PIN`.
