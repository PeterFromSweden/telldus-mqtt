#ifndef CONFIG_H_
#define CONFIG_H_

#include "configjson.h"

typedef struct {
  ConfigJson configJson;
} Config;

Config* Config_GetInstance(void);
void Config_Destroy(Config* self);
int Config_Load(Config* self, char* configFilename);
int Config_LoadFile(Config* self, const char* path);
char* Config_GetStrPtr(Config* self, const char* const property);
int Config_GetInt(Config* self, const char* const property);
void Config_GetStr(Config* self, const char* const property, char* oValue, int valueLen);
char* Config_GetTopicTranslation(Config* self, char* ioString);
// Find the translation of a default topic, e.g. "telldus/{serno}/switch/2".
// Returns the "mqtt" topic and sets name (NULL if none), or returns NULL.
const char* Config_FindTopicTranslation(Config* self, const char* telldusTopic, const char** name);

#endif // !CONFIG_H_
