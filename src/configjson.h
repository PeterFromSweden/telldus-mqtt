#ifndef CONFIGJSON_H_
#define CONFIGJSON_H_

#include <cjson/cJSON.h>
#include <stdbool.h>

typedef struct {
  cJSON* json;
  char* content;
  long contentLen;
  long contentMaxLen;
} ConfigJson;

void ConfigJson_Init(ConfigJson* self);
void ConfigJson_Destroy(ConfigJson* self);

int ConfigJson_LoadContent(ConfigJson* self, char* configFilename);
char* ConfigJson_CopyToContent(ConfigJson* self, char* fromString);
char* ConfigJson_GetContent(ConfigJson* self);
long ConfigJson_GetContentLen(ConfigJson* self);
long ConfigJson_GetContentMaxLen(ConfigJson* self);
int ConfigJson_ParseContent(ConfigJson* self);
void ConfigJson_FreeContent(ConfigJson* self);

void ConfigJson_FreeJson(ConfigJson* self);

char* ConfigJson_GetStringFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] );
char* ConfigJson_GetStringFromProp(ConfigJson* self, const char* const property);

bool ConfigJson_SetStringFromPropList(
  ConfigJson* self, 
  const char* const propertyList[],
  const char* propertyToChange,
  const char* str );
// bool ConfigJson_SetStringFromProp(
//   ConfigJson* self, 
//   const char* const property,
//   const char* propertyToChange,
//   const char* str);

char* ConfigJson_GetJsonFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] );
char* ConfigJson_GetJsonFromProp(ConfigJson* self, const char* const property);

int ConfigJson_GetIntFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] );
int ConfigJson_GetIntFromProp(ConfigJson* self, const char* const property);

#endif // !CONFIGJSON_H_
