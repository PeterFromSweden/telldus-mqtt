#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"
#include "configjson.h"

static bool loggedVersion;

void ConfigJson_Init(ConfigJson* self)
{
  *self = (ConfigJson){ 0 };
  if( !loggedVersion )
  {
    Log(TM_LOG_DEBUG, "cJson %s", cJSON_Version());
    loggedVersion = true;
  }
}

void ConfigJson_Destroy(ConfigJson* self)
{
  ConfigJson_FreeContent( self );
  cJSON_Delete( self->json );
}

int ConfigJson_LoadContent(ConfigJson* self, char* configFilename )
{
  FILE* f = fopen(configFilename, "rt");
  if ( !f )
  {
    Log(TM_LOG_ERROR, "%s %s", configFilename, strerror(errno));
    return 1;
  }

  fseek(f, 0L, SEEK_END);
  self->contentLen = ftell(f);
  self->contentMaxLen = self->contentLen + 200;
  self->content = malloc(self->contentMaxLen);
  if ( self->content == NULL )
  {
    Log(TM_LOG_ERROR, "Error %s", strerror(errno));
    fclose(f);
    return 1;
  }

  fseek(f, 0L, SEEK_SET);
  size_t readCount = fread(self->content, 1, self->contentLen, f);
  fclose(f);
  if( readCount != self->contentLen )
  {
    Log(TM_LOG_ERROR, "Error, read %i bytes, expected %i", readCount, self->contentLen);
    return 1;
  }
  
  return 0;
}

char* ConfigJson_GetContent(ConfigJson* self)
{
  return self->content;
}

int ConfigJson_ParseContent(ConfigJson* self)
{
  self->json = cJSON_Parse(self->content);
  if ( self->json == NULL )
  {
    const char* error_ptr = cJSON_GetErrorPtr();
    if ( error_ptr != NULL )
    {
      Log(TM_LOG_ERROR, "Error before: %s\n", error_ptr);
    }
    return 1;
  }

  //Log(TM_LOG_DEBUG, "%s", cJSON_Print(self->json));

  return 0;
}

void ConfigJson_FreeContent(ConfigJson* self)
{
  if( self->content != NULL )
  {
    free(self->content);
    self->content = NULL;
    self->contentLen = 0;
    self->contentMaxLen = 0;
  }
}

char* ConfigJson_GetStrPtr(ConfigJson* self, const char* const property)
{
  cJSON* item = cJSON_GetObjectItem(self->json, property);
  if ( cJSON_IsString(item) )
  {
    return item->valuestring;
  }
  return NULL;
}

int ConfigJson_GetInt(ConfigJson* self, const char* const property)
{
  cJSON* item = cJSON_GetObjectItem(self->json, property);
  if ( cJSON_IsNumber(item) )
  {
    return item->valueint;
  }
  return -1;
}

