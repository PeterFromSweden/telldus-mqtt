#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"
#include "stringutils.h"
#include "config.h"

static Config theConfig;
static bool created;

Config* Config_GetInstance(void)
{
  Config* self = &theConfig;
  if( created == false )
  {
    *self = (Config){ 0 };
    ConfigJson_Init(&self->configJson);
    created = true;
  }
  return self;
}

void Config_Destroy(Config* self)
{
  ConfigJson_FreeContent( &self->configJson );
}

int Config_Load(Config* self, char* configFilename )
{
  if (ConfigJson_LoadContent( &self->configJson, configFilename ))
  {
    return 1;
  }
  ConfigJson_ParseContent( &self->configJson );
  //ConfigJson_FreeContent( &self->configJson );
  return 0;
}

char* Config_GetStrPtr(Config* self, const char* const property)
{
  return ConfigJson_GetStringFromProp(&self->configJson, property);
}

int Config_GetInt(Config* self, const char* const property)
{
  return ConfigJson_GetIntFromProp( &self->configJson, property );
}

void Config_GetStr(Config* self, const char* const property, char* outputString, int outputLen)
{
  char* strp = Config_GetStrPtr(self, property);
  if ( strp != NULL && outputString != NULL )
  {
    strncpy(outputString, strp, outputLen);
  }
  else
  {
    Log(TM_LOG_ERROR, "Property %s was not a found string", property);
  }
}

char* Config_GetTopicTranslation(Config* self, char* ioString)
{
  cJSON* item = cJSON_GetObjectItem(self->configJson.json, "topic-translation");

  if ( cJSON_IsArray(item) )
  {
    int arrSize = cJSON_GetArraySize(item);
    int i = 0;
    while ( i < arrSize )
    {
      cJSON* arrItem = cJSON_GetArrayItem(item, i);
      char* find = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "telldus"));
      char* replace = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "mqtt"));
      if( find != NULL )
      {
        if( ReplaceWords(ioString, find, replace) )
        {
          // Log(TM_LOG_DEBUG, "Content2:\n%s", cJSON_Print(content));
          return cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "name"));
        }
      }
      i++;
    }
    Log(TM_LOG_DEBUG, "No match of translation");
  }
  else
  {
   Log(TM_LOG_ERROR, "Property topic-translation was not a found array");
  }
  return NULL;
}