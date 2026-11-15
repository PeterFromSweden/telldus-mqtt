# OpenWrt package (draft)

This is the draft of `utils/telldus-mqtt` in openwrt/packages. Copy `Makefile` and `files/` there. Before the PR, set `PKG_SOURCE_VERSION` to the release commit and get `PKG_MIRROR_HASH` with `make package/telldus-mqtt/check FIXUP=1`.

## Files
- `Makefile`: builds the daemon. Installs the five scripts of `../` in `/usr/share/telldus-mqtt/`.
- `files/telldus-mqtt.init`: `/etc/init.d/telldus-mqtt` (procd). At start:
  1. If `main.enabled` is `0`, it does not start.
  2. If the tellstick.conf import is pending (see below), it runs the import with the serial from `/var/run/telldus-mqtt.json`.
  3. `uci2tellstick`: if it prints `changed`, it restarts telldusd. On an error, it logs and continues.
  4. `uci2json` writes `/var/etc/telldus-mqtt.json`. On an error, it logs and does not start.
  5. Starts `telldus-mqtt --nodaemon --config /var/etc/telldus-mqtt.json --status /var/run/telldus-mqtt.json`.

  A UCI change (LuCI Save & Apply) runs these steps again. procd restarts the daemon if the generated file changed.
- `files/telldus-mqtt.config`: the default `/etc/config/telldus-mqtt`.
- `files/telldus-mqtt.defaults`: `/etc/uci-defaults/telldus-mqtt`. It copies an old `/etc/telldus-mqtt/telldus-mqtt.json` to UCI (`json2uci`), then imports the devices of `/etc/tellstick.conf` (`tellstick2uci`). It never fails and is safe to run again.
- `files/telldus-mqtt.json`: a short JSON without devices. It is only for the upgrade from 0.4: opkg keeps a changed conffile only if the new package has the file. The daemon does not read it.

## Import of tellstick.conf
The import needs the TellStick serial if more TellSticks have sections with the same switch id. At the first install there is no status file, thus no serial. Then the import stops and changes nothing, and the uci-defaults script makes `/etc/telldus-mqtt/tellstick-import-pending`. The init script runs the import again at each start when the status file has a serial. The status file is in `/var/run`, so do a restart after the daemon found the TellStick. To stop the retries, delete the file.

## Test packages from CI
The CI job `openwrt` builds `telldus-mqtt_*.ipk` and `luci-app-telldus-mqtt_*.ipk`. Download the artifact for your SDK (for example `telldus-mqtt-mvebu-cortexa9-23.05.2`) and unzip it.

Install (router at 192.168.1.1):
```sh
scp -O telldus-mqtt_*.ipk luci-app-telldus-mqtt_*.ipk root@192.168.1.1:/tmp/
ssh root@192.168.1.1
tar czf /root/telldus-backup.tgz /etc/telldus-mqtt /etc/tellstick.conf
opkg update
opkg install /tmp/telldus-mqtt_*.ipk /tmp/luci-app-telldus-mqtt_*.ipk
logread -e telldus-mqtt
uci show telldus-mqtt
```
If `/etc/telldus-mqtt/tellstick-import-pending` exists, wait until the daemon found the TellStick, then:
```sh
jsonfilter -i /var/run/telldus-mqtt.json -e '@.gateway.serial'
/etc/init.d/telldus-mqtt restart
logread -e telldus-mqtt | tail
```

Go back to 0.4:
```sh
opkg remove luci-app-telldus-mqtt telldus-mqtt
opkg install telldus-mqtt
tar xzf /root/telldus-backup.tgz -C /
/etc/init.d/telldusd restart
/etc/init.d/telldus-mqtt restart
```
`opkg remove` keeps changed conffiles. The restore of the backup puts back the old JSON and tellstick.conf. `/etc/config/telldus-mqtt` stays; 0.4 does not read it.
