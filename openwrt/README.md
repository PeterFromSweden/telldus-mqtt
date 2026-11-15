# OpenWrt scripts

The settings are in the UCI config `/etc/config/telldus-mqtt`. The daemon reads JSON. These ucode scripts convert between the two. They need `ucode`, `ucode-mod-uci` and `ucode-mod-fs`.

## Files
- `telldus-mqtt-uci2json.uc`: the generator. It reads the UCI config and writes the daemon config file. Default output: `/var/etc/telldus-mqtt.json`, mode 0600 (it has the broker password).
- `telldus-mqtt-json2uci.uc`: the migration. It reads an old `/etc/telldus-mqtt/telldus-mqtt.json` and writes the UCI config. If the UCI config has `device` sections already, it does nothing.
- `tests/`: tests for the two scripts. `ctest` runs them if `ucode` with the `uci` module is installed.

## Use
The init script runs the generator, then starts the daemon:
```sh
ucode /usr/share/telldus-mqtt/telldus-mqtt-uci2json.uc /var/etc/telldus-mqtt.json || return 1
procd_set_param command /usr/bin/telldus-mqtt --nodaemon --config /var/etc/telldus-mqtt.json
```
A uci-defaults script runs the migration one time:
```sh
[ -f /etc/telldus-mqtt/telldus-mqtt.json ] && ucode /usr/share/telldus-mqtt/telldus-mqtt-json2uci.uc /etc/telldus-mqtt/telldus-mqtt.json
```
Do not use options that start with `-` after the script name: `ucode` reads them. The optional second argument is a UCI config directory (for tests).

## UCI layout
```
config main 'main'
	option enabled '1'
	option host '127.0.0.1'
	option port '1883'
	option user ''
	option pass ''
	option offline_seconds '600'
	option topic_prefix 'home'

config device
	option type 'sensor'
	option id 'fineoffset_temperaturehumidity_135'
	option name 'Uteförråd'
	option topic 'Hemma/Uteförråd'
	option gateway 'AE01DHPQ'
```
- `type`: `sensor` or `switch`. `id`: the sensor id, or the telldus device number of a switch.
- `name` is necessary. Only devices with a name are published.
- `topic` is optional. Default: `<topic_prefix>/<name>`, with `/`, `+` and `#` in the name changed to `_`.
- `gateway` is optional: the TellStick serial. Without it, the device is used with all TellSticks.
- The generator ignores other options (for example the RF options of a switch).
