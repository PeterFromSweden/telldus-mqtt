#ifndef WIN32
#define _XOPEN_SOURCE 700 // fchmod, fdopen
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <cjson/cJSON.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "log.h"
#include "config.h"
#include "criticalsection.h"
#include "version.h"
#include "status.h"

#define STATUS_PATH_LEN 256

typedef enum {
  STATUS_DEVICE_FREE,
  STATUS_DEVICE_SENSOR,
  STATUS_DEVICE_SWITCH,
} StatusDeviceType;

typedef struct {
  char key[14];   // datatype in lower case, e.g. "temperature"
  char value[10]; // as TelldusSensor value
} StatusValue;

typedef struct {
  StatusDeviceType type;
  char id[72];    // sensor: {protocol}_{model}_{id}, switch: device number
  time_t lastSeen;
  bool online;
  StatusValue values[STATUS_MAX_VALUES];
} StatusDevice;

typedef struct {
  CriticalSection* cs;
  char path[STATUS_PATH_LEN];
  char tmpPath[STATUS_PATH_LEN + 4];
  int offlineSeconds;
  bool dirty;
  bool mutelog;
  time_t started;
  time_t lastWrite;
  time_t lastRf;
  bool serviceConnected;
  bool gatewayFound;
  char gatewayType[20];
  char gatewaySerial[20];
  bool brokerConnected;
  StatusDevice* devices; // STATUS_MAX_DEVICES entries
  // Step 3: raw RF events (newest first) go here and in addEvents().
} Status;

static Status theStatus;
static bool enabled;

static void copyString(char* dest, size_t destSize, const char* src)
{
  snprintf(dest, destSize, "%s", src != NULL ? src : "");
}

int Status_Init(const char* path)
{
  Status* self = &theStatus;
  Status_Destroy();
  if( path == NULL || strlen(path) >= sizeof(self->path) )
  {
    Log(TM_LOG_ERROR, "Status file path is not valid");
    return 1;
  }

  *self = (Status) {0};
  self->devices = (StatusDevice*) calloc(STATUS_MAX_DEVICES, sizeof(StatusDevice));
  self->cs = CriticalSection_Create();
  if( self->devices == NULL || self->cs == NULL )
  {
    Log(TM_LOG_ERROR, "Status: out of memory");
    Status_Destroy();
    return 1;
  }
  copyString(self->path, sizeof(self->path), path);
  snprintf(self->tmpPath, sizeof(self->tmpPath), "%s.tmp", path);
  self->offlineSeconds = Config_GetInt(Config_GetInstance(), "sensor-offline-seconds");
  self->started = time(NULL);
  self->dirty = true;
  enabled = true;
  Log(TM_LOG_INFO, "Status file %s", self->path);
  return 0;
}

void Status_Destroy(void)
{
  Status* self = &theStatus;
  enabled = false;
  CriticalSection_Destroy(self->cs);
  free(self->devices);
  *self = (Status) {0};
}

bool Status_IsEnabled(void)
{
  return enabled;
}

void Status_SetService(bool connected)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  CriticalSection_Enter(self->cs);
  self->serviceConnected = connected;
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

// When the gateway is lost, type and serial keep the last known values.
void Status_SetGateway(bool found, const char* type, const char* serial)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  CriticalSection_Enter(self->cs);
  self->gatewayFound = found;
  if( found )
  {
    copyString(self->gatewayType, sizeof(self->gatewayType), type);
    copyString(self->gatewaySerial, sizeof(self->gatewaySerial), serial);
  }
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

void Status_SetBroker(bool connected)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  CriticalSection_Enter(self->cs);
  self->brokerConnected = connected;
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

// Find a device, or add it. When the list is full, the sensor heard
// longest ago is replaced. Switches are not replaced. Call with lock held.
static StatusDevice* getDevice(Status* self, StatusDeviceType type, const char* id)
{
  StatusDevice* freeDevice = NULL;
  StatusDevice* oldestSensor = NULL;

  for( int i = 0; i < STATUS_MAX_DEVICES; i++ )
  {
    StatusDevice* device = &self->devices[i];
    if( device->type == type && strcmp(device->id, id) == 0 )
    {
      return device;
    }
    if( device->type == STATUS_DEVICE_FREE && freeDevice == NULL )
    {
      freeDevice = device;
    }
    if( device->type == STATUS_DEVICE_SENSOR &&
        (oldestSensor == NULL || device->lastSeen < oldestSensor->lastSeen) )
    {
      oldestSensor = device;
    }
  }

  StatusDevice* device = freeDevice != NULL ? freeDevice : oldestSensor;
  if( device == NULL )
  {
    Log(TM_LOG_DEBUG, "Status: device list full, %s not listed", id);
    return NULL;
  }
  if( device == oldestSensor )
  {
    Log(TM_LOG_DEBUG, "Status: device list full, %s replaces %s", id, device->id);
  }
  *device = (StatusDevice) {0};
  device->type = type;
  copyString(device->id, sizeof(device->id), id);
  return device;
}

