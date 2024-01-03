#include <stdio.h>
#include <cjson/cJSON.h>
#include "config.h"

void Config_Init(Config* self)
{
  *self = (Config){ 0 };
}

int Config_Load(Config* self, char* configFilename )
{
  FILE* f = fopen(configFilename, "rt");
  if ( !f )
  {
    f = fopen(configFilename, "w");
    perror(configFilename);
    return 1;
  }

  char content[2048];
  size_t readCount = fread(content, 1, sizeof(content), f);
  fclose(f);
  if ( readCount >= sizeof(content) )
  {
    fprintf("%s is to big.\r\n", configFilename);
    return 1;
  }

  cJSON* configJson = cJSON_Parse(content);
  if ( configJson == NULL )
  {
    const char* error_ptr = cJSON_GetErrorPtr();
    if ( error_ptr != NULL )
    {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    
    return 1;
  }

  cJSON* item = cJSON_GetObjectItem(configJson, "name");
  if ( cJSON_IsString(item) && (item->valuestring != NULL) )
  {
    printf("Checking %s\n", item->valuestring);
  }


}