# luci-app-telldus-mqtt

LuCI pages for telldus-mqtt, under Services > Telldus MQTT:
- Status: service, TellStick, telldusd, MQTT broker, last radio message. Restart button.
- Sensors: the sensors heard, with values and age. Type a name to publish a sensor. The MQTT topic field: empty = `<topic prefix>/<name>`, or type a topic. Remove name: the sensor is not published. Replace: give the name of an old sensor to a new sensor id (after a battery change).
- Switches: name the switches of telldusd. On and Off buttons switch a device now (`tdtool`, no Save & Apply). Capture a remote control button and make a switch from it.
- Settings: broker address, port, user, password, topic prefix, offline time, manage switches.

JavaScript views and a ucode rpcd plugin. No Lua. OpenWrt 23.05 or newer.

## What it needs from the telldus-mqtt package
- UCI config `/etc/config/telldus-mqtt` (see [../README.md](../README.md)).
- Init script `/etc/init.d/telldus-mqtt`. It reloads the daemon when the UCI config changes.
- The daemon writes the status file `/var/run/telldus-mqtt.json` (option `--status /var/run/telldus-mqtt.json`).
- `tdtool` (from telldus-core) for the On and Off buttons.

## TellSticks (gateway option)
The pages use the TellStick serial of the status file. They show and change only the device sections with no `gateway` or with `gateway` = this serial. Sections for other TellSticks are not changed. A new section gets `gateway` = the serial if some device section has a `gateway` option, else it gets no `gateway`.

## Test on a router without a package build
From the repository root:
```sh
R=root@192.168.1.1
scp -r packaging/openwrt/luci-app-telldus-mqtt/htdocs/luci-static/resources/view/telldus-mqtt $R:/www/luci-static/resources/view/
scp -r packaging/openwrt/luci-app-telldus-mqtt/htdocs/luci-static/resources/telldus-mqtt $R:/www/luci-static/resources/
scp packaging/openwrt/luci-app-telldus-mqtt/root/usr/share/luci/menu.d/luci-app-telldus-mqtt.json $R:/usr/share/luci/menu.d/
scp packaging/openwrt/luci-app-telldus-mqtt/root/usr/share/rpcd/acl.d/luci-app-telldus-mqtt.json $R:/usr/share/rpcd/acl.d/
ssh $R mkdir -p /usr/share/rpcd/ucode
scp packaging/openwrt/luci-app-telldus-mqtt/root/usr/share/rpcd/ucode/luci.telldus-mqtt $R:/usr/share/rpcd/ucode/
ssh $R 'rm -rf /tmp/luci-indexcache* /tmp/luci-modulecache; /etc/init.d/rpcd restart'
```
Then log in to LuCI again. If the router has no `scp` server (dropbear without sftp), use `scp -O`.

## Tests
`node tests/check-syntax.js` parses the JS and JSON files. `node tests/test-logic.js` tests `telldus-mqtt/logic.js` (topics, merge with the gateway rule, ages, Replace, button capture, switch ids). `tests/smoke-views.js` runs the views with a small fake LuCI; it needs the npm package jsdom (`NODE_PATH=<dir>/node_modules`). The `tests` directory is not installed.
