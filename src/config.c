#include <stdio.h>
#include <string.h>
#include "config.h"

void Config_Init(Config* self)
{
  *self = (Config){ 0 };
  printf("cJson %s\r\n", cJSON_Version());
}

int Config_Load(Config* self, char* configFilename )
{
  FILE* f = fopen(configFilename, "rt");
  if ( !f )
  {
    perror(configFilename);
    return 1;
  }

  char content[2048];
  size_t readCount = fread(content, 1, sizeof(content), f);
  fclose(f);
  if ( readCount >= sizeof(content) || readCount < 1)
  {
    fprintf(stderr, "%s has wrong size %i.\r\n", configFilename, readCount);
    return 1;
  }

  self->configJson = cJSON_Parse(content);
  if ( self->configJson == NULL )
  {
    const char* error_ptr = cJSON_GetErrorPtr();
    if ( error_ptr != NULL )
    {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    
    return 1;
  }

  cJSON_Print(self->configJson);

  return 0;
}

static char* getStrp(cJSON* cjson, const char* const property)
{
  cJSON* item = cJSON_GetObjectItem(cjson, property);
  if ( cJSON_IsString(item) )
  {
    return item->valuestring;
  }
  return NULL;
}

static int getInt(cJSON* cjson, const char* const property)
{
  cJSON* item = cJSON_GetObjectItem(cjson, property);
  if ( cJSON_IsNumber(item) )
  {
    return item->valueint;
  }
  return -1;
}

void Config_GetStr(Config* self, const char* const property, char* oValue, int valueLen)
{
  char* strp = getStrp(self->configJson, property);
  if ( strp != NULL && oValue != NULL )
  {
    strncpy(oValue, strp, valueLen);
  }
  else
  {
    fprintf(stderr, "Property %s was not a found string\r\n", property);
  }
}

void Config_GetInt(Config* self, const char* const property, int* oValue)
{
  *oValue = getInt(self->configJson, property);
  if ( *oValue == -1 )
  {
    fprintf(stderr, "Property %s was not a found int\r\n", property);
  }
}

int Config_GetTopicTranslation(Config* self, const char* const telldusstring, char* oValue, int valueLen)
{
  cJSON* item = cJSON_GetObjectItem(self->configJson, "topic-translation");
  int rc = 1;

  if ( cJSON_IsArray(item) )
  {
    int arrSize = cJSON_GetArraySize(item);
    int i = 0;
    while ( i < arrSize && rc == 1 )
    {
      cJSON* arrItem = cJSON_GetArrayItem(item, i);
      char* find = getStrp(arrItem, "input");
      char* replace = getStrp(arrItem, "output");
      if ( find != NULL )
      {
        char* substring = strstr(telldusstring, find);
        if ( substring != NULL )
        {
          // printf("Matched %s with %s\r\n", telldusstring, find);
          // Copy the part before the substring to buffer
          strncpy(oValue, telldusstring, substring - telldusstring, valueLen);
          valueLen -= substring - telldusstring;

          // Null-terminate the buffer
          oValue[substring - telldusstring] = '\0';

          // Concatenate the replacement substring
          strncat(oValue, replace, valueLen);
          valueLen -= strlen(replace);

          // Concatenate the rest of the original string
          strncat(oValue, substring + strlen(find), valueLen);

          rc = 0;

        }
      }
      i++;
    }
  }
  else
  {
    fprintf(stderr, "Property topic-translation was not a found array\r\n");
  }
  return rc;
}