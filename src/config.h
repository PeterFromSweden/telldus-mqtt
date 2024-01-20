#ifndef CONFIG_H_
#define CONFIG_H_

#include "configjson.h"

typedef struct {
  ConfigJson configJson;
} Config;

Config* Config_GetInstance(void);
void Config_Destroy(Config* self);
int Config_Load(Config* self, char* configFilename);
char* Config_GetStrPtr(Config* self, const char* const property);
int Config_GetInt(Config* self, const char* const property);
void Config_GetStr(Config* self, const char* const property, char* oValue, int valueLen);
char* Config_GetTopicTranslation(Config* self, char* ioString);

#endif // !CONFIG_H_
