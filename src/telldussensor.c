#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <telldus-core.h>
#include "log.h"
#include "telldussensor.h"


typedef struct SensorNodeS SensorNode;

struct SensorNodeS {
  TelldusSensor sensor;
  SensorNode* next;
};

SensorNode* sensors;

char* TelldusSensor_ToString(TelldusSensor* self, char* strp, int len);

TelldusSensor* TelldusSensor_Create(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp)
{
  SensorNode* sensorNode = sensors;
  SensorNode* lastSensorPtr;
  char str[80];
  
  // Find existing sensor
  while ( sensorNode != NULL )
  {
    TelldusSensor* sensor = &sensorNode->sensor;
    if( strcmp(sensor->protocol, protocol) == 0 && 
        strcmp(sensor->model, model) == 0 &&
        sensor->id == id &&
        sensor->dataType == dataType )
    {
      Log(TM_LOG_DEBUG, "Sensor %s", TelldusSensor_ToString(&sensorNode->sensor, str, sizeof(str)));
      return sensor;
    }
    lastSensorPtr = sensorNode;
    sensorNode = sensorNode->next;
  }
  
  // New sensor
  SensorNode* newSensorListNode = calloc( 1, sizeof(SensorNode) );
  if( sensors == NULL )
  {
    sensors = newSensorListNode;
  }
  else
  {
    lastSensorPtr->next = newSensorListNode;
  }

  strcpy(newSensorListNode->sensor.protocol, protocol);
  strcpy(newSensorListNode->sensor.model, model);
  newSensorListNode->sensor.id = id;
  newSensorListNode->sensor.dataType = dataType;
  Log(TM_LOG_DEBUG, "New sensor %s", TelldusSensor_ToString(&newSensorListNode->sensor, str, sizeof(str)));

  return &newSensorListNode->sensor;
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
char* TelldusSensor_ToString(TelldusSensor* self, char* strp, int len)
{
  snprintf(strp, len, "%s-%s-%i-%s",
    self->protocol,
    self->model,
    self->id,
    TelldusSensor_DataTypeToString(self->dataType)
  );
  return strp;
}

void TelldusSensor_OnEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context)
{
  //TelldusClient* self = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "telldusSensorEvent %s, %s, %i, %i, %s, %i, %i", protocol, model, id, dataType, value, timestamp, callbackId);
  TelldusSensor* sensor = TelldusSensor_Create(protocol, model, id, dataType, value, timestamp);
}
