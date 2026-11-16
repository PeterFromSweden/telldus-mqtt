# OpenWrt scripts

The settings are in the UCI config `/etc/config/telldus-mqtt`. The daemon reads JSON, telldusd reads `/etc/tellstick.conf`. These ucode scripts convert between them. They need `ucode`, `ucode-mod-uci` and `ucode-mod-fs`.

## Files
- `telldus-mqtt-uci2json.uc`: the generator. It reads the UCI config and writes the daemon config file. Default output: `/var/etc/telldus-mqtt.json`, mode 0600 (it has the broker password).
- `telldus-mqtt-json2uci.uc`: the migration. It reads an old `/etc/telldus-mqtt/telldus-mqtt.json` and writes the UCI config. If the UCI config has `device` sections already, it does nothing. It sets `topic_prefix` to the most used prefix of the old topics (the topic without its last part, for example `Hemma` from `Hemma/Uteförråd`; if two are used equally often, the first in the file). The devices keep their topics.
- `telldus-mqtt-uci2tellstick.uc`: writes the `device` sections of `/etc/tellstick.conf` from the UCI switches. Only if `manage_switches` is `1`. All other content of the file stays (telldusd writes the `controller` sections). It writes the file only if the devices change, with the same file mode. It prints `changed`, `unchanged` or `off`. On an error it does not change the file.
- `telldus-mqtt-tellstick2uci.uc`: the import, one time. It copies the devices of `/etc/tellstick.conf` to the UCI switches, then sets `manage_switches` to `1`. If `manage_switches` is `1` already, it does nothing.
- `telldus-mqtt-tellstick.uc`: the tellstick.conf parser and writer for the two scripts. Install it in the same directory.
- `package/`: the draft OpenWrt package that uses the scripts (Makefile, init script, uci-defaults). See [package/README.md](package/README.md).
- `luci-app-telldus-mqtt/`: the LuCI pages. See [luci-app-telldus-mqtt/README.md](luci-app-telldus-mqtt/README.md).
- `tests/`: tests for the scripts. `ctest` runs them if `ucode` with the `uci` module is installed. With libconfuse, the tests also read the files as telldusd does.

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
For switches, the init script writes tellstick.conf before the daemon starts. telldusd reads the file only when it starts, so the init script restarts telldusd if the file changed:
```sh
[ "$(ucode /usr/share/telldus-mqtt/telldus-mqtt-uci2tellstick.uc)" = changed ] && /etc/init.d/telldusd restart
```
The uci-defaults script runs the import one time. The optional third argument is the serial of this TellStick:
```sh
ucode /usr/share/telldus-mqtt/telldus-mqtt-tellstick2uci.uc /etc/tellstick.conf /etc/config AE01DHPQ
```
Do not use options that start with `-` after the script name: `ucode` reads them. The optional second argument is a UCI config directory (for tests).

## Switches in tellstick.conf
```
config main 'main'
	option manage_switches '1'

config device
	option type 'switch'
	option id '2'
	option name 'Arbetsrum'
	option protocol 'arctech'
	option model 'selflearning-switch:nexa'
	option house '30341302'
	option unit '2'
```
- `manage_switches`: `1` = the switches in UCI are the devices in tellstick.conf. Missing or `0`: tellstick.conf is not changed.
- A `switch` section with `protocol` is a device in tellstick.conf. `id` is the device id (1 to 999999999). The ids must be different.
- `model` and the parameters `devices`, `house`, `unit`, `code`, `system`, `units`, `fade` are optional. They are the same as in tellstick.conf.
- A switch without `name` is in tellstick.conf, but it is not published to MQTT.
- A device that is in tellstick.conf already keeps its name, its `controller` and the order of its parameters. A new device gets the UCI `name` (or `switch <id>`) and no `controller` (telldusd uses 0). Thus a change of the UCI name does not change tellstick.conf.
- The import uses the switch section with the same id. If more sections have the id, it uses the section with `gateway` = the serial, else the section without `gateway`. If that is not clear, it changes nothing. With the serial, it does not use a section with another `gateway`. If there is no section, it adds a switch section without name.
- Comments in a device section of tellstick.conf are deleted. The devices go where the first device section was, with one blank line between them. The blank lines after an old device section are deleted. If the last old device section had a blank line after it, the last new one has one too.

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
- To change the prefix: `uci set telldus-mqtt.main.topic_prefix=Hemma; uci commit telldus-mqtt; /etc/init.d/telldus-mqtt reload`. Devices with a `topic` do not change.
- `gateway` is optional: the TellStick serial. Without it, the device is used with all TellSticks.
- The generator ignores other options (for example the RF options of a switch, see below).
