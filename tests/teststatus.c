// Tests of the status file (--status) against the TellStick simulator.
// No MQTT broker is needed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
#include <telldus-core.h>
#include "telldussim/telldussim.h"
#include "telldusclient.h"
#include "telldusdevice.h"
#include "config.h"
#include "status.h"
#include "mythread.h"
#include "log.h"
#include "version.h"

#define SERIAL "SIMSERIAL"
#define CHECK(c) if(!(c)) { printf("FAIL %s:%i: %s\n", __FILE__, __LINE__, #c); return 1; }

static char configFile[64];
static char statusFile[64];
static char tmpFile[70];

static bool writeConfig(void)
{
  FILE* f = fopen(configFile, "wt");
  if( f == NULL )
  {
    return false;
  }
  fprintf(f,
    "{\n"
    "  \"host\": \"127.0.0.1\",\n"
    "  \"port\": 1883,\n"
    "  \"user\": \"secretuser\",\n"
    "  \"pass\": \"secretpass\",\n"
    "  \"sensor-offline-seconds\": 2,\n"
    "  \"filter-non-transled\": \"yes\",\n"
    "  \"topic-translation\": [\n"
    "    { \"telldus\": \"telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_135\",\n"
    "      \"mqtt\": \"Home/Shed\", \"name\": \"Shed\" },\n"
    "    { \"telldus\": \"telldus/" SERIAL "/switch/2\",\n"
    "      \"mqtt\": \"Home/Lamp\", \"name\": \"Lamp\" }\n"
    "  ]\n"
    "}\n");
  fclose(f);
  return true;
}

static char* readText(const char* path)
{
  FILE* f = fopen(path, "rb");
  if( f == NULL )
  {
    return NULL;
  }
  char* text = calloc(1, 65536);
  if( text != NULL )
  {
    fread(text, 1, 65535, f);
  }
  fclose(f);
  return text;
}

static cJSON* readStatus(void)
{
  char* text = readText(statusFile);
  if( text == NULL )
  {
    return NULL;
  }
  cJSON* json = cJSON_Parse(text);
  free(text);
  return json;
}

static bool fileExists(const char* path)
{
  return access(path, F_OK) == 0;
}

static const char* getString(cJSON* object, const char* name)
{
  return cJSON_GetStringValue(cJSON_GetObjectItem(object, name));
}

static bool strEq(const char* a, const char* b)
{
  return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static double getNumber(cJSON* object, const char* name)
{
  return cJSON_GetNumberValue(cJSON_GetObjectItem(object, name));
}

static bool getBool(cJSON* object, const char* name)
{
  return cJSON_IsTrue(cJSON_GetObjectItem(object, name));
}

static cJSON* findDevice(cJSON* status, const char* type, const char* id)
{
  cJSON* device;
  cJSON_ArrayForEach(device, cJSON_GetObjectItem(status, "devices"))
  {
    if( strEq(getString(device, "type"), type) && strEq(getString(device, "id"), id) )
    {
      return device;
    }
  }
  return NULL;
}

// The file exists before telldusd is connected, with all approved keys
static int testEarlyWrite(void)
{
  time_t start = time(NULL);
  CHECK( Status_Init(statusFile) == 0 );
  CHECK( Status_Write() == 0 );
  CHECK( fileExists(statusFile) );
  CHECK( !fileExists(tmpFile) );

  struct stat st;
  CHECK( stat(statusFile, &st) == 0 );
  CHECK( (st.st_mode & 0777) == 0644 );

  cJSON* status = readStatus();
  CHECK( status != NULL );
  const char* const keys[] = { "format", "version", "started", "updated", "gateway",
    "service", "broker", "last_rf", "devices", "events" };
  for( size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++ )
  {
    CHECK( cJSON_GetObjectItem(status, keys[i]) != NULL );
  }
  CHECK( cJSON_GetArraySize(status) == (int) (sizeof(keys) / sizeof(keys[0])) );
  CHECK( getNumber(status, "format") == 1 );
  CHECK( strEq(getString(status, "version"), TELLDUS_MQTT_VERSION) );
  CHECK( getNumber(status, "started") >= start );
  CHECK( getNumber(status, "updated") >= getNumber(status, "started") );
  CHECK( getNumber(status, "last_rf") == 0 );
  CHECK( !getBool(cJSON_GetObjectItem(status, "service"), "connected") );
  CHECK( !getBool(cJSON_GetObjectItem(status, "gateway"), "found") );
  cJSON* broker = cJSON_GetObjectItem(status, "broker");
  CHECK( !getBool(broker, "connected") );
  CHECK( strEq(getString(broker, "host"), "127.0.0.1") );
  CHECK( getNumber(broker, "port") == 1883 );
  CHECK( cJSON_IsArray(cJSON_GetObjectItem(status, "devices")) );
  CHECK( cJSON_GetArraySize(cJSON_GetObjectItem(status, "devices")) == 0 );
  CHECK( cJSON_IsArray(cJSON_GetObjectItem(status, "events")) );
  CHECK( cJSON_GetArraySize(cJSON_GetObjectItem(status, "events")) == 0 );
  cJSON_Delete(status);
  return 0;
}

static int testConnect(void)
{
  TelldusSim_Reset();
  TelldusSim_SetController(5, TELLSTICK_CONTROLLER_TELLSTICK_DUO, SERIAL, true);
  TelldusSim_AddDevice(1);
  TelldusSim_AddDevice(2);
  TelldusClient* client = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(client) == 0 );
  CHECK( Status_Write() == 0 );

  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( getBool(cJSON_GetObjectItem(status, "service"), "connected") );
  cJSON* gateway = cJSON_GetObjectItem(status, "gateway");
  CHECK( getBool(gateway, "found") );
  CHECK( strEq(getString(gateway, "type"), "tellstick duo") );
  CHECK( strEq(getString(gateway, "serial"), SERIAL) );
  cJSON_Delete(status);
  return 0;
}

