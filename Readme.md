# telldus-core-mqtt
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

### Build telldus-core-mqtt
```bash
cd <telldus-core-mqtt>
cmake -B build
cmake --build build
```
Run telldus-core-mqtt...

### Build telldus-core-mqtt
```bash
sudo cmake --install build
```

### Install cJson
```bash
sudo apt install libcjson-dev
```

## Windows 11 / Visual Studio 2022
### Install vcpkg
https://vcpkg.io/en/getting-started.html
```batch
git clone https://github.com/Microsoft/vcpkg.git
vcpkg\bootstrap-vcpkg.bat
```

### Install mosquitto prerequsity cJson
```batch
vcpkg install cJson
vcpkg integrate install
```

### Install mosquitto prerequsity pthreads
```batch
vcpkg install pthreads
vcpkg integrate install
```

### Install mosquitto prerequsity openssl
choco install openssl

### Build/install mosquitto
```batch
git clone https://github.com/eclipse/mosquitto.git
```
Open *folder* mosquitto in Visual Studio  
Edit CMakeLists.txt:70
```
	#find_package(Threads REQUIRED)
	set (PTHREAD_LIBRARIES "C:/Users/Peter/Downloads/vcpkg/packages/pthreads_x64-windows/lib/pthreadVC3.lib")
	set (PTHREAD_INCLUDE_DIR "C:/Users/Peter/Downloads/vcpkg/packages/pthreads_x64-windows/include")
```
In visual studio build and install

Install locations  
+ mosquitto/out/install

### Install telldus
TelldusCenter-2.1.2.exe 
http://download.telldus.com/TellStick/Software/TelldusCenter/TelldusCenter-2.1.2.exe  
NOTE: Browser tricks needed nowadays to download non-https links!


# Build telldus-core-mqtt
```bash
cd <telldus-core-mqtt>
cmake -B build
cmake --build build
```
Run telldus-core-mqtt...

### Build telldus-core-mqtt
```bash
sudo cmake --install build
```

