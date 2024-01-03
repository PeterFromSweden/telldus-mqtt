#ifndef CONFIG_H_
#define CONFIG_H_

typedef struct {
  int dummy;
} Config;

void Config_Init(Config* self);
int Config_Load(Config* self, char* configFilename);

#endif // !CONFIG_H_
