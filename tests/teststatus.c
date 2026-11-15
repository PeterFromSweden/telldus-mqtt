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
  const char* const keys[] = { "format", "version", "started", "updated", "running", "gateway",
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
  CHECK( getBool(status, "running") );
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

static cJSON* getEvents(cJSON* status)
{
  return cJSON_GetObjectItem(status, "events");
}

// Raw RF command events are listed newest first, with the raw keys as strings
static int testEvents(void)
{
  // From testLastRf
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 1 );
  cJSON_Delete(status);

  time_t before = time(NULL);
  TelldusSim_RawDeviceEvent("class:command;protocol:arctech;model:selflearning;house:15858734;unit:1;group:0;method:turnon;");
  // Spaces are trimmed, other keys are kept as they are
  TelldusSim_RawDeviceEvent(" class : command ; protocol:sartano; model:codeswitch; code:0111010000 ;");
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 3 );
  cJSON* newest = cJSON_GetArrayItem(getEvents(status), 0);
  CHECK( cJSON_GetArraySize(newest) == 5 );
  CHECK( cJSON_IsNumber(cJSON_GetObjectItem(newest, "time")) );
  CHECK( getNumber(newest, "time") >= before );
  CHECK( getNumber(newest, "time") <= time(NULL) );
  CHECK( strEq(getString(newest, "class"), "command") );
  CHECK( strEq(getString(newest, "protocol"), "sartano") );
  CHECK( strEq(getString(newest, "model"), "codeswitch") );
  CHECK( strEq(getString(newest, "code"), "0111010000") );
  cJSON* event = cJSON_GetArrayItem(getEvents(status), 1);
  CHECK( cJSON_GetArraySize(event) == 8 );
  CHECK( strEq(getString(event, "class"), "command") );
  CHECK( strEq(getString(event, "protocol"), "arctech") );
  CHECK( strEq(getString(event, "model"), "selflearning") );
  CHECK( strEq(getString(event, "house"), "15858734") );
  CHECK( strEq(getString(event, "unit"), "1") );
  CHECK( strEq(getString(event, "group"), "0") );
  CHECK( strEq(getString(event, "method"), "turnon") );
  CHECK( strEq(getString(cJSON_GetArrayItem(getEvents(status), 2), "house"), "1") );
  cJSON_Delete(status);
  return 0;
}

// Sensor events are not listed, but set last_rf
static int testSensorEventNotListed(void)
{
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  double lastRf = getNumber(status, "last_rf");
  cJSON_Delete(status);

  MyThread_Sleep(1100);
  TelldusSim_RawDeviceEvent("class:sensor;protocol:fineoffset;id:167;model:temperaturehumidity;humidity:44;temp:15.8;");
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  CHECK( getNumber(status, "last_rf") > lastRf );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 3 );
  CHECK( strEq(getString(cJSON_GetArrayItem(getEvents(status), 0), "protocol"), "sartano") );
  cJSON_Delete(status);
  return 0;
}

// The same message again only updates the time of the newest entry
static int testEventDuplicate(void)
{
  const char* raw = "class:command;protocol:arctech;model:selflearning;house:42;unit:2;group:0;method:turnoff;";
  TelldusSim_RawDeviceEvent(raw);
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 4 );
  double first = getNumber(cJSON_GetArrayItem(getEvents(status), 0), "time");
  cJSON_Delete(status);

  MyThread_Sleep(1100);
  TelldusSim_RawDeviceEvent(raw);
  TelldusSim_RawDeviceEvent(raw);
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 4 );
  cJSON* newest = cJSON_GetArrayItem(getEvents(status), 0);
  CHECK( strEq(getString(newest, "house"), "42") );
  CHECK( getNumber(newest, "time") > first );
  cJSON_Delete(status);

  // Not the newest: a new entry
  TelldusSim_RawDeviceEvent("class:command;protocol:arctech;model:selflearning;house:15858734;unit:1;group:0;method:turnon;");
  TelldusSim_RawDeviceEvent(raw);
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  CHECK( cJSON_GetArraySize(getEvents(status)) == 6 );
  CHECK( strEq(getString(cJSON_GetArrayItem(getEvents(status), 0), "house"), "42") );
  CHECK( strEq(getString(cJSON_GetArrayItem(getEvents(status), 1), "house"), "15858734") );
  cJSON_Delete(status);
  return 0;
}

