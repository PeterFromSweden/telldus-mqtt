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