#ifndef MQTTCLIENT_H_
#define MQTTCLIENT_H_

#include "telldussensor.h"
#include "telldusdevice.h"
#include "config.h"
#include "mosquitto.h"

typedef enum {
  TM_MQCONN_NONE,
  TM_MQCONN_START,
  TM_MQCONN_OK
} TMqConn;

typedef struct
{
  struct mosquitto* mosq;
  Config* config;
  TMqConn mqconn;
  bool mutelog;
} MqttClient;

MqttClient* MqttClient_GetInstance(void);
void MqttClient_Destroy(MqttClient* self);
TMqConn MqttClient_GetConnection(MqttClient *self);
int MqttClient_Connect(MqttClient *self);
void MqttClient_Disconnect(MqttClient *self);
void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor);
void MqttClient_SensorValue(MqttClient* self, TelldusSensor* sensor);
void MqttClient_SensorOnline(MqttClient* self, TelldusSensor* sensor, bool online);
void MqttClient_AddDevice(MqttClient* self, TelldusDevice* device);
void MqttClient_DeviceValue(MqttClient* self, TelldusDevice* device);

#endif // MQTTCLIENT_H_