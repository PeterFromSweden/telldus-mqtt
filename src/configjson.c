#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"
#include "configjson.h"

typedef enum {
  MARKER_SET,
  MARKER_CLEAR,
  MARKER_CHECK
} TMarkerOp;

static bool loggedVersion;

static void contentMarker(ConfigJson* self, TMarkerOp op);

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
  memset(self->content, 0xdd, self->contentMaxLen);
  contentMarker(self, MARKER_SET);
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
  self->content[self->contentLen] = '\0';
  contentMarker(self, MARKER_CHECK);
  
  return 0;
}

char* ConfigJson_GetContent(ConfigJson* self)
{
  contentMarker(self, MARKER_CHECK);
  return self->content;
}

long ConfigJson_GetContentLen(ConfigJson* self)
{
  contentMarker(self, MARKER_CHECK);
  return self->contentLen;
}

long ConfigJson_GetContentMaxLen(ConfigJson* self)
{
  contentMarker(self, MARKER_CHECK);
  return self->contentMaxLen;
}

int ConfigJson_ParseContent(ConfigJson* self)
{
  contentMarker(self, MARKER_CHECK);
  
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
  contentMarker(self, MARKER_CHECK);

  if( self->content != NULL )
  {
    contentMarker(self, MARKER_CLEAR);
    free(self->content);
    self->content = NULL;
    self->contentLen = 0;
    self->contentMaxLen = 0;
  }
}

char* ConfigJson_GetSubStrPtr(
  ConfigJson* self, 
  const char* const property,
  const char* const subproperty )
{
  cJSON* item = cJSON_GetObjectItem(self->json, property);
  if ( cJSON_IsObject(item) )
  {
    cJSON* item2 = cJSON_GetObjectItem( item, subproperty );
    if( cJSON_IsString( item2 ) )
    {
      return item2->valuestring;;
    }
  }
  return NULL;
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

static void contentMarker(ConfigJson* self, TMarkerOp op)
{
  if( self->content != NULL )
  {
    switch( op )
    {
      case MARKER_SET:
        self->content[self->contentMaxLen-1] = 0x03; // ETX
        break;

      case MARKER_CLEAR:
        self->content[self->contentMaxLen-1] = 0;
        break;

      case MARKER_CHECK:
        if( self->content[self->contentMaxLen-1] != 0x03 )
        {
          Log(TM_LOG_ERROR, "Buffer overrun!");
        }
        break;
      
      default:
        Log(TM_LOG_ERROR, "contentMarker!");
        break;
    }
  }
}