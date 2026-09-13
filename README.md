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
- Each sensor keeps a rolling 24-hour temperature history in RAM.
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
- 24-hour temperature history graphs
- configured min/max limits
- current alarm state

The active root page is currently the Croatian interface. An English page exists
in the firmware and can be enabled by switching the root handler in
`setupWifiAccessPoint()`.

The JSON API at `/api/sensors` returns the current sensor state for external
tools or integrations. Per-sensor history is available from
`/api/history?index=0`, with samples returned from oldest to newest and `null`
used for invalid readings.

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

### Confirmed CB2S / Raspberry Pi 5 flashing setup

A DS-1311WN with a CB2S (BK7231N) module was successfully flashed with
OpenBeken using a Raspberry Pi 5:

- The [`hid_download_py`](https://github.com/OpenBekenIOT/hid_download_py)
  UART flasher was run on the Raspberry Pi.
- The OpenBeken firmware binary was obtained from the
  [`OpenBK7231T_App` releases](https://github.com/openshwprojects/OpenBK7231T_App).
- The Raspberry Pi UART was configured to use the 40-pin GPIO header rather
  than its default UART route, then connected to the CB2S UART pins with TX/RX
  crossed and a common ground.
- The CB2S was powered from the Raspberry Pi's 3.3 V header pin.

For Raspberry Pi OS on a Raspberry Pi 5, add the following to
`/boot/firmware/config.txt` and reboot:

```ini
enable_uart=1
dtoverlay=uart0-pi5
```

In `sudo raspi-config`, select **Interface Options → Serial Port**, answer
**No** to the login-shell/serial-console prompt, and **Yes** to enable serial
hardware. `uart0-pi5` exposes UART0 on the 40-pin header: GPIO14/pin 8 is TX,
GPIO15/pin 10 is RX. Use `/dev/ttyAMA0` with the flasher; on a Pi 5,
`/dev/serial0` normally refers to the separate debug UART instead.

The switch must remain completely disconnected from AC power throughout
programming. Do not connect any 5 V UART signal to the CB2S.

### Controlling OpenBeken from the temperature controller

The firmware controls OpenBeken locally over HTTP. Configure OpenBeken to join
the `TEMPERATURE_CONTROLLER` access point, give it a fixed address of
`192.168.10.2`, and configure its relay as channel 1. If another address is
used, update `OPENBEKEN_IP` near the top of `temperature_controller.ino` before
building.

The controller web page provides **Manual ON**, **Manual OFF**, and **AUTO**
buttons. The selected mode is retained in NVS across ESP32 restarts. AUTO uses
each sensor's configured temperature limits:

- Any sensor below its minimum limit, any invalid sensor, or no discovered
  sensors turns the CB2S relay **OFF**.
- Otherwise, any sensor above its maximum limit turns the relay **ON**.
- If every sensor is within range, the relay keeps its most recently confirmed
  state.

OFF has priority when sensors disagree. This avoids powering the load when a
sensor is too cold or cannot be read. Commands use OpenBeken's
Tasmota-compatible HTTP endpoint, for example
`http://192.168.10.2/cm?cmnd=POWER%20ON`.