// Only the newest STATUS_MAX_EVENTS events are kept
static int testEventMax(void)
{
  for( int i = 0; i < STATUS_MAX_EVENTS + 5; i++ )
  {
    char raw[100];
    snprintf(raw, sizeof(raw), "class:command;protocol:arctech;model:codeswitch;house:A;unit:%i;method:turnon;", i);
    TelldusSim_RawDeviceEvent(raw);
  }
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  cJSON* events = getEvents(status);
  CHECK( cJSON_GetArraySize(events) == STATUS_MAX_EVENTS );
  for( int i = 0; i < STATUS_MAX_EVENTS; i++ )
  {
    char unit[12];
    snprintf(unit, sizeof(unit), "%i", STATUS_MAX_EVENTS + 4 - i);
    CHECK( strEq(getString(cJSON_GetArrayItem(events, i), "unit"), unit) );
  }
  cJSON_Delete(status);
  return 0;
}

// Bad raw text does not crash and gives a valid file
static int testEventMalformed(void)
{
  char longRaw[400];
  snprintf(longRaw, sizeof(longRaw), "class:command;protocol:x;");
  while( strlen(longRaw) < sizeof(longRaw) - 20 )
  {
    strcat(longRaw, "abcdefgh:12345678;");
  }
  char longValue[400];
  snprintf(longValue, sizeof(longValue), "class:command;protocol:");
  memset(longValue + strlen(longValue), 'v', sizeof(longValue) - strlen(longValue) - 1);
  longValue[sizeof(longValue) - 1] = '\0';

  // Not listed: no class:command
  TelldusSim_RawDeviceEvent("");
  TelldusSim_RawDeviceEvent(";;;");
  TelldusSim_RawDeviceEvent(" ; ;: ;:;");
  TelldusSim_RawDeviceEvent("no colon at all");
  TelldusSim_RawDeviceEvent("class;command;");
  TelldusSim_RawDeviceEvent("class:");
  CHECK( Status_Write() == 0 );
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( strEq(getString(cJSON_GetArrayItem(getEvents(status), 0), "unit"), "24") );
  cJSON_Delete(status);

  // Listed: bad parts are skipped, "time" from the raw text is not used
  time_t before = time(NULL);
  TelldusSim_RawDeviceEvent("junk;:empty;class:command;;novalue:;time:5;a:b:c;class:sensor;method:turnon");
  TelldusSim_RawDeviceEvent(longRaw);
  TelldusSim_RawDeviceEvent(longValue);
  CHECK( Status_Write() == 0 );
  status = readStatus();
  CHECK( status != NULL );
  cJSON* events = getEvents(status);
  CHECK( cJSON_GetArraySize(events) == STATUS_MAX_EVENTS );

  cJSON* event = cJSON_GetArrayItem(events, 0);
  CHECK( cJSON_GetArraySize(event) == 2 ); // The part cut in the middle is not listed
  CHECK( strEq(getString(event, "class"), "command") );

  event = cJSON_GetArrayItem(events, 1);
  CHECK( strEq(getString(event, "protocol"), "x") );
  CHECK( strEq(getString(event, "abcdefgh"), "12345678") ); // Cut after a complete part

  event = cJSON_GetArrayItem(events, 2);
  CHECK( cJSON_GetArraySize(event) == 5 );
  CHECK( getNumber(event, "time") >= before );
  CHECK( strEq(getString(event, "class"), "command") );
  CHECK( strEq(getString(event, "novalue"), "") );
  CHECK( strEq(getString(event, "a"), "b:c") );
  CHECK( strEq(getString(event, "method"), "turnon") );
  cJSON_Delete(status);

  char* text = readText(statusFile);
  CHECK( text != NULL );
  bool cut = strstr(text, "vvvv") == NULL && strstr(text, "abcdefgh") != NULL;
  free(text);
  CHECK( cut );
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

// At exit: "running" is false, the connection state is kept
static int testStop(void)
{
  Status_Stop();
  cJSON* status = readStatus();
  CHECK( status != NULL );
  CHECK( !getBool(status, "running") );
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
    res |= testEvents();
    res |= testSensorEventNotListed();
    res |= testEventDuplicate();
    res |= testEventMax();
    res |= testEventMalformed();
    res |= testSensors();
    res |= testSwitches();
    res |= testNoSecrets();
    res |= testPoll();
    res |= testOffline();
    res |= testUnplug();
    res |= testStop();
  }
  res |= testNoFileWithoutOption();

  Status_Destroy();
  remove(configFile);
  remove(statusFile);
  printf("%s\n", res ? "FAILED" : "PASSED");
  return res;
}
