#include <telldus-core.h>
#include "log.h"
#include "telldussensor.h"
#include "telldusclient.h"

// Temp include
#include <stdio.h>


#define TM_NO_CONTROLLER  (-1)

static const char* getDeviceTypeString(int type);

static void telldusDeviceEvent(int deviceId, int method, const char *data, int callbackId, void *context);
static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void *context);
static void telldusRawDeviceEvent(const char *data, int controllerId, int callbackId, void *context);
static void telldusSensorEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context);
static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char *newValue, int callbackId, void *context);

int TelldusClient_Init(TelldusClient *self)
{
  *self = (TelldusClient) {0};
  self->controllerId = TM_NO_CONTROLLER;

  tdInit();
  int ret = tdGetNumberOfDevices();
  if (ret == 0)
  {
    Log(TM_LOG_INFO, "Telldus has no devices");
  }
  else if( ret < 0)
  {
    Log(TM_LOG_ERROR, "Telldus error %s", tdGetErrorString(ret));
    return -1;
  }
  Log(TM_LOG_DEBUG, "Telldus has %i devices", ret);
  return 0;
}

bool TelldusClient_IsConnected(TelldusClient *self)
{
  // This is a deferred disconnect from an event
  if( self->disconnectRequest )
  {
    TelldusClient_Disconnect(self);
    self->disconnectRequest = false;
  }

  // This is a heartbeat message to see if service is still alive...
  if( self->controllerId != TM_NO_CONTROLLER )
  {
    char str[20];
    int ret = tdControllerValue(self->controllerId, "available", str, sizeof(str));
    if( ret != TELLSTICK_SUCCESS )
    {
      // Service is down (not supported method)
      Log(TM_LOG_ERROR, "Telldus error %s", tdGetErrorString(ret));
      TelldusClient_Disconnect(self);
    }
  }

  return self->controllerId != TM_NO_CONTROLLER;
}

int TelldusClient_Connect(TelldusClient *self)
{
  int ret = TELLSTICK_SUCCESS;
  int available = 0;
  int id;
  int type;
  char str[32];

  while( available == 0 && ret == TELLSTICK_SUCCESS )
  {
    ret = tdController(&id, &type, str, sizeof(str), &available);
  }

  if( ret != TELLSTICK_SUCCESS )
  {
    if( !self->mutelog )
    {
      Log(TM_LOG_ERROR, "Telldus connect error %s", tdGetErrorString(ret));
      self->mutelog = true;
    }
    self->controllerId != TM_NO_CONTROLLER;
    
    return -1;
  }

  self->controllerId = id;
  self->mutelog = false;
  tdControllerValue(id, "serial", self->controllerSerial, sizeof(self->controllerSerial));

  Log(TM_LOG_INFO, "Telldus controller id %i:%s:%s:%s selected", 
    id, str, self->controllerSerial, getDeviceTypeString(type));
  
  self->evtController   = tdRegisterControllerEvent(&telldusControllerEvent, (void*) self);
  self->evtDeviceChange = tdRegisterDeviceChangeEvent(&telldusDeviceChangeEvent, (void*) self);
  self->evtDevice       = tdRegisterDeviceEvent(&telldusDeviceEvent, (void*) self);
  self->evtSensor       = tdRegisterSensorEvent(&TelldusSensor_OnEvent, (void*) self);
  self->evtRawDevice    = tdRegisterRawDeviceEvent(&telldusRawDeviceEvent, (void*) self);

  return 0;
}

void TelldusClient_Disconnect(TelldusClient *self)
{
  tdUnregisterCallback(self->evtController);
  tdUnregisterCallback(self->evtDeviceChange);
  tdUnregisterCallback(self->evtDevice);
  tdUnregisterCallback(self->evtSensor);
  tdUnregisterCallback(self->evtRawDevice);
  self->evtController = 0;
  self->evtDeviceChange = 0;
  self->evtDevice = 0;
  self->evtSensor = 0;
  self->evtRawDevice = 0;
  self->controllerId = TM_NO_CONTROLLER;
}

static const char* getDeviceTypeString(int type)
{
  switch(type)
  {
    case TELLSTICK_CONTROLLER_TELLSTICK: return "tellstick";
    case TELLSTICK_CONTROLLER_TELLSTICK_DUO: return "tellstick duo";
    case TELLSTICK_CONTROLLER_TELLSTICK_NET: return "tellstick net";
    default: 
      Log(TM_LOG_ERROR, "unknown tellstick %i", type);
      return "tellstick uknown";
  }
}

static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char *newValue, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  // Disconnect
  // Id=5, evt=4, type=5, val=0, evtCbId=1
  
  // Connect:
  // Id=5, evt=4, type=5, val=1, evtCbId=1
  // Id=5, evt=2, type=6, val=12, evtCbId=1

  if( controllerId == self->controllerId )
  {
    if( changeEvent == 4 )
    {
      Log(TM_LOG_ERROR, "Selected tellstick controller disconnected");
      self->disconnectRequest = true;
    }
  }
  Log(TM_LOG_DEBUG, "telldusControllerEvent Id=%i, evt=%i, type=%i, val=%s, evtCbId=%i", 
    controllerId, changeEvent, changeType, newValue, callbackId);
}