static void setValue(StatusDevice* device, const char* key, const char* value)
{
  char lowerKey[sizeof(device->values[0].key)];
  copyString(lowerKey, sizeof(lowerKey), key);
  for( char* p = lowerKey; *p != '\0'; p++ )
  {
    *p = (char) tolower((unsigned char) *p);
  }

  for( int i = 0; i < STATUS_MAX_VALUES; i++ )
  {
    StatusValue* v = &device->values[i];
    if( v->key[0] == '\0' || strcmp(v->key, lowerKey) == 0 )
    {
      copyString(v->key, sizeof(v->key), lowerKey);
      copyString(v->value, sizeof(v->value), value);
      return;
    }
  }
  Log(TM_LOG_DEBUG, "Status: too many values for %s, %s not listed", device->id, lowerKey);
}

void Status_SensorValue(const char* protocol, const char* model, int id, const char* dataType, const char* value)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  char sensorId[sizeof(self->devices[0].id)];
  snprintf(sensorId, sizeof(sensorId), "%s_%s_%i", protocol, model, id);
  time_t now = time(NULL);

  CriticalSection_Enter(self->cs);
  self->lastRf = now;
  StatusDevice* device = getDevice(self, STATUS_DEVICE_SENSOR, sensorId);
  if( device != NULL )
  {
    device->lastSeen = now;
    device->online = true;
    setValue(device, dataType, value);
  }
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

void Status_AddSwitch(int deviceNumber)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  char switchId[12];
  snprintf(switchId, sizeof(switchId), "%i", deviceNumber);

  CriticalSection_Enter(self->cs);
  getDevice(self, STATUS_DEVICE_SWITCH, switchId);
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

void Status_SwitchValue(int deviceNumber, const char* state)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  char switchId[12];
  snprintf(switchId, sizeof(switchId), "%i", deviceNumber);

  CriticalSection_Enter(self->cs);
  StatusDevice* device = getDevice(self, STATUS_DEVICE_SWITCH, switchId);
  if( device != NULL )
  {
    setValue(device, "state", state);
  }
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

void Status_RawEvent(const char* data)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  CriticalSection_Enter(self->cs);
  self->lastRf = time(NULL);
  // Step 3: keep data in the raw RF event list.
  self->dirty = true;
  CriticalSection_Leave(self->cs);
}

// A sensor is offline when silent for sensor-offline-seconds, like the
// MQTT availability topic. 0 means never offline (as MyTimer_Start with 0).
// Returns true if a sensor changed. Call with lock held.
static bool updateOnline(Status* self, time_t now)
{
  bool changed = false;
  for( int i = 0; i < STATUS_MAX_DEVICES; i++ )
  {
    StatusDevice* device = &self->devices[i];
    if( device->type == STATUS_DEVICE_SENSOR )
    {
      bool online = self->offlineSeconds <= 0 || now - device->lastSeen < self->offlineSeconds;
      if( online != device->online )
      {
        device->online = online;
        changed = true;
      }
    }
  }
  return changed;
}

static void addDevice(Status* self, cJSON* array, StatusDevice* device)
{
  cJSON* item = cJSON_CreateObject();
  char telldusTopic[160];
  if( device->type == STATUS_DEVICE_SENSOR )
  {
    cJSON_AddStringToObject(item, "type", "sensor");
    snprintf(telldusTopic, sizeof(telldusTopic), "telldus/%s/sensor/%s", self->gatewaySerial, device->id);
  }
  else
  {
    cJSON_AddStringToObject(item, "type", "switch");
    snprintf(telldusTopic, sizeof(telldusTopic), "telldus/%s/switch/%s", self->gatewaySerial, device->id);
  }
  cJSON_AddStringToObject(item, "id", device->id);

  // Only devices with a topic-translation have a name and topic
  const char* name;
  const char* topic = Config_FindTopicTranslation(Config_GetInstance(), telldusTopic, &name);
  if( topic != NULL )
  {
    if( name != NULL )
    {
      cJSON_AddStringToObject(item, "name", name);
    }
    cJSON_AddStringToObject(item, "topic", topic);
  }

  if( device->type == STATUS_DEVICE_SENSOR )
  {
    cJSON_AddNumberToObject(item, "last_seen", (double) device->lastSeen);
    cJSON_AddBoolToObject(item, "online", device->online);
  }

  cJSON* values = cJSON_AddObjectToObject(item, "values");
  for( int i = 0; i < STATUS_MAX_VALUES && device->values[i].key[0] != '\0'; i++ )
  {
    cJSON_AddStringToObject(values, device->values[i].key, device->values[i].value);
  }
  cJSON_AddItemToArray(array, item);
}

