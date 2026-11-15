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

static int testTranslation(void)
{
  int res = 0;
  char iostr[] = "telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183";
  char* name = Config_GetTopicTranslation(config, iostr);
  CHECK( name != NULL );
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

int main(void)
{
  Log_Init(TM_LOG_CONSOLE, "testconfig", TM_LOG_DEBUG, false);
  config = Config_GetInstance();
  int res = 0;
  CHECK( Config_Load(config, "telldus-mqtt.json") == 0 );
  res |= testTranslation();
  res |= testLoadFileMissing();
  res |= testLoadFileInvalid();
  ConfigJson_Destroy(&config->configJson);
  res |= testLoadFile();
  printf("%s\n", res ? "FAILED" : "PASSED");
  return res;
}