static int testLastRf(void)
{
  time_t before = time(NULL);
  TelldusSim_RawDeviceEvent("class:command;protocol:arctech;model:selflearning;house:1;unit:1;method:turnon;");
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( getNumber(status, "last_rf") >= before );
  CHECK( getNumber(status, "last_rf") <= time(NULL) );
  cJSON_Delete(status);
  return 0;
}

static int testSensors(void)
{
  // Named sensor: two datatypes in one entry
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 135, TELLSTICK_TEMPERATURE, "21.5");
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 135, TELLSTICK_HUMIDITY, "44");
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 135, TELLSTICK_TEMPERATURE, "21.6");
  // Unnamed sensor: not in topic-translation (filtered from MQTT)
  TelldusSim_SensorEvent("oregon", "1A2D", 80, TELLSTICK_TEMPERATURE, "3.1");
  CHECK( Status_Write() == 0 );
  CHECK( !fileExists(tmpFile) );

  cJSON* status = readStatus();
  CHECK( status != NULL );
  cJSON* named = findDevice(status, "sensor", "fineoffset_temperaturehumidity_135");
  CHECK( named != NULL );
  CHECK( strEq(getString(named, "name"), "Shed") );
  CHECK( strEq(getString(named, "topic"), "Home/Shed") );
  CHECK( getBool(named, "online") );
  CHECK( getNumber(named, "last_seen") > 0 );
  cJSON* values = cJSON_GetObjectItem(named, "values");
  CHECK( cJSON_GetArraySize(values) == 2 );
  CHECK( strEq(getString(values, "temperature"), "21.6") );
  CHECK( strEq(getString(values, "humidity"), "44") );

  cJSON* unnamed = findDevice(status, "sensor", "oregon_1A2D_80");
  CHECK( unnamed != NULL );
  CHECK( cJSON_GetObjectItem(unnamed, "name") == NULL );
  CHECK( cJSON_GetObjectItem(unnamed, "topic") == NULL );
  CHECK( getBool(unnamed, "online") );
  CHECK( strEq(getString(cJSON_GetObjectItem(unnamed, "values"), "temperature"), "3.1") );
  CHECK( getNumber(status, "last_rf") >= getNumber(unnamed, "last_seen") );
  cJSON_Delete(status);
  return 0;
}

static int testSwitches(void)
{
  TelldusDevice_Create(1);
  TelldusDevice_Create(2);
  TelldusSim_DeviceEvent(2, TELLSTICK_TURNON);
  CHECK( Status_Write() == 0 );

  cJSON* status = readStatus();
  CHECK( status != NULL );
  cJSON* named = findDevice(status, "switch", "2");
  CHECK( named != NULL );
  CHECK( strEq(getString(named, "name"), "Lamp") );
  CHECK( strEq(getString(named, "topic"), "Home/Lamp") );
  CHECK( strEq(getString(cJSON_GetObjectItem(named, "values"), "state"), "ON") );
  CHECK( cJSON_GetObjectItem(named, "online") == NULL );

  cJSON* unnamed = findDevice(status, "switch", "1");
  CHECK( unnamed != NULL );
  CHECK( cJSON_GetObjectItem(unnamed, "name") == NULL );
  CHECK( cJSON_GetObjectItem(unnamed, "topic") == NULL );
  CHECK( cJSON_GetArraySize(cJSON_GetObjectItem(unnamed, "values")) == 0 );
  cJSON_Delete(status);

  TelldusSim_DeviceEvent(2, TELLSTICK_TURNOFF);
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  named = findDevice(status, "switch", "2");
  CHECK( strEq(getString(cJSON_GetObjectItem(named, "values"), "state"), "OFF") );
  cJSON_Delete(status);
  return 0;
}

