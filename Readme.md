# telldus-mqtt
Bridge between Telldus 433 MHz devices and MQTT, with Home Assistant MQTT Discovery.

- Requires a TellStick (e.g. TellStick Duo) and telldus-core (`telldusd`).
- Sensors (temperature, humidity, rain, wind) are published automatically.
- Switches support on/off only and must be defined in telldus-core (`tellstick.conf`).

## MQTT topics
| | Topic |
|---|---|
| Sensor value | `telldus/{serno}/sensor/{protocol}_{model}_{id}/{datatype}` |
| Switch state | `telldus/{serno}/switch/{device_no}/state` |
| Switch command | `telldus/{serno}/switch/{device_no}/set` (`ON`/`OFF`) |

## Configuration
`/etc/telldus-mqtt/telldus-mqtt.json`:
```json
{
  "host": "127.0.0.1",
  "port": 1883,
  "user": "",
  "pass": "",
  "sensor-offline-seconds": 600,
  "topic-translation": [
    { "telldus": "telldus/A703AKOX/switch/2", "mqtt": "Home/Office", "name": "Office" }
  ]
}
```
- `sensor-offline-seconds`: a sensor is marked offline when silent this long.
- `topic-translation` (optional): replaces the long topics with your own; `name` is the Home Assistant name.

`/etc/telldus-mqtt/telldus-mqtt-homeassistant.json` holds the discovery templates and normally needs no changes.

## Run
```bash
telldus-mqtt [--nodaemon] [--debug] [--logtime] [--raw]
```
`--raw` logs raw RF messages, useful for finding remote controls.

## Install
### OpenWrt
```bash
opkg update && opkg install telldus-mqtt
```
The service starts right away. Logs: `logread | grep telldus-mqtt`.

### Linux (Ubuntu 22.04+)
```bash
sudo apt install mosquitto libmosquitto-dev pkg-config libconfuse-dev libftdi-dev libcjson-dev

git clone https://github.com/PeterFromSweden/telldus.git
cmake -S telldus/telldus-core -B telldus/build
cmake --build telldus/build && sudo cmake --install telldus/build   # installs to /usr/local

cmake -B build && cmake --build build && sudo cmake --install build
```
If the library is not found: `export LD_LIBRARY_PATH=/usr/local/lib`.

Or run `scripts/setup-linux-deps.sh` to install all of the dependencies above. Run the tests with `cd build && ctest`. The tests use a simulated TellStick, so no hardware is needed.

### Windows 11
1. Install [vcpkg](https://vcpkg.io/en/getting-started.html), then `vcpkg install cJson pthreads && vcpkg integrate install`.
2. Install [Mosquitto](https://mosquitto.org/download/) and [TelldusCenter 2.1.2](http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe).
3. Build with `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, then `cmake --install build` as administrator.
