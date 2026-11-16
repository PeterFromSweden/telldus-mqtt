telldus-mqtt for Windows (64-bit)
=================================

telldus-mqtt connects a TellStick (for example TellStick Duo) to an MQTT
broker. It publishes sensor values and lets you turn switches on and off.
Home Assistant finds the sensors and switches with MQTT Discovery.

1. Install other software
-------------------------
- TelldusCenter 2.1.2. It installs the TellStick driver, the Telldus
  Service and TelldusCore.dll. telldus-mqtt.exe needs TelldusCore.dll.
  http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe
- An MQTT broker, if you do not have one. For example Mosquitto
  (https://mosquitto.org/download/) or the Mosquitto add-on in Home Assistant.

This folder already has all other DLLs (mosquitto, cJSON, the libraries
that they use and the Microsoft C runtime).

2. Connect the TellStick
------------------------
Connect the TellStick. Start TelldusCenter and make sure that it shows the
TellStick. Add your switches (remote controlled outlets) in TelldusCenter.
Sensors need no setup. Close TelldusCenter. The Telldus Service continues
to run.

3. Set the broker
-----------------
Edit telldus-mqtt.json in this folder (for example with Notepad):
  "host": "127.0.0.1"   the IP address or name of the MQTT broker
  "port": 1883
  "user", "pass"        leave empty if the broker has no login
Names and topics (optional):
  "devices": [
    { "type": "sensor", "id": "fineoffset_temperaturehumidity_135",
      "name": "Shed", "topic": "Home/Shed" },
    { "type": "switch", "id": "2", "name": "Office", "topic": "Home/Office" }
  ]
  "name" is the name in Home Assistant. "topic" replaces the long MQTT topic.
  To find the ids: run one time with "--status status.json" and read
  that file. The switch ids are the numbers in TelldusCenter.
  Devices that are not in the list are published too, with the topic
  telldus/<serial>/... and in Home Assistant. To publish only the
  devices in the list, add:  "filter-non-transled": "yes"
More information: https://github.com/PeterFromSweden/telldus-mqtt

telldus-mqtt-homeassistant.json has the Home Assistant templates. Usually
you do not change it.

4. Run
------
Open a Command Prompt in this folder and run:
  telldus-mqtt.exe --nodaemon
Stop it with Ctrl+C. Other options:
  --debug          more log messages
  --raw            log the raw radio messages (to find remote controls)
  --config <path>  use this config file
  --status <path>  write a JSON status file
  --version        show the version

The program looks for telldus-mqtt.json and telldus-mqtt-homeassistant.json
in the current folder, then in ..\ and then in ..\etc\telldus-mqtt\.
Thus start it from this folder. --config changes only the place of
telldus-mqtt.json.

The program stops if it loses the connection to the broker or to the
Telldus Service. To start it again automatically, use for example the
Windows Task Scheduler (trigger "At startup", "Start in" = this folder).

Problems
--------
- "TelldusCore.dll was not found": install TelldusCenter 2.1.2.
- "Telldus error" or "Telldus connect error": make sure that the Telldus
  Service runs (services.msc) and that TelldusCenter shows the TellStick.
- "Cannot load config": telldus-mqtt.json is not in the current folder.

Licenses
--------
telldus-mqtt: see LICENSE.txt. The DLLs: see the licenses folder.
