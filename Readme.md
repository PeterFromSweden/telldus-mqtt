# telldus-mqtt
Telldus - MQTT converter
Home assistant compatible.
Optional user defined MQTT topic mapping still wtih home assistant support.

# Configuration
There are two json files:
```bash
/etc/telldus-mqtt/telldus-mqtt.json
/etc/telldus-mqtt/telldus-mqtt-homeassistant.json
```
The home assistant topics are required to self detect telldus activity, but the default topics are very long and not mapped to its physical meaning.  
To do this there is topic-translation that maps the home automation telldus topics to user defined topics that has a physical meaning. Additional short form name for correspondance to how sensor/device should be presented in home assistant.  
  
There is an alternate approach if the mqtt topics are of no interest, to just live with the long topics and add name in home assistant software instead.
  
## MQTT broker
The file
```bash
/etc/telldus-mqtt/telldus-mqtt.json
```
Has host, username and password for MQTT broker that must be configured.

# Build / Install
## Openwrt install
```bash
opkg update
opkg install telldus-mqtt
```
To test-run telldus-mqtt and check output run
```bash
telldus-mqtt --nodaemin --debug
```
To enable service  run
```bash
service telldus-mqtt enable
service telldus-mqtt start
```

To check service output
```bash
logread | grep telldus-mqtt
```

## Ubuntu 20.04 build
### Install mosquitto
```bash
sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa
sudo apt-get update
sudo apt-get install mosquitto mosquitto-dev mosquitto-clients
```
Install locations  
+ /usr/bin/
+ /usr/sbin/
+ /usr/includemosquitto.h 
+ /etc/mosquitto/

### Build and install telldus-core
```bash
git clone git@github.com:PeterFromSweden/telldus.git
cd telldus/telldus-core
cmake -B build
cmake --build build
sudo cmake --install build

```
Install locations
+ /usr/local

To fix library path (if troublesome)
```bash
export LD_LIBRARY_PATH=/usr/local/lib
```

### Build telldus-mqtt
```bash
cd <telldus-mqtt>
cmake -B build
cmake --build build
```
Run telldus-mqtt...

### Build telldus-mqtt
```bash
sudo cmake --install build
```

### Install cJson
```bash
sudo apt install libcjson-dev
```

## Windows 11 / Visual Studio 2022 - Not supported at present
### Install vcpkg
https://vcpkg.io/en/getting-started.html
```batch
c:\
git clone https://github.com/Microsoft/vcpkg.git
vcpkg\bootstrap-vcpkg.bat
```

### Install prerequsities
(pthreads may be autoinstalled as dependancy to mosquitto?)
```batch
vcpkg install cJson
vcpkg install pthreads
vcpkg install mosquitto
vcpkg integrate install
```
settings.json:  
```
"cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    },
```

### Mosquitto alternative
vcpkg install above is slow and MAY have problem with threads?
There is an installer that can be used as alternative

### Install telldus
TelldusCenter-2.1.2.exe 
http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe  
NOTE: Browser tricks needed nowadays to download non-https links!

# Build telldus-mqtt vscode
Cmake extension used for building and debugging.

# Build telldus-mqtt cli
```bash
cd <telldus-mqtt>
cmake -B build
cmake --build build
```
Run telldus-mqtt...

### Build telldus-mqtt
```bash
sudo cmake --install build
```
