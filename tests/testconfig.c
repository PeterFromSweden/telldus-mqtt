#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "stringutils.h"
#include "config.h"
#include "asrt.h"
#include "log.h"

Config* config;

int testTranslation(void)
{
  int res = 0;

  char iostr[] = "telldus/A703AKOX/sensor/fineoffset_temperaturehumidity_183";
  char* name;
  name = Config_GetTopicTranslation(config, iostr);
  res &= (name == NULL);
  //puts(iostr);
  //puts(name);

  return res;

}
int main(void)
{
  config = Config_GetInstance();
  int res = 0;
  res &= Config_Load(config, "telldus-mqtt.json");
  res &= testTranslation();
  return res;
}