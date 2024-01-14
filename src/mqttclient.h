#ifndef MQTTCLIENT_H_
#define MQTTCLIENT_H_

#include "telldussensor.h"
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

void MqttClient_Init(MqttClient* self);
void MqttClient_Destroy(MqttClient* self);
bool MqttClient_IsConnected(MqttClient *self);
int MqttClient_Connect(MqttClient *self);
void MqttClient_Disconnect(MqttClient *self);
void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor);

#endif // MQTTCLIENT_H_