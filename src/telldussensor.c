#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <telldus-core.h>
#include "log.h"
#include "mqttclient.h"
#include "telldussensor.h"


typedef struct SensorNodeS SensorNode;

struct SensorNodeS {
  TelldusSensor sensor;
  SensorNode* next;
};

SensorNode* sensors;

static char* TelldusSensor_DataTypeToString(int dataType);
static char* TelldusSensor_DataTypeToUnit(int dataType);
static void myTimerCallback(MyTimer* myTimer);

TelldusSensor* TelldusSensor_Create(const char *protocol, const char *model, int id, int dataType)
{
  SensorNode* sensorNode = sensors;
  SensorNode* lastSensorPtr;
  char str[80];
  char idStr[10];
  char dataTypeStr[20];
  sprintf(idStr, "%i", id);
  strcpy(dataTypeStr, TelldusSensor_DataTypeToString(dataType));
  
  // Find existing sensor
  while ( sensorNode != NULL )
  {
    TelldusSensor* sensor = &sensorNode->sensor;
    if( strcmp(sensor->protocol, protocol) == 0 && 
        strcmp(sensor->model, model) == 0 &&
        strcmp(sensor->id, idStr) == 0 &&
        strcmp(sensor->dataType, dataTypeStr) == 0 )
    {
      Log(TM_LOG_DEBUG, "Sensor %s", TelldusSensor_ToString(&sensorNode->sensor, str, sizeof(str)));
      return sensor;
    }
    lastSensorPtr = sensorNode;
    sensorNode = sensorNode->next;
  }
  
  // New sensor node, link into list
  SensorNode* newSensorListNode = calloc( 1, sizeof(SensorNode) );
  if( sensors == NULL )
  {
    sensors = newSensorListNode;
  }
  else
  {
    lastSensorPtr->next = newSensorListNode;
  }

  // Initialize sensor
  TelldusSensor* self = &newSensorListNode->sensor;
  strcpy(self->protocol, protocol);
  strcpy(self->model, model);
  strcpy(self->id, idStr);
  strcpy(self->dataType, dataTypeStr);
  strcpy(self->unit, TelldusSensor_DataTypeToUnit(dataType));
  //Log(TM_LOG_DEBUG, "New sensor %s", TelldusSensor_ToString(sensor, str, sizeof(str)));
  
  self->myTimer = MyTimer_Create( myTimerCallback, (void*) self );

  MqttClient_AddSensor(MqttClient_GetInstance(), self);
  
  return self;
}

char* TelldusSensor_DataTypeToString(int dataType)
{
  switch (dataType)
  {
  case TELLSTICK_TEMPERATURE:   return "Temperature";
  case TELLSTICK_HUMIDITY:      return "Humidity";
  case TELLSTICK_RAINRATE:      return "Rainrate";
  case TELLSTICK_RAINTOTAL:     return "Raintotal";
  case TELLSTICK_WINDDIRECTION: return "Winddirection";
  case TELLSTICK_WINDAVERAGE:   return "Windaverage";
  case TELLSTICK_WINDGUST:      return "Windgust";
  default:                      return "-----";
  }
}

char* TelldusSensor_DataTypeToUnit(int dataType)
{
  switch (dataType)
  {
  case TELLSTICK_TEMPERATURE:   return "°C";
  case TELLSTICK_HUMIDITY:      return "%";
  case TELLSTICK_RAINRATE:      return "";
  case TELLSTICK_RAINTOTAL:     return "";
  case TELLSTICK_WINDDIRECTION: return "";
  case TELLSTICK_WINDAVERAGE:   return "";
  case TELLSTICK_WINDGUST:      return "";
  default:                      return "?";
  }
}

char* TelldusSensor_ToString(TelldusSensor* self, char* strp, int len)
{
  snprintf(strp, len, "%s-%s-%s-%s",
    self->protocol,
    self->model,
    self->id,
    self->dataType
  );
  return strp;
}

char* TelldusSensor_ItemToString(TelldusSensor* self, TSensorContent content)
{
  char* ret = NULL;

  switch (content)
  {
  case TM_SENSOR_CONTENT_DATATYPE: return self->dataType;
  case TM_SENSOR_CONTENT_UNIT: return self->unit;
  case TM_SENSOR_CONTENT_PROTOCOL: return self->protocol;
  case TM_SENSOR_CONTENT_MODEL: return self->model;
  case TM_SENSOR_CONTENT_ID: return self->id;
  default: return "UKNOWN";
  }
}

void TelldusSensor_OnEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context)
{
  //TelldusClient* self = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "telldusSensorEvent %s, %s, %i, %i, %s, %i, %i", protocol, model, id, dataType, value, timestamp, callbackId);
  MqttClient* mqttclient = MqttClient_GetInstance();
  if( !MqttClient_IsConnected(mqttclient) )
  {
    Log(TM_LOG_WARNING, "telldusSensorEvent, not connected to mqtt => ignore");
    return;
  }

  // Find or create new sensor
  TelldusSensor* self = TelldusSensor_Create(protocol, model, id, dataType);
  
  // Add value
  strcpy(self->value, value);
  self->timestamp = timestamp;

  MqttClient_SensorValue(MqttClient_GetInstance(), self);
  if( !self->online )
  {
    char buf[100];
    Log(TM_LOG_INFO, "Sensor %s is now online", TelldusSensor_ToString(self, buf, sizeof(buf)));
    MqttClient_SensorOnline(MqttClient_GetInstance(), self, true);
    self->online = true;
  }
  MyTimer_Start( 
    self->myTimer, 
    Config_GetInt(Config_GetInstance(), "sensor-offline-seconds") * 1000 );
}

static void myTimerCallback(MyTimer* myTimer)
{
  TelldusSensor* self = (TelldusSensor*) MyTimer_GetCallbackData(myTimer);
  char buf[100];
  Log(TM_LOG_INFO, "Sensor %s is now offline", TelldusSensor_ToString(self, buf, sizeof(buf)));
  MqttClient_SensorOnline(MqttClient_GetInstance(), self, false);
  self->online = false;
}