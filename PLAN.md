# telldus-mqtt plan

Date: 2026-11-15. Replaces ARCHITECTURE_REVIEW.md (2025-11-30).

## Done
- #6 Old retained topics: cleared when a mapping is added (PR #9).
- #4 Device-1 vs Device-10 mapping: fixed in code. Issue closed.
- Tests: TellStick simulator and broker tests run with ctest (PR #7).
- Setup script for build dependencies (PR #7).
- Sensor offline time is configurable (`sensor-offline-seconds`).

## OpenWrt without config files
Goal: a new user sets up everything in LuCI. No file edits.

LuCI page:
- Sensors heard, with last values. The user types a name. The topic comes from the name (can be changed). A name publishes the sensor. Unnamed sensors stay filtered.
- Replace: move a name to a new sensor ID (after battery change).
- Button capture: press a remote button, see protocol/house/unit, make a switch from it.
- Status: service, TellStick, telldusd, broker, time since last RF message. Restart button.
- Broker settings: host, port, user, pass. Default 127.0.0.1:1883. Broker setup: link to luci-app-mosquitto (exists in openwrt/luci 23.05, 24.10, 25.12).

Design:
- Settings in UCI `/etc/config/telldus-mqtt`: one main section, one section per named sensor/switch. No Telldus-specific names where it costs nothing (a Zigbee gateway can use the same layout later).
- The daemon reads JSON. At start, procd runs a ucode script: UCI -> JSON. A migration script: telldus-mqtt.json -> UCI.
- The daemon writes a status file in /var/run: seen sensors, last values, recent raw RF events, connection state. LuCI reads it (MQTT has no unnamed sensors).
- ucode and a JS LuCI view. No Lua.

Location:
- This repo: daemon, `openwrt/` (generator, migration), `luci-app-telldus-mqtt/` (moves to openwrt/luci when the status format is stable).
- openwrt/packages `utils/telldus-mqtt`: Makefile, init script, default UCI config (Peter, later PR).

Steps, one PR each:
1. `--config <path>` option, with tests.
2. Status file: seen sensors, last values, connection state. Peter approves the format first.
3. Raw RF events in the status file.
4. UCI layout, generator, migration. Peter approves the layout first.
5. tellstick.conf from UCI.
6. LuCI page. Peter tests it on a router.

The openwrt/packages PR needs:
- Makefile: depends on ucode, ucode-mod-uci, ucode-mod-fs. Install `openwrt/telldus-mqtt-uci2json.uc` and `openwrt/telldus-mqtt-json2uci.uc` (e.g. in `/usr/share/telldus-mqtt/`). Conffile `/etc/config/telldus-mqtt`.
- Default `/etc/config/telldus-mqtt`: `config main 'main'` with enabled 1, host 127.0.0.1, port 1883, user/pass empty, offline_seconds 600, topic_prefix home.
- uci-defaults script: if telldus-mqtt.json exists, run `ucode .../telldus-mqtt-json2uci.uc /etc/telldus-mqtt/telldus-mqtt.json`. It does nothing if the UCI config has device sections.
- Init script: run `ucode .../telldus-mqtt-uci2json.uc /var/etc/telldus-mqtt.json` (writes mode 0600, exit code not 0 on error), start with `--config /var/etc/telldus-mqtt.json`, respawn, reload on UCI change. No `-` options after the script name (ucode reads them).
- Step 5, switches in tellstick.conf:
  - Makefile: also install `telldus-mqtt-uci2tellstick.uc`, `telldus-mqtt-tellstick2uci.uc` and `telldus-mqtt-tellstick.uc` (same directory).
  - Init script: before the daemon starts, run `ucode .../telldus-mqtt-uci2tellstick.uc`. If it prints `changed`, restart telldusd. If the exit code is not 0, log it and start without a change. Also run it on a UCI change (reload).
  - uci-defaults: after json2uci, run `ucode .../telldus-mqtt-tellstick2uci.uc /etc/tellstick.conf /etc/config "$serial"`. `serial` is from the status file of a daemon that ran before: `jsonfilter -i /var/run/telldus-mqtt.json -e '@.gateway.serial'`.
  - Limits: on a first install, there is no status file, so there is no serial. Then the import works only if each switch id has one section, or one section without `gateway`. Else it stops with an error and changes nothing; the user runs it again with the serial. The controller sections of tellstick.conf do not tell which TellStick is on this router. telldusd reads tellstick.conf only at start, and writes all of it from memory (for example for a new controller). Thus restart telldusd at once after `changed`: a telldusd with the old devices can write them back. Then the next run prints `changed` again and the restart corrects it. Not tested on a router.
- Remove `telldus-mqtt-watchdog.sh` when the status file has the time since the last RF message (the script has the topic "Hemma" hard-coded).

## Next
1. #5 Robust set: now one resend after 1 s. Find out if telldus-core repeats. If not, add more resends with a small random delay. Make the count a config value.
2. CI: done in `.github/workflows/build.yml` (ctest and OpenWrt build). Add more targets if necessary.
3. Exit codes: use one exit code per failure type (MQTT, telldusd, config). Update `analyze-logs.sh`.

## Maybe later
- MQTT TLS.
- Free device and sensor lists on exit (low value: process exits).

## Dropped
- Message queue when MQTT is down: restart recovers, sensors send again.
- State file: Home Assistant keeps state from retained topics.
- Signal quality, metrics, dashboard: no clear need.

## Keep
- Fail fast: `exit(1)` on fatal errors. systemd/procd and the watchdog restart the process.
