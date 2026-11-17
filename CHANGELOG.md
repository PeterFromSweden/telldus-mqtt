# Changelog

## 0.5 (2026-11-16)

New:
- OpenWrt: setup without config-file edits. The settings are in UCI
  (`/etc/config/telldus-mqtt`). An old JSON config and `tellstick.conf` are
  migrated at the first start.
- OpenWrt: LuCI page (package `luci-app-telldus-mqtt`) with Status, Sensors,
  Switches and Settings. Switches have On and Off buttons.
- Options `--config` and `--status`. The status file (JSON) shows the
  connection state and all seen devices, also raw RF events.
- Windows: ready zip (x64) from the CI build.

Changed:
- Only sensors with a name are published. Version 0.4 published all sensors.
- The config file is also searched in `/usr/local/etc/telldus-mqtt/`.
- The log is written at once, also when the output goes to a file.

Known limits:
- A sensor keeps `online` after the program stops.
- The On and Off buttons are not tested on OpenWrt 22.03.

## 0.4.1 (2026-10-07)

- Option `--version`.

## 0.4 (2026-10-06)

- Old retained default topics are cleared when a topic is translated.
- Topic translation replaces only complete words.
- Device state fixes.
- TellStick simulator and end-to-end test.
- Shorter README.

## 0.2 (2024-02-10)

- First release.
