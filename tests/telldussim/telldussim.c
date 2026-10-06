#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <telldus-core.h>
#include "telldussim.h"

#define MAX_CALLBACKS 32

typedef enum {
  CB_NONE,
  CB_DEVICE,
  CB_DEVICE_CHANGE,
  CB_RAW_DEVICE,
  CB_SENSOR,
  CB_CONTROLLER,
} CallbackType;

typedef struct {
  CallbackType type;
  void* function;
  void* context;
} Callback;

typedef struct {
  int id;
  int turnOnCount;
  int turnOffCount;
} Device;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Callback callbacks[MAX_CALLBACKS]; // index + 1 = callback id
static Device devices[TELLDUSSIM_MAX_DEVICES];
static int deviceCount;

static bool hasController;
static int controllerId;
static int controllerType;
static char controllerSerial[20];
static bool controllerAvailable;
static int controllerIterator;

void TelldusSim_Reset(void)
{
  pthread_mutex_lock(&lock);
  memset(callbacks, 0, sizeof(callbacks));
  memset(devices, 0, sizeof(devices));
  deviceCount = 0;
  hasController = false;
  controllerIterator = 0;
  pthread_mutex_unlock(&lock);
}

void TelldusSim_SetController(int id, int type, const char* serial, bool available)
{
  pthread_mutex_lock(&lock);
  hasController = true;
  controllerId = id;
  controllerType = type;
  snprintf(controllerSerial, sizeof(controllerSerial), "%s", serial);
  controllerAvailable = available;
  pthread_mutex_unlock(&lock);
}

static Device* findDevice(int deviceId)
{
  for( int i = 0; i < deviceCount; i++ )
  {
    if( devices[i].id == deviceId )
    {
      return &devices[i];
    }
  }
  return NULL;
}

void TelldusSim_AddDevice(int deviceId)
{
  pthread_mutex_lock(&lock);
  if( deviceCount < TELLDUSSIM_MAX_DEVICES && findDevice(deviceId) == NULL )
  {
    devices[deviceCount].id = deviceId;
    deviceCount++;
  }
  pthread_mutex_unlock(&lock);
}

// Copy matching callbacks so they can be invoked without holding the lock
// (callbacks may call back into the simulator).
static int collect(CallbackType type, Callback* out, int* ids)
{
  int n = 0;
  pthread_mutex_lock(&lock);
  for( int i = 0; i < MAX_CALLBACKS; i++ )
  {
    if( callbacks[i].type == type )
    {
      out[n] = callbacks[i];
      ids[n] = i + 1;
      n++;
    }
  }
  pthread_mutex_unlock(&lock);
  return n;
}

void TelldusSim_UnplugController(void)
{
  pthread_mutex_lock(&lock);
  int id = controllerId;
  hasController = false;
  pthread_mutex_unlock(&lock);

  Callback cbs[MAX_CALLBACKS];
  int ids[MAX_CALLBACKS];
  int n = collect(CB_CONTROLLER, cbs, ids);
  for( int i = 0; i < n; i++ )
  {
    ((TDControllerEvent) cbs[i].function)(id, 4, 5, "0", ids[i], cbs[i].context);
  }
}

void TelldusSim_SensorEvent(const char* protocol, const char* model, int id, int dataType, const char* value)
{
  Callback cbs[MAX_CALLBACKS];
  int ids[MAX_CALLBACKS];
  int n = collect(CB_SENSOR, cbs, ids);
  for( int i = 0; i < n; i++ )
  {
    ((TDSensorEvent) cbs[i].function)(protocol, model, id, dataType, value, 0, ids[i], cbs[i].context);
  }
}

void TelldusSim_DeviceEvent(int deviceId, int method)
{
  Callback cbs[MAX_CALLBACKS];
  int ids[MAX_CALLBACKS];
  int n = collect(CB_DEVICE, cbs, ids);
  for( int i = 0; i < n; i++ )
  {
    ((TDDeviceEvent) cbs[i].function)(deviceId, method, "", ids[i], cbs[i].context);
  }
}

int TelldusSim_GetTurnOnCount(int deviceId)
{
  pthread_mutex_lock(&lock);
  Device* d = findDevice(deviceId);
  int count = d ? d->turnOnCount : 0;
  pthread_mutex_unlock(&lock);
  return count;
}

int TelldusSim_GetTurnOffCount(int deviceId)
{
  pthread_mutex_lock(&lock);
  Device* d = findDevice(deviceId);
  int count = d ? d->turnOffCount : 0;
  pthread_mutex_unlock(&lock);
  return count;
}

// ---- telldus-core API ----

void WINAPI tdInit(void)
{
}

void WINAPI tdClose(void)
{
}

static int registerCallback(CallbackType type, void* function, void* context)
{
  int id = 0;
  pthread_mutex_lock(&lock);
  for( int i = 0; i < MAX_CALLBACKS; i++ )
  {
    if( callbacks[i].type == CB_NONE )
    {
      callbacks[i] = (Callback) { type, function, context };
      id = i + 1;
      break;
    }
  }
  pthread_mutex_unlock(&lock);
  return id;
}

