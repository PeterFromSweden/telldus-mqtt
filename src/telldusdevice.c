#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <telldus-core.h>
#include "log.h"
#include "mqttclient.h"
#include "telldusdevice.h"

typedef struct DeviceNodeS DeviceNode;

struct DeviceNodeS {
  TelldusDevice device;
  DeviceNode* next;
};

DeviceNode* devices;

static char* TelldusDevice_DataTypeToString(int dataType);
static char* TelldusDevice_DataTypeToUnit(int dataType);

TelldusDevice* TelldusDevice_Get(int device_number, DeviceNode** lastDeviceNode)
{
  DeviceNode* deviceNode = devices;
  DeviceNode* tmpDeviceNode = NULL;
  
  // Find existing device
  while ( deviceNode != NULL )
  {
    TelldusDevice* device = &deviceNode->device;
    if( device->device_number == device_number )
    {
      char str[80];
      Log(TM_LOG_DEBUG, "Device %s", TelldusDevice_ToString(&deviceNode->device, str, sizeof(str)));
      return device;
    }
    tmpDeviceNode = deviceNode;
    deviceNode = deviceNode->next;
  }

  if( lastDeviceNode != NULL )
  {
    *lastDeviceNode = tmpDeviceNode;
  }
  return &deviceNode->device;
}

TelldusDevice* TelldusDevice_GetTopic(const char* topic)
{
  DeviceNode* deviceNode = devices;

  // Find existing device
  while ( deviceNode != NULL )
  {
    TelldusDevice* device = &deviceNode->device;
    if( strcmp(device->command_topic, topic) == 0 )
    {
      char str[80];
      Log(TM_LOG_DEBUG, "Device from topic %s", TelldusDevice_ToString(&deviceNode->device, str, sizeof(str)));
      return device;
    }
    deviceNode = deviceNode->next;
  }
  
  Log(TM_LOG_ERROR, "Device from topic NOT FOUND %s", topic);

  return NULL;
}

TelldusDevice* TelldusDevice_Create(int device_number)
{
  DeviceNode* lastDevicePtr;
  TelldusDevice* device = TelldusDevice_Get(device_number, &lastDevicePtr);
  if( device != NULL )
  {
    return device; // Already existing device in device list
  }

  // New device node, link into list
  DeviceNode* newDeviceListNode = calloc( 1, sizeof(DeviceNode) );
  if( devices == NULL )
  {
    devices = newDeviceListNode;
  }
  else
  {
    lastDevicePtr->next = newDeviceListNode;
  }

  // Initialize device
  device = &newDeviceListNode->device;
  char device_no[5];
  sprintf(device_no, "%i", device_number);
  
  strcpy(device->device_no, device_no);
  device->device_number = device_number;
  char str[80];
  Log(TM_LOG_DEBUG, "New device %s", TelldusDevice_ToString(device, str, sizeof(str)));
  
  //MqttClient_AddDevice(MqttClient_GetInstance(), device);
  return device;
}


char* TelldusDevice_ToString(TelldusDevice* self, char* strp, int len)
{
  snprintf(strp, len, "%s", self->device_no);
  return strp;
}

char* TelldusDevice_ItemToString(TelldusDevice* self, TDeviceContent content)
{
  char* ret = NULL;

  switch (content)
  {
  case TM_DEVICE_CONTENT_DEVICENO: return self->device_no;
  default: return "UKNOWN";
  }
}

static bool compareStringsIgnoreCase(const char *str1, const char *str2) 
{
  while (*str1 != '\0' && *str2 != '\0') 
  {
    if (tolower(*str1) != tolower(*str2)) 
    {
      return false; // Strings are not equal
    }
    str1++;
    str2++;
  }
  return (*str1 == '\0' && *str2 == '\0');
}

void TelldusDevice_Action(TelldusDevice* self, const char* action)
{
  if( compareStringsIgnoreCase(action, "on" ) )
  {
    tdTurnOn(self->device_number);
    Log(TM_LOG_DEBUG, "Turn on on device %i", self->device_number);
  }
  else if( compareStringsIgnoreCase(action, "off" ) )
  {
    tdTurnOff(self->device_number);
    Log(TM_LOG_DEBUG, "Turn off on device %i", self->device_number);
  }
  else
  {
    Log(TM_LOG_WARNING, "Unknown action %s on device %i", action, self->device_number);
  }
}

static char* TelldusDevice_MethodToString(int method)
{
  switch (method)
  {
  case TELLSTICK_TURNON:  return "ON";
  case TELLSTICK_TURNOFF: return "OFF";
  case TELLSTICK_BELL:    return "BELL";
  case TELLSTICK_TOGGLE:  return "TOGGLE";
  case TELLSTICK_DIM:     return "DIM";
  case TELLSTICK_EXECUTE: return "EXECUTE";
  case TELLSTICK_UP:      return "UP";
  case TELLSTICK_DOWN:    return "DOWN";
  case TELLSTICK_STOP:    return "STOP";
  default:                return "?";
  }
}

void TelldusDevice_OnEvent(int deviceId, int method, const char *data, int callbackId, void *context)
{
  //TelldusClient* telldusclient = (TelldusClient*) context;
  Log(TM_LOG_DEBUG, "telldusDeviceEvent %i, %i, %s, %i", deviceId, method, data, callbackId);
  if( !MqttClient_IsConnected(MqttClient_GetInstance()) )
  {
    Log(TM_LOG_WARNING, "telldusDeviceEvent, not connected to mqtt => ignore");
    return;
  }

  TelldusDevice* self = TelldusDevice_Get(deviceId, NULL);
  if( self == NULL )
  {
    Log(TM_LOG_ERROR, "telldusDeviceEvent, device new? => ignore");
    return;
  }
  
  strcpy(self->value, TelldusDevice_MethodToString(method));
  MqttClient_DeviceValue(MqttClient_GetInstance(), self);
}