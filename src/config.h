#ifndef CONFIG_H_
#define CONFIG_H_

#include <stddef.h>
#include <stdbool.h>
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
// "filter-non-transled" is "yes": devices without a name are not published.
bool Config_FilterUnnamed(Config* self);
// Translate the default topics of a device in text (the Home Assistant
// config) to the user topic. serial is the TellStick serial, type is
// "sensor" or "switch", id is as in the default topic
// "telldus/{serial}/{type}/{id}". An entry in "devices" comes first, then
// "topic-translation". Returns true if an entry matched, and sets name
// (NULL if the entry has none). text has textSize bytes.
bool Config_TranslateTopics(Config* self, const char* serial, const char* type, const char* id,
  char* text, size_t textSize, const char** name);
// As Config_TranslateTopics, for the status file: returns the user topic
// and sets name (NULL if none), or returns NULL.
const char* Config_FindTopic(Config* self, const char* serial, const char* type, const char* id, const char** name);

#endif // !CONFIG_H_
