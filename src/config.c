#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"
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

int Config_Load(Config* self, char* configFilename )
{
  ConfigJson_LoadContent( &self->configJson, configFilename );
  ConfigJson_ParseContent( &self->configJson );
  ConfigJson_FreeContent( &self->configJson );
  return 0;
}

char* Config_GetStrPtr(Config* self, const char* const property)
{
  return ConfigJson_GetStrPtr(&self->configJson, property);
}

int Config_GetInt(Config* self, const char* const property)
{
  return ConfigJson_GetInt( &self->configJson, property );
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

int Config_GetTopicTranslation(
  Config* self, 
  const char* const inputKey,
  const char* const inputString, 
  const char* const outputKey,
  char* outputString,
  int outputLen)
{
  cJSON* item = cJSON_GetObjectItem(self->configJson.json, "topic-translation");
  int rc = 1;

  if ( cJSON_IsArray(item) )
  {
    int arrSize = cJSON_GetArraySize(item);
    int i = 0;
    while ( i < arrSize && rc == 1 )
    {
      cJSON* arrItem = cJSON_GetArrayItem(item, i);
      char* find = Config_GetStrPtr((Config*)arrItem, inputKey);
      char* replace = Config_GetStrPtr((Config*)arrItem, outputKey);
      if ( find != NULL )
      {
        char* sub_string = strstr(inputString, find);
        if ( sub_string != NULL )
        {
          // printf("Matched %s with %s\r\n", inputString, find);
          // Copy the part before the sub_string to buffer
          strncpy(outputString, inputString, sub_string - inputString);
          outputLen -= (int) (sub_string - inputString);

          // Null-terminate the buffer
          outputString[sub_string - inputString] = '\0';

          // Concatenate the replacement sub_string
          strncat(outputString, replace, outputLen);
          outputLen -= (int) strlen(replace);

          // Concatenate the rest of the original string
          strncat(outputString, sub_string + strlen(find), outputLen);

          rc = 0;

        }
      }
      i++;
    }
  }
  else
  {
   Log(TM_LOG_ERROR, "Property topic-translation was not a found array");
  }
  return rc;
}