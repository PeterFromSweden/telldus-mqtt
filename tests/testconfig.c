#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "stringutils.h"
#include "config.h"
#include "configjson.h"
#include "asrt.h"
#include "log.h"

#define CHECK(c) if(!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); res = 1; }

Config* config;

static int writeFile(const char* path, const char* content)
{
  FILE* f = fopen(path, "wt");
  if( f == NULL )
  {
    return 1;
  }
  fputs(content, f);
  fclose(f);
  return 0;
}

static bool strEq(const char* a, const char* b)
{
  return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int loadJson(const char* content);

static const char* translationJson =
  "{ \"topic-translation\": [\n"
  "    { \"telldus\": \"telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_135\", \"mqtt\": \"Hemma/Uteförråd\", \"name\": \"Uteförråd\" },\n"
  "    { \"telldus\": \"telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183\", \"mqtt\": \"Hemma/Tvätt\", \"name\": \"Tvättstuga\" }\n"
  "  ] }\n";

static int testTranslation(void)
{
  int res = 0;
  CHECK( loadJson(translationJson) == 0 );
  char iostr[100] = "telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183/Temperature";
  const char* name;
  CHECK( Config_TranslateTopics(config, "A703AKOX", "sensor", "fineoffset_temperaturehumidity_183",
    iostr, sizeof(iostr), &name) );
  CHECK( strEq(name, "Tvättstuga") );
  CHECK( strEq(iostr, "Hemma/Tvätt/Temperature") );
  return res;
}

// Replace the loaded config with content
static int loadJson(const char* content)
{
  const char* path = "testconfig-devices.json";
  ConfigJson_Destroy(&config->configJson);
  if( writeFile(path, content) != 0 )
  {
    return 1;
  }
  int ret = Config_LoadFile(config, path);
  remove(path);
  return ret;
}

static const char* devicesJson =
  "{ \"host\": \"127.0.0.1\", \"port\": 1883, \"filter-non-transled\": \"yes\",\n"
  "  \"devices\": [\n"
  "    { \"type\": \"sensor\", \"id\": \"fineoffset_temperaturehumidity_135\", \"name\": \"Uteförråd\", \"topic\": \"Hemma/Uteförråd\" },\n"
  "    { \"type\": \"switch\", \"id\": \"6\", \"name\": \"Uteförråd-lampa\", \"topic\": \"Hemma/Uteförråd\" },\n"
  "    { \"type\": \"sensor\", \"id\": \"oregon_1A2D_80\", \"name\": \"Landet\", \"topic\": \"Landet/Kök\", \"gateway\": \"A703AJQJ\" },\n"
  "    { \"type\": \"sensor\", \"id\": \"oregon_1A2D_80\", \"name\": \"Båt\", \"topic\": \"Båthall/Båt\", \"gateway\": \"A6028FQX\" },\n"
  "    { \"type\": \"sensor\", \"id\": \"mandolyn_temperaturehumidity_11\", \"topic\": \"Hemma/Fönster\", \"gateway\": \"\" },\n"
  "    { \"type\": \"sensor\", \"id\": \"no_topic_1\", \"name\": \"No topic\" },\n"
  "    { \"type\": \"switch\", \"id\": \"9\", \"name\": \"Long\", \"topic\": \"Hemma/0123456789012345678901234567890123456789012345678901234567890123456789\" }\n"
  "  ],\n"
  "  \"topic-translation\": [\n"
  "    { \"telldus\": \"\", \"mqtt\": \"Empty/Key\", \"name\": \"Empty key\" },\n"
  "    { \"telldus\": \"telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_135\", \"mqtt\": \"Old/Topic\", \"name\": \"Old\" },\n"
  "    { \"telldus\": \"telldus/AE01DHPQ/switch/2\", \"mqtt\": \"Hemma/Arbetsrum\" }\n"
  "  ]\n"
  "}\n";

// "devices": the same result as a topic-translation of the default topic
static int testDevices(void)
{
  int res = 0;
  const char* name;
  CHECK( loadJson(devicesJson) == 0 );
  CHECK( Config_FilterUnnamed(config) );

  // "devices" wins over topic-translation. Only whole words are replaced.
  char text[300] =
    "\"telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_135/Temperature\" "
    "telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_135/status "
    "telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_1350/status";
  CHECK( Config_TranslateTopics(config, "AE01DHPQ", "sensor", "fineoffset_temperaturehumidity_135",
    text, sizeof(text), &name) );
  CHECK( strEq(name, "Uteförråd") );
  CHECK( strEq(text, "\"Hemma/Uteförråd/Temperature\" Hemma/Uteförråd/status "
    "telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_1350/status") );
  CHECK( strEq(Config_FindTopic(config, "AE01DHPQ", "sensor", "fineoffset_temperaturehumidity_135", &name),
    "Hemma/Uteförråd") );
  CHECK( strEq(name, "Uteförråd") );

  // A sensor and a switch can have the same topic. The type must match.
  CHECK( strEq(Config_FindTopic(config, "AE01DHPQ", "switch", "6", &name), "Hemma/Uteförråd") );
  CHECK( strEq(name, "Uteförråd-lampa") );
  CHECK( Config_FindTopic(config, "AE01DHPQ", "switch", "fineoffset_temperaturehumidity_135", &name) == NULL );
  CHECK( name == NULL );
  char switchText[100] = "telldus/AE01DHPQ/switch/6/set";
  CHECK( Config_TranslateTopics(config, "AE01DHPQ", "switch", "6", switchText, sizeof(switchText), &name) );
  CHECK( strEq(switchText, "Hemma/Uteförråd/set") );

  // gateway: the same id on two TellSticks
  CHECK( strEq(Config_FindTopic(config, "A703AJQJ", "sensor", "oregon_1A2D_80", &name), "Landet/Kök") );
  CHECK( strEq(name, "Landet") );
  CHECK( strEq(Config_FindTopic(config, "A6028FQX", "sensor", "oregon_1A2D_80", &name), "Båthall/Båt") );
  CHECK( strEq(name, "Båt") );
  CHECK( Config_FindTopic(config, "AE01DHPQ", "sensor", "oregon_1A2D_80", &name) == NULL );
  char boatText[100] = "telldus/A6028FQX/sensor/oregon_1A2D_80/status";
  CHECK( Config_TranslateTopics(config, "A6028FQX", "sensor", "oregon_1A2D_80", boatText, sizeof(boatText), &name) );
  CHECK( strEq(boatText, "Båthall/Båt/status") );
  char otherText[100] = "telldus/AE01DHPQ/sensor/oregon_1A2D_80/status";
  CHECK( !Config_TranslateTopics(config, "AE01DHPQ", "sensor", "oregon_1A2D_80", otherText, sizeof(otherText), &name) );
  CHECK( strEq(otherText, "telldus/AE01DHPQ/sensor/oregon_1A2D_80/status") );

  // An empty gateway matches any TellStick. No name: matched, name NULL.
  char windowText[100] = "telldus/X/sensor/mandolyn_temperaturehumidity_11/status";
  CHECK( Config_TranslateTopics(config, "X", "sensor", "mandolyn_temperaturehumidity_11", windowText, sizeof(windowText), &name) );
  CHECK( name == NULL );
  CHECK( strEq(windowText, "Hemma/Fönster/status") );

  // No topic: not used
  CHECK( Config_FindTopic(config, "X", "sensor", "no_topic_1", &name) == NULL );

  // topic-translation still works, also without a name
  char oldText[100] = "telldus/AE01DHPQ/switch/2/set";
  CHECK( Config_TranslateTopics(config, "AE01DHPQ", "switch", "2", oldText, sizeof(oldText), &name) );
  CHECK( name == NULL );
  CHECK( strEq(oldText, "Hemma/Arbetsrum/set") );
  CHECK( strEq(Config_FindTopic(config, "AE01DHPQ", "switch", "2", &name), "Hemma/Arbetsrum") );

  // Not named
  char noText[100] = "telldus/AE01DHPQ/switch/3/set";
  CHECK( !Config_TranslateTopics(config, "AE01DHPQ", "switch", "3", noText, sizeof(noText), &name) );
  CHECK( strEq(noText, "telldus/AE01DHPQ/switch/3/set") );
  CHECK( Config_FindTopic(config, "AE01DHPQ", "switch", "3", &name) == NULL );

  // A topic too long for the buffer: no overflow, not changed
  char longText[60] = "telldus/AE01DHPQ/switch/9/set";
  CHECK( !Config_TranslateTopics(config, "AE01DHPQ", "switch", "9", longText, sizeof(longText), &name) );
  CHECK( strEq(longText, "telldus/AE01DHPQ/switch/9/set") );
  return res;
}

static int testFilterKey(void)
{
  int res = 0;
  CHECK( loadJson("{ \"filter-non-transled\": \"no\" }") == 0 );
  CHECK( !Config_FilterUnnamed(config) );
  CHECK( loadJson("{ \"host\": \"h\" }") == 0 );
  CHECK( !Config_FilterUnnamed(config) );
  // No devices and no topic-translation
  const char* name;
  char text[100] = "telldus/X/switch/1/set";
  CHECK( !Config_TranslateTopics(config, "X", "switch", "1", text, sizeof(text), &name) );
  CHECK( Config_FindTopic(config, "X", "switch", "1", &name) == NULL );
  return res;
}

// --config <path>: the given file is used, not the default telldus-mqtt.json
static int testLoadFile(void)
{
  int res = 0;
  const char* path = "testconfig-given.json";
  CHECK( writeFile(path, "{ \"host\": \"10.1.2.3\", \"port\": 1884, \"topic-translation\": [] }") == 0 );
  CHECK( Config_LoadFile(config, path) == 0 );
  char* host = Config_GetStrPtr(config, "host");
  CHECK( host != NULL && strcmp(host, "10.1.2.3") == 0 );
  CHECK( Config_GetInt(config, "port") == 1884 );
  remove(path);
  return res;
}

static int testLoadFileMissing(void)
{
  int res = 0;
  ConfigJson cj;
  ConfigJson_Init(&cj);
  // No search in other directories
  CHECK( ConfigJson_LoadFile(&cj, "no-such-dir/telldus-mqtt.json") != 0 );
  ConfigJson_Destroy(&cj);
  return res;
}

static int testLoadFileInvalid(void)
{
  int res = 0;
  const char* path = "testconfig-invalid.json";
  CHECK( writeFile(path, "{ \"host\": ") == 0 );
  ConfigJson cj;
  ConfigJson_Init(&cj);
  CHECK( ConfigJson_LoadFile(&cj, path) == 0 );
  CHECK( ConfigJson_ParseContent(&cj) != 0 );
  ConfigJson_Destroy(&cj);
  remove(path);
  return res;
}

static int testDefaultConfig(void)
{
  int res = 0;
  const char* name;
  char text[100] = "telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183/Temperature";
  CHECK( !Config_FilterUnnamed(config) );
  CHECK( !Config_TranslateTopics(config, "A703AKOX", "sensor", "fineoffset_temperaturehumidity_183",
    text, sizeof(text), &name) );
  CHECK( strEq(text, "telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183/Temperature") );
  CHECK( Config_FindTopic(config, "A703AKOX", "sensor", "fineoffset_temperaturehumidity_183", &name) == NULL );
  return res;
}

int main(void)
{
  Log_Init(TM_LOG_CONSOLE, "testconfig", TM_LOG_DEBUG, false);
  config = Config_GetInstance();
  int res = 0;
  // The default config: loads, publishes all, names nothing
  CHECK( Config_Load(config, "telldus-mqtt.json") == 0 );
  res |= testDefaultConfig();
  res |= testTranslation();
  res |= testLoadFileMissing();
  res |= testLoadFileInvalid();
  res |= testDevices();
  res |= testFilterKey();
  ConfigJson_Destroy(&config->configJson);
  res |= testLoadFile();
  printf("%s\n", res ? "FAILED" : "PASSED");
  return res;
}
