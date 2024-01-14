#ifndef CONFIGJSON_H_
#define CONFIGJSON_H_

#include <cjson/cJSON.h>

typedef struct {
  cJSON* json;
  char* content;
  long contentLen;
  long contentMaxLen;
} ConfigJson;

void ConfigJson_Init(ConfigJson* self);
void ConfigJson_Destroy(ConfigJson* self);
int ConfigJson_LoadContent(ConfigJson* self, char* configFilename);
char* ConfigJson_GetContent(ConfigJson* self);
int ConfigJson_ParseContent(ConfigJson* self);
void ConfigJson_FreeContent(ConfigJson* self);
char* ConfigJson_GetStrPtr(ConfigJson* self, const char* const property);
int ConfigJson_GetInt(ConfigJson* self, const char* const property);

#endif // !CONFIGJSON_H_
