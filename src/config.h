#ifndef CONFIG_H_
#define CONFIG_H_

#include "configjson.h"

typedef struct {
  ConfigJson configJson;
} Config;

Config* Config_GetInstance(void);
int Config_Load(Config* self, char* configFilename);
char* Config_GetStrPtr(Config* self, const char* const property);
int Config_GetInt(Config* self, const char* const property);
void Config_GetStr(Config* self, const char* const property, char* oValue, int valueLen);
int Config_GetTopicTranslation(
  Config* self,
  const char* const inputKey,
  const char* const inputString,
  const char* const outputKey,
  char* outputString,
  int outputLen);

#endif // !CONFIG_H_
