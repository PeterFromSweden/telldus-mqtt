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
  return ConfigJson_ParseContent( &self->configJson );
}

int Config_LoadFile(Config* self, const char* path)
{
  if (ConfigJson_LoadFile( &self->configJson, path ))
  {
    return 1;
  }
  Log(TM_LOG_INFO, "Config %s", path);
  return ConfigJson_ParseContent( &self->configJson );
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
  if ( strp != NULL && outputString != NULL && outputLen > 0 )
  {
    strncpy(outputString, strp, outputLen);
    outputString[outputLen - 1] = '\0';
  }
  else
  {
    Log(TM_LOG_ERROR, "Property %s was not a found string", property);
  }
}

bool Config_FilterUnnamed(Config* self)
{
  // Optional: not with Config_GetStrPtr, which logs an error when missing
  char* filter = cJSON_GetStringValue(cJSON_GetObjectItem(self->configJson.json, "filter-non-transled"));
  return filter != NULL && strcmp(filter, "yes") == 0;
}

// Find the "devices" entry of a device. An entry with a gateway matches only
// that TellStick serial, an entry without one matches any. Returns NULL if none.
static cJSON* findDeviceEntry(Config* self, const char* serial, const char* type, const char* id)
{
  cJSON* entry;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItem(self->configJson.json, "devices"))
  {
    char* entryType = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "type"));
    char* entryId = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "id"));
    char* topic = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "topic"));
    char* gateway = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "gateway"));
    if( entryType == NULL || entryId == NULL || topic == NULL || *topic == '\0' ||
        strcmp(entryType, type) != 0 || strcmp(entryId, id) != 0 )
    {
      continue;
    }
    if( gateway != NULL && *gateway != '\0' && strcmp(gateway, serial) != 0 )
    {
      continue;
    }
    return entry;
  }
  return NULL;
}

static void defaultTopic(char* topic, size_t topicSize, const char* serial, const char* type, const char* id)
{
  snprintf(topic, topicSize, "telldus/%s/%s/%s", serial, type, id);
}

bool Config_TranslateTopics(Config* self, const char* serial, const char* type, const char* id,
  char* text, size_t textSize, const char** name)
{
  *name = NULL;

  // "devices": as a topic-translation of the default topic
  cJSON* entry = findDeviceEntry(self, serial, type, id);
  if( entry != NULL )
  {
    char find[160];
    defaultTopic(find, sizeof(find), serial, type, id);
    char* topic = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "topic"));
    if( ReplaceWordsSize(text, textSize, find, topic) )
    {
      *name = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "name"));
      return true;
    }
    Log(TM_LOG_ERROR, "Topic %s is too long => not used", topic);
    return false;
  }

  cJSON* item = cJSON_GetObjectItem(self->configJson.json, "topic-translation");
  if( !cJSON_IsArray(item) )
  {
    // topic-translation is optional
    Log(TM_LOG_DEBUG, "Property topic-translation was not a found array");
    return false;
  }
  cJSON* arrItem;
  cJSON_ArrayForEach(arrItem, item)
  {
    char* find = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "telldus"));
    char* replace = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "mqtt"));
    if( find != NULL && replace != NULL )
    {
      if( ReplaceWordsSize(text, textSize, find, replace) )
      {
        *name = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "name"));
        return true;
      }
    }
  }
  Log(TM_LOG_DEBUG, "No match of translation");
  return false;
}

const char* Config_FindTopic(Config* self, const char* serial, const char* type, const char* id, const char** name)
{
  *name = NULL;
  cJSON* entry = findDeviceEntry(self, serial, type, id);
  if( entry != NULL )
  {
    *name = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "name"));
    return cJSON_GetStringValue(cJSON_GetObjectItem(entry, "topic"));
  }

  char telldusTopic[160];
  defaultTopic(telldusTopic, sizeof(telldusTopic), serial, type, id);
  cJSON* arrItem;
  cJSON_ArrayForEach(arrItem, cJSON_GetObjectItem(self->configJson.json, "topic-translation"))
  {
    char* find = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "telldus"));
    char* replace = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "mqtt"));
    if( find != NULL && replace != NULL && strcmp(find, telldusTopic) == 0 )
    {
      *name = cJSON_GetStringValue(cJSON_GetObjectItem(arrItem, "name"));
      return replace;
    }
  }
  return NULL;
}
