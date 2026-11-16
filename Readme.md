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
`/etc/telldus-mqtt/telldus-mqtt.json`. The installed file has no devices: all sensors and switches are published with the default topics.
```json
{
  "host": "127.0.0.1",
  "port": 1883,
  "user": "",
  "pass": "",
  "sensor-offline-seconds": 600,
  "devices": []
}
```
- `sensor-offline-seconds`: a sensor is marked offline when silent this long.
- `topic-translation` (optional): replaces the long topics with your own; `name` is the Home Assistant name.
  ```json
  "topic-translation": [
    { "telldus": "telldus/A703AKOX/switch/2", "mqtt": "Home/Office", "name": "Office" }
  ]
  ```
- `devices` (optional): names a device without the TellStick serial in the key. `type` is `sensor` or `switch`, `id` is the sensor id or the switch device number. `gateway` (optional) is a TellStick serial: the entry is then used only with that TellStick. If an entry in `devices` and in `topic-translation` match, `devices` is used.
  ```json
  "devices": [
    { "type": "sensor", "id": "fineoffset_temperaturehumidity_135", "name": "Shed", "topic": "Home/Shed" },
    { "type": "switch", "id": "2", "name": "Office", "topic": "Home/Office", "gateway": "A703AKOX" }
  ]
  ```
- `filter-non-transled` (optional): `"yes"` publishes only the sensors and switches with an entry in `devices` or `topic-translation`. The status file (`--status`) shows all of them. Without it, all are published.

On OpenWrt the settings are in UCI (`/etc/config/telldus-mqtt`). The init script makes the JSON file from UCI. See [packaging/openwrt/README.md](packaging/openwrt/README.md).

`/etc/telldus-mqtt/telldus-mqtt-homeassistant.json` holds the discovery templates and normally needs no changes.

## Run
```bash
telldus-mqtt [--nodaemon] [--debug] [--logtime] [--raw] [--config <path>] [--status <path>] [--version]
```
`--config <path>` reads this config file. Default: `telldus-mqtt.json` in `/etc/telldus-mqtt`.
`--status <path>` writes a JSON status file: connection state, the sensors and switches heard with their last values, and the last remote control commands heard. Example: `/var/run/telldus-mqtt.json`.
`--raw` logs raw RF messages, useful for finding remote controls.

## Install
### OpenWrt
```bash
opkg update && opkg install telldus-mqtt
```
The service starts right away. Logs: `logread | grep telldus-mqtt`.

### Linux (Ubuntu 22.04+)
```bash
sudo apt install build-essential cmake pkg-config libmosquitto-dev libconfuse-dev libftdi-dev libcjson-dev

git clone https://github.com/PeterFromSweden/telldus.git
cmake -S telldus/telldus-core -B telldus/build
cmake --build telldus/build && sudo cmake --install telldus/build   # installs to /usr/local

cmake -B build && cmake --build build && sudo cmake --install build
```
- A broker is necessary. If you have none: `sudo apt install mosquitto`.
- The telldus-core install writes a new `/etc/tellstick.conf`. Save your old file first.
- `telldusd` must be able to write its files. It runs as user `nobody`, group `plugdev`:
  ```bash
  sudo chgrp plugdev /etc/tellstick.conf && sudo chmod 664 /etc/tellstick.conf
  sudo mkdir -p /tmp/state && sudo chown -R nobody:plugdev /tmp/state
  ```
  Without this, the log says `Unable to open config file with write access` and the TellStick is not found.
- Start `telldusd`: `sudo telldusd`. Check with `tdtool --list-sensors`.
- The config file is `/usr/local/etc/telldus-mqtt/telldus-mqtt.json`. Set the broker there.
- Start: `telldus-mqtt --nodaemon`. The log shows which config file is used.
- To find the sensor ids: `telldus-mqtt --nodaemon --status /tmp/telldus-mqtt.json`, then read that file.
- To start at boot, make a systemd unit that runs `telldus-mqtt --nodaemon`.
- If the library is not found: `sudo ldconfig`.

Or run `scripts/setup-linux-deps.sh` to install the dependencies for build and test. Run the tests with `cd build && ctest`. The tests use a simulated TellStick, so no hardware is needed.

### Windows 11
Get the zip `telldus-mqtt-<version>-win64` from the "Windows zip" workflow on the [Actions](https://github.com/PeterFromSweden/telldus-mqtt/actions) page. Unpack it. Install [TelldusCenter 2.1.2](http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe) (driver, Telldus Service and `TelldusCore.dll`) and use an MQTT broker. Then follow `README.txt` in the zip.

To build from source:
1. Install [vcpkg](https://vcpkg.io/en/getting-started.html), then `vcpkg install cJson pthreads && vcpkg integrate install`.
2. Install [Mosquitto](https://mosquitto.org/download/) and [TelldusCenter 2.1.2](http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe).
3. Build with `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, then `cmake --install build` as administrator.