static void telldusDeviceEvent(int deviceId, int method, const char *data, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "NYI:telldusDeviceEvent %i, %i, %s, %i", deviceId, method, data, callbackId);
}

static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "NYI:telldusDeviceChangeEvent %i, %i, %i, %i", deviceId, changeEvent, changeType, callbackId);
}

static void telldusRawDeviceEvent(const char *data, int controllerId, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  //Log(TM_LOG_DEBUG, "NYI:telldusRawDeviceEvent %s, %i, %i", data, controllerId, callbackId);
  // telldusRawDeviceEvent class :sensor; protocol:fineoffset; id : 167; model:temperaturehumidity; humidity : 44; temp:15.8; , 2, 5
}

static void telldusSensorEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  //Log(TM_LOG_DEBUG, "telldusSensorEvent %s, %s, %i, %i, %s, %i, %i", protocol, model, id, dataType, value, timestamp, callbackId);
}
#if 0
static void getTelldusDevices(void)
{
  int nrDev = tdGetNumberOfDevices();
  for (int i = 0; i < nrDev; i++)
  {
    printf("Device %i\r\n", i);
    printf("\tid = %i\r\n", tdGetDeviceId(i));
    printf("\ttype = %i\r\n", tdGetDeviceType(i));
    printf("\tname = %s\r\n", tdGetName(i));
    printf("\tprotocol = %s\r\n", tdGetProtocol(i));
    printf("\tmodel = %s\r\n", tdGetModel(i));

    const char *names[] = {"code", "fade", "house", "model", "name", "protocol", "state", "stateValue", "system", "unit", "units", ""};
    int j = 0;
    while (*names[j])
    {
      printf("\t%s=%s\r\n", names[j], tdGetDeviceParameter(i, names[j], "---"));
      j++;
    }
    // char* WINAPI tdGetDeviceParameter(int intDeviceId, const char* strName, const char* defaultValue);
  }
}

static void telldusDeviceEvent(int deviceId, int method, const char *data, int callbackId, void *context)
{
  printf("NYI:telldusDeviceEvent %i, %i, %s, %i\r\n", deviceId, method, data, callbackId);
}

static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void *context)
{
  printf("NYI:telldusDeviceChangeEvent %i, %i, %i, %i\r\n", deviceId, changeEvent, changeType, callbackId);
}

static void telldusRawDeviceEvent(const char *data, int controllerId, int callbackId, void *context)
{
  printf("NYI:telldusRawDeviceEvent %s, %i, %i\r\n", data, controllerId, callbackId);
  // telldusRawDeviceEvent class :sensor; protocol:fineoffset; id : 167; model:temperaturehumidity; humidity : 44; temp:15.8; , 2, 5
}

static void telldusSensorEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context)
{
  // printf("telldusSensorEvent %s, %s, %i, %i, %s, %i, %i\r\n", protocol, model, id, dataType, value, timestamp, callbackId);
  if (!connected)
  {
    printf("telldusSensorEvent but mqtt is not connected!\r\n");
    return;
  }
  char payload[20];
  int rc;

  /* Print it to a string for easy human reading - payload format is highly
   * application dependent. */
  snprintf(payload, sizeof(payload), "%s", value);

  char dataDypeStr[20] = "NYI";
  switch (dataType)
  {
  case TELLSTICK_TEMPERATURE:
    sprintf(dataDypeStr, "temp");
    break;
  case TELLSTICK_HUMIDITY:
    sprintf(dataDypeStr, "hum");
    break;
  }

  /* Publish the message
   * mosq - our client instance
   * *mid = NULL - we don't want to know what the message id for this message is
   * topic = "example/temperature" - the topic on which this message will be published
   * payloadlen = strlen(payload) - the length of our payload in bytes
   * payload - the actual payload
   * qos = 2 - publish with QoS 2 for this example
   * retain = false - do not use the retained message feature for this message
   */
  char telldusTopic[70];
  snprintf(telldusTopic, sizeof(telldusTopic), "telldus/%s/%s/%i/%s", protocol, model, id, dataDypeStr);
  char topic[70];
  if (Config_GetTopicTranslation(&config, "telldus", telldusTopic, "mqtt", topic, sizeof(topic)))
  {
    // No match
    strncpy(topic, telldusTopic, sizeof(topic));
  }
  CriticalSection_Enter(criticalsectionPtr);
  rc = mosquitto_publish(mosq, NULL, topic, (int)strlen(payload), payload, 0, false);
  if (rc != MOSQ_ERR_SUCCESS)
  {

    fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
  }
  else
  {
    // printf("Published %s = '%s'\r\n", topic, payload);
  }
  CriticalSection_Leave(criticalsectionPtr);
}

static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char *newValue, int callbackId, void *context)
{
  printf("telldusControllerEvent %i, %i, %i, %s, %i\r\n", controllerId, changeEvent, changeType, newValue, callbackId);
}
#endif