int WINAPI tdRegisterDeviceEvent(TDDeviceEvent eventFunction, void *context)
{
  return registerCallback(CB_DEVICE, (void*) eventFunction, context);
}

int WINAPI tdRegisterDeviceChangeEvent(TDDeviceChangeEvent eventFunction, void *context)
{
  return registerCallback(CB_DEVICE_CHANGE, (void*) eventFunction, context);
}

int WINAPI tdRegisterRawDeviceEvent(TDRawDeviceEvent eventFunction, void *context)
{
  return registerCallback(CB_RAW_DEVICE, (void*) eventFunction, context);
}

int WINAPI tdRegisterSensorEvent(TDSensorEvent eventFunction, void *context)
{
  return registerCallback(CB_SENSOR, (void*) eventFunction, context);
}

int WINAPI tdRegisterControllerEvent(TDControllerEvent eventFunction, void *context)
{
  return registerCallback(CB_CONTROLLER, (void*) eventFunction, context);
}

int WINAPI tdUnregisterCallback(int callbackId)
{
  pthread_mutex_lock(&lock);
  if( callbackId > 0 && callbackId <= MAX_CALLBACKS )
  {
    callbacks[callbackId - 1].type = CB_NONE;
  }
  pthread_mutex_unlock(&lock);
  return TELLSTICK_SUCCESS;
}

int WINAPI tdGetNumberOfDevices(void)
{
  pthread_mutex_lock(&lock);
  int n = deviceCount;
  pthread_mutex_unlock(&lock);
  return n;
}

int WINAPI tdGetDeviceId(int intDeviceIndex)
{
  pthread_mutex_lock(&lock);
  int id = (intDeviceIndex >= 0 && intDeviceIndex < deviceCount) ? devices[intDeviceIndex].id : -1;
  pthread_mutex_unlock(&lock);
  return id;
}

char* WINAPI tdGetErrorString(int intErrorNo)
{
  const char* str;
  switch( intErrorNo )
  {
    case TELLSTICK_SUCCESS:         str = "Success"; break;
    case TELLSTICK_ERROR_NOT_FOUND: str = "TellStick not found"; break;
    default:                        str = "Unknown error"; break;
  }
  char* copy = malloc(strlen(str) + 1);
  if( copy != NULL )
  {
    strcpy(copy, str);
  }
  return copy;
}

void WINAPI tdReleaseString(char *thestring)
{
  free(thestring);
}

int WINAPI tdController(int *id, int *type, char *name, int nameLen, int *available)
{
  int ret = TELLSTICK_ERROR_NOT_FOUND;
  pthread_mutex_lock(&lock);
  if( hasController && controllerIterator == 0 )
  {
    *id = controllerId;
    *type = controllerType;
    snprintf(name, nameLen, "Simulated TellStick");
    *available = controllerAvailable ? 1 : 0;
    controllerIterator++;
    ret = TELLSTICK_SUCCESS;
  }
  else
  {
    controllerIterator = 0; // Restart the listing on the next call
  }
  pthread_mutex_unlock(&lock);
  return ret;
}

int WINAPI tdControllerValue(int id, const char *name, char *value, int valueLen)
{
  int ret = TELLSTICK_ERROR_NOT_FOUND;
  pthread_mutex_lock(&lock);
  if( hasController && id == controllerId )
  {
    if( strcmp(name, "serial") == 0 )
    {
      snprintf(value, valueLen, "%s", controllerSerial);
      ret = TELLSTICK_SUCCESS;
    }
    else if( strcmp(name, "available") == 0 )
    {
      snprintf(value, valueLen, "%d", controllerAvailable ? 1 : 0);
      ret = TELLSTICK_SUCCESS;
    }
  }
  pthread_mutex_unlock(&lock);
  return ret;
}

// Like telldusd, a sent command is echoed back as a device event.
static int sendCommand(int deviceId, int method)
{
  pthread_mutex_lock(&lock);
  Device* d = findDevice(deviceId);
  if( d != NULL )
  {
    if( method == TELLSTICK_TURNON ) d->turnOnCount++;
    if( method == TELLSTICK_TURNOFF ) d->turnOffCount++;
  }
  pthread_mutex_unlock(&lock);

  if( d == NULL )
  {
    return TELLSTICK_ERROR_DEVICE_NOT_FOUND;
  }
  TelldusSim_DeviceEvent(deviceId, method);
  return TELLSTICK_SUCCESS;
}

int WINAPI tdTurnOn(int intDeviceId)
{
  return sendCommand(intDeviceId, TELLSTICK_TURNON);
}

int WINAPI tdTurnOff(int intDeviceId)
{
  return sendCommand(intDeviceId, TELLSTICK_TURNOFF);
}
