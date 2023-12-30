# TelldusCodeMQTT
Embryo of telldus - MQTT converter

## Ubuntu 20.04
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

### Build TelldusCoreMQTT
```bash
cd <TelldusCoreMQTT>
cmake -B build
cmake --build build
```
Run TelldusCoreMQTT...

### Build TelldusCoreMQTT
```bash
sudo cmake --install build
```

## Windows 11 / Visual Studio 2022
### Build/install mosquitto
```batch
choco install openssl
git clone https://github.com/eclipse/mosquitto.git
cd mosquitto
cmake -B build -DWITH_THREADING=OFF
cmake --build build
```
As admin
```batch
cmake --install build --config Debug
```

Install locations  
+ C:\Program files(x86)\mosquitto

### Install telldus
TelldusCenter-2.1.2.exe 
http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe  
NOTE: Browser tricks needed nowadays to download non-https links!


### Build TelldusCoreMQTT
```bash
cd <TelldusCoreMQTT>
cmake -B build
cmake --build build
```
Run TelldusCoreMQTT...

### Build TelldusCoreMQTT
```bash
sudo cmake --install build
```