#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "configjson.h"
#include "stringutils.h"
#include "asrt.h"

// This file was made with visual studio code and character encoding ISO 8859-1
// Default is UTF-8 which will expand non-ASCII to two bytes characters.

const char ref[] = "{\n\
\t\"device_class\":\t\"temperature\",\n\
\t\"unit_of_measurement\":\t\"°\",\n\
\t\"state_topic\":\t\"telldus/serial/sensor/some_protocol_some_model_999/temperature\",\n\
\t\"unique_id\":\t\"telldus/serial/sensor/some_protocol_some_model_999/temperature\",\n\
\t\"device\":\t{\n\
\t\t\"manufacturer\":\t\"Telldus\",\n\
\t\t\"identifiers\":\t[\"serial-some_protocol-some_model-999-temperature\"],\n\
\t\t\"name\":\t\"serial-some_protocol-some_model-999-temperature\"\n\
\t},\n\
\t\"availability\":\t{\n\
\t\t\"topic\":\t\"telldus/serial/sensor/some_protocol_some_model_999/status\"\n\
\t},\n\
\t\"origin\":\t{\n\
\t\t\"name\":\t\"telldus-mqtt\",\n\
\t\t\"sw_version\":\t\"0.1.3\"\n\
\t}\n\
}";

void PrintComparison(const char* payload, const char* ref);

int buffer_replace(void)
{
  ConfigJson cj;
  ConfigJson_Init(&cj);
  ASRT( !ConfigJson_LoadContent(&cj, "telldus-mqtt-homeassistant.json") );
  cJSON* cjson = cJSON_Parse(ConfigJson_GetContent(&cj));
  strcpy( ConfigJson_GetContent(&cj), cJSON_Print(cjson));
  ReplaceWordList( ConfigJson_GetContent(&cj),
    (const char * const []) {
      "{serno}", "{device_no}", "{protocol}", "{model}", "{id}", "{datatype}", "{unit}", ""
      },
    (const char * const []) {
      "serial", "device", "some_protocol", "some_model", "999", "temperature", "°"
      });
  
  // Check for errors.  
  ConfigJson_ParseContent(&cj);
  
  char* payload = ConfigJson_GetJsonFromProp(&cj, "sensor-config-content");
  
  // puts(payload);
  // PrintComparison(payload, ref);

  int res = strcmp(payload, ref);

  // printf("res = %d\n", res);

  cJSON_Delete(cjson);
  ConfigJson_Destroy(&cj);

  return res;
}

int bufferAndProperty_replace(void)
{
  ConfigJson cj;
  ConfigJson_Init(&cj);
  ASRT( !ConfigJson_LoadContent(&cj, "telldus-mqtt-homeassistant.json") );
  ReplaceWordList( ConfigJson_GetContent(&cj),
    (const char * const []) {
      "{serno}", "{device_no}", "{protocol}", "{model}", "{id}", "{datatype}", "{unit}", ""
      },
    (const char * const []) {
      "serial", "device", "some_protocol", "some_model", "999", "temperature", "°"
      });
  
  // Check for errors.  
  ConfigJson_ParseContent(&cj);
  
  // Zoom in to one property
  char* payload = ConfigJson_GetJsonFromProp(&cj, "sensor-config-content");
  strcpy( ConfigJson_GetContent(&cj), payload);
  ConfigJson_FreeJson(&cj);
  ConfigJson_ParseContent(&cj);
  
  // puts(ConfigJson_GetContent(&cj));
  
  char* name = ConfigJson_GetStringFromPropList( &cj, 
    (const char * const []) {"device", "name", "" } );

  // printf("name = %s\n", name);
  //cJSON* cjson = cJSON_Parse(ConfigJson_GetContent(&cj));
  //strcpy( ConfigJson_GetContent(&cj), cJSON_Print(cjson));
  
  
  //cJSON_Delete(cjson);
  
  const char ref[] = "serial-some_protocol-some_model-999-temperature";
  int res = strcmp( name, ref );
  // PrintComparison( name, ref );
  // printf("res = %d\n", res);
  if( res ) goto errorexit;

  cJSON* item = cj.json;
  cJSON* prop1 = cJSON_GetObjectItem(item, "device");
  cJSON* prop2 = cJSON_GetObjectItem(prop1, "name");
  // printf("name = %s\n", prop2->valuestring);
  res = strcmp( prop2->valuestring, ref );
  // printf("res = %d\n", res);
  if( res ) goto errorexit;

  char newName[] = "Gött namn";
  cJSON* newProp2 = cJSON_CreateString(newName);
  cJSON_ReplaceItemInObject(prop1, "name", newProp2);
  prop2 = cJSON_GetObjectItem(prop1, "name");
  // printf("name = %s\n", prop2->valuestring);
  res = strcmp( prop2->valuestring, newName );
  // printf("res = %d\n", res);
  // puts(cJSON_Print(item));

errorexit:  
  ConfigJson_Destroy(&cj);

  return res;
}

int GetSet(void)
{
  ConfigJson cj;
  ConfigJson_Init(&cj);
  ASRT( !ConfigJson_LoadContent(&cj, "telldus-mqtt-homeassistant.json") );
  ConfigJson_ParseContent(&cj);
  
  char *str;
  int res;

  str = ConfigJson_GetStringFromPropList(&cj,
    (const char * const []) {""}
  );
  res = str != NULL;
  if( res ) goto errorexit;

  str = ConfigJson_GetStringFromPropList(&cj,
    (const char * const []) {"sensor-config-content", ""}
  );
  res = str != NULL;
  if( res ) goto errorexit;

  str = ConfigJson_GetStringFromPropList(&cj,
    (const char * const []) {"sensor-config-content", "availability", "topic", ""}
  );
  res = strcmp( str, "telldus/{serno}/sensor/{protocol}_{model}_{id}/status" );
  // PrintComparison(str, "telldus/{serno}/sensor/{protocol}_{model}_{id}/status" );
  if( res ) goto errorexit;

  str = ConfigJson_GetStringFromPropList(&cj,
    (const char * const []) {"sensor-config-content", "availability", "toppic", ""}
  );
  res = str != NULL;
  if( res ) goto errorexit;

  str = ConfigJson_GetStringFromProp(&cj, "sensor-config-content");
  res = str != NULL;
  if( res ) goto errorexit;

  res = !ConfigJson_SetStringFromPropList(&cj, 
    (const char * const []) {"sensor-config-content", "device", ""},
    "name", 
    "Gött-namn");
  if( res ) goto errorexit;

errorexit:  
  ConfigJson_Destroy(&cj);

  return res;
}

int main(void)
{
  int res = 0;
  res &= buffer_replace();
  res &= bufferAndProperty_replace();
  res &= GetSet();
  return res;
}

void PrintComparison(const char* payload, const char* ref)
{
  for(int i = 0; i < strlen(payload) && i < strlen(ref); i++)
  {
    if( isprint(payload[i]) )
    {
      printf("%c  ", payload[i]);
    }
    else
    {
      printf("%02X ", payload[i]);
    }
    printf(" : ");
    if( isprint(ref[i]) )
    {
      printf("%c  ", ref[i]);
    }
    else
    {
      printf("%02X ", ref[i]);
    }
    printf("\n");
    if( payload[i] != ref[i] )
      break;
  }
}