static int testNoSecrets(void)
{
  char* text = readText(statusFile);
  CHECK( text != NULL );
  bool secret = strstr(text, "secret") != NULL ||
    strstr(text, "\"pass\"") != NULL ||
    strstr(text, "\"user\"") != NULL;
  free(text);
  CHECK( !secret );
  return 0;
}

// Status_Poll writes a change at most one time per second
static int testPoll(void)
{
  CHECK( Status_Write() == 0 );
  time_t written = time(NULL);
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 99, TELLSTICK_HUMIDITY, "60");
  Status_Poll();
  if( time(NULL) == written )
  {
    char* text = readText(statusFile);
    CHECK( text != NULL );
    bool found = strstr(text, "fineoffset_temperaturehumidity_99") != NULL;
    free(text);
    CHECK( !found );
  }

  MyThread_Sleep(1100);
  Status_Poll();
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( findDevice(status, "sensor", "fineoffset_temperaturehumidity_99") != NULL );
  CHECK( !fileExists(tmpFile) );
  cJSON_Delete(status);
  return 0;
}

// Sensors go offline after sensor-offline-seconds (2 s in this test)
static int testOffline(void)
{
  MyThread_Sleep(3100);
  Status_Poll();
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( !getBool(findDevice(status, "sensor", "oregon_1A2D_80"), "online") );
  cJSON_Delete(status);

  // Start at a new second, so 1.1 s later is less than 2 s in time() units
  time_t t = time(NULL);
  while( time(NULL) == t )
  {
    MyThread_Sleep(10);
  }
  TelldusSim_SensorEvent("oregon", "1A2D", 80, TELLSTICK_TEMPERATURE, "3.2");
  MyThread_Sleep(1100);
  Status_Poll();
  status = readStatus();
  CHECK( status != NULL );
  cJSON* sensor = findDevice(status, "sensor", "oregon_1A2D_80");
  CHECK( getBool(sensor, "online") );
  CHECK( strEq(getString(cJSON_GetObjectItem(sensor, "values"), "temperature"), "3.2") );
  CHECK( !getBool(findDevice(status, "sensor", "fineoffset_temperaturehumidity_135"), "online") );
  cJSON_Delete(status);
  return 0;
}

static int testUnplug(void)
{
  TelldusSim_UnplugController();
  CHECK( !TelldusClient_IsConnected(TelldusClient_GetInstance()) );
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  cJSON* gateway = cJSON_GetObjectItem(status, "gateway");
  CHECK( !getBool(gateway, "found") );
  CHECK( strEq(getString(gateway, "serial"), SERIAL) ); // Last known
  CHECK( getBool(cJSON_GetObjectItem(status, "service"), "connected") );
  cJSON_Delete(status);
  return 0;
}

// Without --status (no Status_Init), no file is written
static int testNoFileWithoutOption(void)
{
  Status_Destroy();
  remove(statusFile);
  CHECK( !Status_IsEnabled() );
  TelldusSim_Reset();
  TelldusSim_SetController(5, TELLSTICK_CONTROLLER_TELLSTICK_DUO, SERIAL, true);
  CHECK( TelldusClient_Connect(TelldusClient_GetInstance()) == 0 );
  TelldusSim_SensorEvent("oregon", "1A2D", 80, TELLSTICK_TEMPERATURE, "3.3");
  TelldusSim_RawDeviceEvent("class:sensor;");
  Status_Poll();
  CHECK( Status_Write() != 0 );
  CHECK( !fileExists(statusFile) );
  CHECK( !fileExists(tmpFile) );
  TelldusClient_Disconnect(TelldusClient_GetInstance());
  return 0;
}

int main(void)
{
  Log_Init(TM_LOG_CONSOLE, "teststatus", TM_LOG_DEBUG, false);
  umask(077); // The status file is 0644 anyway
  snprintf(configFile, sizeof(configFile), "teststatus-%d.json", (int) getpid());
  snprintf(statusFile, sizeof(statusFile), "teststatus-%d-status.json", (int) getpid());
  snprintf(tmpFile, sizeof(tmpFile), "%s.tmp", statusFile);
  CHECK( writeConfig() );
  CHECK( Config_Load(Config_GetInstance(), configFile) == 0 );

  int res = 0;
  res |= testEarlyWrite();
  if( res == 0 )
  {
    res |= testConnect();
    res |= testLastRf();
    res |= testSensors();
    res |= testSwitches();
    res |= testNoSecrets();
    res |= testPoll();
    res |= testOffline();
    res |= testUnplug();
  }
  res |= testNoFileWithoutOption();

  Status_Destroy();
  remove(configFile);
  remove(statusFile);
  printf("%s\n", res ? "FAILED" : "PASSED");
  return res;
}