// Step 3: add raw RF events here, max 20, newest first.
static void addEvents(Status* self, cJSON* root)
{
  cJSON_AddArrayToObject(root, "events");
}

static char* createJson(Status* self, time_t now)
{
  Config* config = Config_GetInstance();
  cJSON* root = cJSON_CreateObject();

  cJSON_AddNumberToObject(root, "format", STATUS_FORMAT);
  cJSON_AddStringToObject(root, "version", TELLDUS_MQTT_VERSION);
  cJSON_AddNumberToObject(root, "started", (double) self->started);
  cJSON_AddNumberToObject(root, "updated", (double) now);

  cJSON* gateway = cJSON_AddObjectToObject(root, "gateway");
  cJSON_AddBoolToObject(gateway, "found", self->gatewayFound);
  cJSON_AddStringToObject(gateway, "type", self->gatewayType);
  cJSON_AddStringToObject(gateway, "serial", self->gatewaySerial);

  cJSON* service = cJSON_AddObjectToObject(root, "service");
  cJSON_AddBoolToObject(service, "connected", self->serviceConnected);

  // Never write user or pass
  cJSON* broker = cJSON_AddObjectToObject(root, "broker");
  cJSON_AddBoolToObject(broker, "connected", self->brokerConnected);
  const char* host = Config_GetStrPtr(config, "host");
  cJSON_AddStringToObject(broker, "host", host != NULL ? host : "");
  cJSON_AddNumberToObject(broker, "port", Config_GetInt(config, "port"));

  cJSON_AddNumberToObject(root, "last_rf", (double) self->lastRf); // 0 = none yet

  cJSON* devices = cJSON_AddArrayToObject(root, "devices");
  for( int i = 0; i < STATUS_MAX_DEVICES; i++ )
  {
    if( self->devices[i].type != STATUS_DEVICE_FREE )
    {
      addDevice(self, devices, &self->devices[i]);
    }
  }

  addEvents(self, root);

  char* json = cJSON_Print(root);
  cJSON_Delete(root);
  return json;
}

// Write tmpPath, then rename it over path, so a reader never sees a partial file.
static int writeFile(Status* self, const char* json)
{
  size_t len = strlen(json);
  bool ok;

#ifdef WIN32
  FILE* f = fopen(self->tmpPath, "wb");
#else
  FILE* f = NULL;
  int fd = open(self->tmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if( fd >= 0 )
  {
    fchmod(fd, 0644); // Not changed by umask
    f = fdopen(fd, "wb");
    if( f == NULL )
    {
      close(fd);
    }
  }
#endif
  if( f == NULL )
  {
    ok = false;
  }
  else
  {
    ok = fwrite(json, 1, len, f) == len;
    ok = (fclose(f) == 0) && ok;
  }

#ifdef WIN32
  ok = ok && MoveFileExA(self->tmpPath, self->path, MOVEFILE_REPLACE_EXISTING);
#else
  ok = ok && rename(self->tmpPath, self->path) == 0;
#endif

  if( !ok )
  {
    remove(self->tmpPath);
    if( !self->mutelog )
    {
      Log(TM_LOG_ERROR, "Cannot write status file %s", self->path);
      self->mutelog = true;
    }
    return 1;
  }
  self->mutelog = false;
  return 0;
}

// Call with lock held
static int writeStatus(Status* self, time_t now)
{
  updateOnline(self, now);
  char* json = createJson(self, now);
  if( json == NULL )
  {
    Log(TM_LOG_ERROR, "Status: out of memory");
    return 1;
  }
  int ret = writeFile(self, json);
  cJSON_free(json);
  self->lastWrite = now;
  self->dirty = (ret != 0); // Try again on the next poll
  return ret;
}

void Status_Poll(void)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return;
  }
  time_t now = time(NULL);
  CriticalSection_Enter(self->cs);
  if( updateOnline(self, now) )
  {
    self->dirty = true;
  }
  if( self->dirty && now != self->lastWrite )
  {
    writeStatus(self, now);
  }
  CriticalSection_Leave(self->cs);
}

int Status_Write(void)
{
  Status* self = &theStatus;
  if( !enabled )
  {
    return 1;
  }
  CriticalSection_Enter(self->cs);
  int ret = writeStatus(self, time(NULL));
  CriticalSection_Leave(self->cs);
  return ret;
}
