#include <stdlib.h>
#include <telldus-core.h>
#include "log.h"
#include "telldusdevice.h"
#include "telldussensor.h"
#include "telldusclient.h"

// Temp include
#include <stdio.h>

#define TM_NO_CONTROLLER  (-1)

static TelldusClient theTelldusClient;
static bool created;

static const char* getDeviceTypeString(int type);

static void telldusDeviceEvent(int deviceId, int method, const char *data, int callbackId, void *context);
static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void *context);
static void telldusRawDeviceEvent(const char *data, int controllerId, int callbackId, void *context);
static void telldusSensorEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context);
static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char *newValue, int callbackId, void *context);

TelldusClient* TelldusClient_GetInstance(void)
{
  TelldusClient* self = &theTelldusClient;
  if( created == false )
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
      exit(1);
    }
    Log(TM_LOG_DEBUG, "Telldus has %i devices", ret);
    self->deviceCount = ret;
    created = true;
  }
  return self;
}

void TelldusClient_Destroy(TelldusClient *self)
{
  TelldusClient_Disconnect( self );
  tdClose();
}

void TelldusClient_SetLogRaw(TelldusClient *self, bool lograw)
{
  self->lograw = lograw;
}

bool TelldusClient_IsConnected(TelldusClient *self)
{
  // This is a deferred disconnect from an event
  if( self->disconnectRequest )
  {
    TelldusClient_Disconnect(self);
    self->disconnectRequest = false;
  }
  else
  {
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
    self->controllerId = TM_NO_CONTROLLER;
    
    return -1;
  }

  self->controllerId = id;
  self->mutelog = false;
  tdControllerValue(id, "serial", self->controllerSerial, sizeof(self->controllerSerial));

  Log(TM_LOG_INFO, "Telldus controller id %i:%s:%s:%s selected", 
    id, str, self->controllerSerial, getDeviceTypeString(type));
  
  self->evtController   = tdRegisterControllerEvent(&telldusControllerEvent, (void*) self);
  self->evtDeviceChange = tdRegisterDeviceChangeEvent(&telldusDeviceChangeEvent, (void*) self);
  self->evtDevice       = tdRegisterDeviceEvent(&TelldusDevice_OnEvent, (void*) self);
  self->evtSensor       = tdRegisterSensorEvent(&TelldusSensor_OnEvent, (void*) self);
  self->evtRawDevice    = tdRegisterRawDeviceEvent(&telldusRawDeviceEvent, (void*) self);

  return 0;
}

void TelldusClient_Disconnect(TelldusClient *self)
{
  Log(TM_LOG_DEBUG, "Telldus disconnect");
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

int TelldusClient_GetDeviceNo(TelldusClient *self, TDeviceGetOp op)
{
  int ret;
  int deviceIndex;

  if( op == TM_DEVICE_GET_FIRST )
  {
    deviceIndex = 0;
    // Log(TM_LOG_DEBUG, "Telldus first device");
  }
  else
  {
    deviceIndex = self->lastDeviceIx + 1;
  }

  if( deviceIndex >= self->deviceCount )
  {
    return -1; // No more device numbers
  }
  
  ret = tdGetDeviceId(deviceIndex);
  if (ret == 0)
  {
    Log(TM_LOG_ERROR, "Telldus has no device index %i", deviceIndex);
    return -1;
  }
  else if( ret < 0)
  {
    Log(TM_LOG_ERROR, "Telldus device index %i error %s", deviceIndex, tdGetErrorString(ret));
    return -1;
  }
  
  self->lastDeviceIx = deviceIndex;
  
  return ret; // Device Id
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
  if( self->lograw )
  {
    // telldusRawDeviceEvent class :sensor; protocol:fineoffset; id : 167; model:temperaturehumidity; humidity : 44; temp:15.8; , 2, 5
    Log(TM_LOG_INFO, "telldusRawDeviceEvent %s, %i, %i", data, controllerId, callbackId);
  }
}

static void telldusSensorEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context)
{
  TelldusClient* self = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "telldusSensorEvent %s, %s, %i, %i, %s, %i, %i", protocol, model, id, dataType, value, timestamp, callbackId);
}
