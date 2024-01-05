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
    fprintf(stderr, "%s has wrong size %i.\r\n", configFilename, (int)readCount);
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

void Config_GetStr(Config* self, const char* const property, char* outputString, int outputLen)
{
  char* strp = getStrp(self->configJson, property);
  if ( strp != NULL && outputString != NULL )
  {
    strncpy(outputString, strp, outputLen);
  }
  else
  {
    fprintf(stderr, "Property %s was not a found string\r\n", property);
  }
}

void Config_GetInt(Config* self, const char* const property, int* outputString)
{
  *outputString = getInt(self->configJson, property);
  if ( *outputString == -1 )
  {
    fprintf(stderr, "Property %s was not a found int\r\n", property);
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
  cJSON* item = cJSON_GetObjectItem(self->configJson, "topic-translation");
  int rc = 1;

  if ( cJSON_IsArray(item) )
  {
    int arrSize = cJSON_GetArraySize(item);
    int i = 0;
    while ( i < arrSize && rc == 1 )
    {
      cJSON* arrItem = cJSON_GetArrayItem(item, i);
      char* find = getStrp(arrItem, inputKey);
      char* replace = getStrp(arrItem, outputKey);
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
    fprintf(stderr, "Property topic-translation was not a found array\r\n");
  }
  return rc;
}