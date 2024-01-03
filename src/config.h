#ifndef CONFIG_H_
#define CONFIG_H_

#include <cjson/cJSON.h>

typedef struct {
  cJSON* configJson;
} Config;

void Config_Init(Config* self);
int Config_Load(Config* self, char* configFilename);
void Config_GetStr(Config* self, const char* const property, char* oValue, int valueLen);
void Config_GetInt(Config* self, const char* const property, int* oValue);
int Config_GetTopicTranslation(Config* self, const char* const telldusstring, char* oValue, int valueLen);

#endif // !CONFIG_H_
