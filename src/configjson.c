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

void ConfigJson_FreeJson(ConfigJson* self)
{
  cJSON_Delete( self->json );
}

void ConfigJson_Destroy(ConfigJson* self)
{
  ConfigJson_FreeContent( self );
  ConfigJson_FreeJson( self );
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

char* ConfigJson_CopyToContent(ConfigJson* self, char* fromString)
{
  contentMarker(self, MARKER_CHECK);
  int fsl = strlen(fromString );
  if( fsl > self->contentMaxLen - 3 )
  {
    Log(TM_LOG_DEBUG, "CopyToContent overflow");
  }
  Log(TM_LOG_DEBUG, "self->content:\n%s", self->content);
  Log(TM_LOG_DEBUG, "fromString\n%s", fromString);
  strncpy(self->content, fromString, self->contentMaxLen);
  Log(TM_LOG_DEBUG, "self->content:\n%s", self->content);
  self->content[ self->contentMaxLen - 2 ] = '\0';
  contentMarker(self, MARKER_CHECK);
  return self->content;
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

static cJSON* getPropList(cJSON* item, const char* const propertyList[])
{
  if( item == NULL )
  {
    return NULL;
  }
  
  if( *propertyList[0] == '\0' )
  {
    return item;
  }

  return getPropList(cJSON_GetObjectItem(item, propertyList[0]), &propertyList[1]);
}

char* ConfigJson_GetStringFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] )
{
  contentMarker(self, MARKER_CHECK);
  cJSON* item = getPropList(self->json, propertyList);
  if( item == NULL )
  {
    Log(TM_LOG_ERROR, "Json property not found!");
    return NULL;
  }

  if( !cJSON_IsString( item ) )
  {
    Log(TM_LOG_ERROR, "Json property is not a string!");
  }

  return item->valuestring;
}

char* ConfigJson_GetStringFromProp(ConfigJson* self, const char* const property)
{
  return ConfigJson_GetStringFromPropList(self, 
            (const char * const []) {property, ""});
}

bool ConfigJson_SetStringFromPropList(
  ConfigJson* self, 
  const char* const propertyList[],
  const char* propertyToChange,
  const char* str )
{
  contentMarker(self, MARKER_CHECK);
  cJSON* item = getPropList(self->json, propertyList);
  if( item == NULL )
  {
    Log(TM_LOG_ERROR, "Json property not found!");
    return false;
  }

  cJSON* oldItem = cJSON_GetObjectItem(item, propertyToChange);
  if( !cJSON_IsString( oldItem ) )
  {
    Log(TM_LOG_ERROR, "Json property is not a string!");
    return false;
  }

  cJSON* newItem = cJSON_CreateString(str);
  cJSON_ReplaceItemInObject(item, propertyToChange, newItem);

  contentMarker(self, MARKER_CHECK);
  return true;
}

char* ConfigJson_GetJsonFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] )
{
  contentMarker(self, MARKER_CHECK);
  cJSON* item = getPropList(self->json, propertyList);
  if( item == NULL )
  {
    Log(TM_LOG_ERROR, "Json property not found!");
    return NULL;
  }

  return cJSON_Print(item);
}

char* ConfigJson_GetJsonFromProp(ConfigJson* self, const char* const property)
{
  return ConfigJson_GetJsonFromPropList(self, 
            (const char * const []) {property, ""});
}

int ConfigJson_GetIntFromPropList(
  ConfigJson* self, 
  const char* const propertyList[] )
{
  contentMarker(self, MARKER_CHECK);
  cJSON* item = getPropList(self->json, propertyList);
  if( item == NULL )
  {
    Log(TM_LOG_ERROR, "Json property not found!");
    return 0;
  }

  if( !cJSON_IsNumber( item ) )
  {
    Log(TM_LOG_ERROR, "Json property is not a number!");
  }

  return item->valueint;
}

int ConfigJson_GetIntFromProp(ConfigJson* self, const char* const property)
{
  return ConfigJson_GetIntFromPropList(self, 
            (const char * const []) {property, ""});
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
        Log(TM_LOG_ERROR, "contentMarker illegal operation!");
        break;
    }
  }
}