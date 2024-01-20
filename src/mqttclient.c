#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "asrt.h"
#include "telldusclient.h"
#include "config.h"
#include "configjson.h"
#include "stringutils.h"
#include "mqttclient.h"

static MqttClient themqttclient;
static bool created;

static void on_connect(struct mosquitto* mosq, void* obj, int reason_code);
static void on_publish(struct mosquitto* mosq, void* obj, int mid);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);

MqttClient* MqttClient_GetInstance(void)
{
  MqttClient* self = &themqttclient;

  if( created == false )
  {
    *self = (MqttClient) {0};
    
    // Init mosq
    int major, minor, revision;
    mosquitto_lib_init();
    mosquitto_lib_version(&major, &minor, &revision);
    Log(TM_LOG_INFO, "Mosquitto %i.%i.%i", major, minor, revision);

    char* serial = TelldusClient_GetControllerSerial(TelldusClient_GetInstance());
    self->mosq = mosquitto_new(serial, true, (void*) self);
    if ( self->mosq == NULL ) 
    {
      Log(TM_LOG_ERROR, "Error: Out of memory.");
      exit(1);
    }
    mosquitto_connect_callback_set(self->mosq, on_connect);
    mosquitto_publish_callback_set(self->mosq, on_publish);
    mosquitto_message_callback_set(self->mosq, on_message);

    self->config = Config_GetInstance();
    created = true;
  }
  return self;
}

void MqttClient_Destroy(MqttClient* self)
{
  mosquitto_destroy(self->mosq);
  mosquitto_lib_cleanup();
}

TMqConn MqttClient_GetConnection(MqttClient *self)
{
  return self->mqconn;
}

int MqttClient_Connect(MqttClient *self)
{
  int rc = mosquitto_username_pw_set(
    self->mosq,
    Config_GetStrPtr(self->config, "user"),
    Config_GetStrPtr(self->config, "pass"));
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(self->mosq);
    Log(TM_LOG_ERROR, "Error: %s", mosquitto_strerror(rc));
    return 1;
  }
 
  rc = mosquitto_connect(
    self->mosq, 
    Config_GetStrPtr(self->config, "host"), 
    Config_GetInt(self->config, "port"), 
    60);
  if ( rc == MOSQ_ERR_ERRNO )
  {
    if( !self->mutelog )
    {
      Log(TM_LOG_WARNING, "Is %s:%i a MQTT broker?", 
        Config_GetStrPtr(self->config, "host"), 
        Config_GetInt(self->config, "port"));
      self->mutelog = true;
    }
    return 1;
  }
  if ( rc != MOSQ_ERR_SUCCESS ) 
  {
    mosquitto_destroy(self->mosq);
    if( !self->mutelog )
    {
      Log(TM_LOG_ERROR, "Error: %s", mosquitto_strerror(rc));
      self->mutelog = true;
    }
    return 1;
  }
  
  rc = mosquitto_loop_start(self->mosq);
  if ( rc != MOSQ_ERR_SUCCESS ) 
  {
    mosquitto_destroy(self->mosq);
    Log(TM_LOG_ERROR, "Error: %s", mosquitto_strerror(rc));
    return 1;
  }
  self->mqconn = TM_MQCONN_START;
  self->mutelog = false;
}

void MqttClient_Disconnect(MqttClient *self)
{
  Log(TM_LOG_DEBUG, "mqtt disconnect");
  self->mqconn = TM_MQCONN_NONE;
}

#if 0
static void json_keywordexpansion(TelldusSensor* sensor, char* sernoPtr, char* buf)
{
  const char* wordsToReplace[] = {
    "{datatype}", "{unit}", "{serno}", "{protocol}", "{model}", "{id}", ""
  };
  char* replacement[] = {
    TelldusSensor_ItemToString(sensor, TM_SENSOR_CONTENT_DATATYPE),
    TelldusSensor_ItemToString(sensor, TM_SENSOR_CONTENT_UNIT),
    sernoPtr,
    TelldusSensor_ItemToString(sensor, TM_SENSOR_CONTENT_PROTOCOL),
    TelldusSensor_ItemToString(sensor, TM_SENSOR_CONTENT_MODEL),
    TelldusSensor_ItemToString(sensor, TM_SENSOR_CONTENT_ID)
  };
  int i = 0;
  //Log(TM_LOG_DEBUG, "json pre len=%i", strlen(buf));
  while( *wordsToReplace[i] )
  {
    replaceWords(buf, wordsToReplace[i], replacement[i]);
    int buflen = strlen(buf);
    if( buf[buflen-1] != '}' )
    {
      Log(TM_LOG_ERROR, "json not ending with }");
    }
    i++;
  }
  //Log(TM_LOG_DEBUG, "json post len=%i", strlen(buf));
}
#endif

void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor)
{
  char str[40];
  char* sernoPtr = TelldusClient_GetControllerSerial(TelldusClient_GetInstance());

  Log(TM_LOG_DEBUG, "AddSensor %s %s", 
    sernoPtr,
    TelldusSensor_ToString(sensor, str, sizeof(str))
  );

  if( self->mqconn != TM_MQCONN_OK )
  {
    Log(TM_LOG_ERROR, "Not MQTT is not connected...");
    return;
  }

  ConfigJson cj;
  ConfigJson_Init(&cj);
  ASRT( !ConfigJson_LoadContent(&cj, "telldus-mqtt-homeassistant.json") );
  ReplaceWordList(ConfigJson_GetContent(&cj), 
    (const char * const []) {
      "{datatype}", "{unit}", "{serno}", "{protocol}", "{model}", "{id}", ""
      },
    (const char * const []) {
      sensor->dataType,
      sensor->unit,
      sernoPtr,
      sensor->protocol,
      sensor->model,
      sensor->id,
      ""  
      });

  
  // Translate default generated topics with user defined.
  char* name = Config_GetTopicTranslation(self->config, ConfigJson_GetContent(&cj) );

  if( name == NULL )
  {
    // No match
    ConfigJson_ParseContent(&cj);
  }
  else
  {
    ConfigJson_ParseContent(&cj);
    ConfigJson_SetStringFromPropList(&cj, 
      (const char * const []) {"sensor-config-content", "device", ""},
    "name", 
    name);
  }

  //Log(TM_LOG_DEBUG, "%s", ConfigJson_GetStrPtr(&cjTopic, "topic"));
  strcpy(sensor->state_topic, ConfigJson_GetStringFromPropList(&cj, 
    (const char * const []) {"sensor-config-content", "state_topic", ""}));
  strcpy(sensor->availability,  ConfigJson_GetStringFromPropList(&cj, 
    (const char * const []) {"sensor-config-content", "availability", "topic", ""}));

  char* topic = ConfigJson_GetStringFromPropList(&cj, 
      (const char * const []) {"sensor-config", "topic", ""});
  char* payload = ConfigJson_GetJsonFromProp(&cj, "sensor-config-content");
  mosquitto_publish(
    self->mosq, 
    NULL, 
    topic, 
    strlen(payload),
    payload, 
    0, // qos
    true //retain
  );
  
  ConfigJson_Destroy(&cj);
}

void MqttClient_SensorValue(MqttClient* self, TelldusSensor* sensor)
{
  mosquitto_publish(
    self->mosq, 
    NULL, 
    sensor->state_topic, 
    strlen(sensor->value), 
    sensor->value, 
    0, // qos
    false //retain
  );

  mosquitto_publish(
    self->mosq, 
    NULL, 
    sensor->availability, 
    strlen("online"), 
    "online", 
    0, // qos
    false //retain
  );
}

void MqttClient_AddDevice(MqttClient* self, TelldusDevice* device)
{
  char str[40];
  char* sernoPtr = TelldusClient_GetControllerSerial(TelldusClient_GetInstance());

  Log(TM_LOG_DEBUG, "AddDevice %s %s", 
    sernoPtr,
    TelldusDevice_ToString(device, str, sizeof(str))
  );

  if( self->mqconn != TM_MQCONN_OK )
  {
    Log(TM_LOG_ERROR, "Not MQTT is not connected...");
    return;
  }

  ConfigJson cj;
  ConfigJson_Init(&cj);
  ASRT( !ConfigJson_LoadContent(&cj, "telldus-mqtt-homeassistant.json") );
  ReplaceWordList(ConfigJson_GetContent(&cj), 
    (const char * const []) {
      "{device_no}", "{serno}", ""
      },
    (const char * const []) {
      TelldusDevice_ItemToString(device, TM_DEVICE_CONTENT_DEVICENO),
      sernoPtr,
      ""  
      });

  // Translate default generated topics with user defined.
  char* name = Config_GetTopicTranslation(self->config, ConfigJson_GetContent(&cj) );

  if( name == NULL )
  {
    // No match
    ConfigJson_ParseContent(&cj);
  }
  else
  {
    ConfigJson_ParseContent(&cj);
    ConfigJson_SetStringFromPropList(&cj, 
      (const char * const []) {"device-config-content", "device", ""},
    "name", 
    name);
  }
  
  //Log(TM_LOG_DEBUG, "%s", ConfigJson_GetStrPtr(&cjTopic, "topic"));
  strcpy(device->state_topic, ConfigJson_GetStringFromPropList(&cj, 
          (const char * const []) {"device-config-content", "state_topic", ""}));
  strcpy(device->command_topic, ConfigJson_GetStringFromPropList(&cj, 
          (const char * const []) {"device-config-content", "command_topic", ""}));

  char* topic = ConfigJson_GetStringFromPropList(&cj, 
      (const char * const []) {"device-config", "topic", ""});
  char* payload = ConfigJson_GetJsonFromProp(&cj, "device-config-content");
  mosquitto_publish(
    self->mosq, 
    NULL,
    topic,
    strlen(payload), 
    payload, 
    0, // qos
    true //retain
  );
  
  mosquitto_subscribe(
    self->mosq,
    NULL,
    device->command_topic,
    0 // qos
  );

  ConfigJson_Destroy(&cj);
}

void MqttClient_DeviceValue(MqttClient* self, TelldusDevice* device)
{
  mosquitto_publish(
    self->mosq, 
    NULL, 
    device->state_topic, 
    strlen(device->value), 
    device->value, 
    0, // qos
    true //retain
  );
}

/* Callback called when the client receives a CONNACK message from the broker. */
static void on_connect(struct mosquitto* mosq, void* obj, int reason_code)
{
  MqttClient* self = (MqttClient*) obj;
  /* Print out the connection result. mosquitto_connack_string() produces an
   * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
   * clients is mosquitto_reason_string().
   */
  Log(TM_LOG_DEBUG, "on_connect: %s", mosquitto_connack_string(reason_code));
  if ( reason_code != 0 ) {
    /* If the connection fails for any reason, we don't want to keep on
     * retrying in this example, so disconnect. Without this, the client
     * will attempt to reconnect. */
    MqttClient_Disconnect(self);
    return;
  }

  /* You may wish to set a flag here to indicate to your application that the
   * client is now connected. */
  self->mqconn = TM_MQCONN_OK;

  TelldusClient* telldusclient = TelldusClient_GetInstance();
  int deviceNo = TelldusClient_GetDeviceNo(telldusclient, TM_DEVICE_GET_FIRST);
  while( deviceNo >= 0 )
  {
    TelldusDevice* device = TelldusDevice_Create(deviceNo);
    MqttClient_AddDevice(self, device);
    deviceNo = TelldusClient_GetDeviceNo(telldusclient, TM_DEVICE_GET_NEXT);
  }
}


/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
static void on_publish(struct mosquitto* mosq, void* obj, int mid)
{
  MqttClient* self = (MqttClient*) obj;
  Log(TM_LOG_DEBUG, "Message with mid %d has been published.", mid);
}

// Callback
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
  MqttClient* self = (MqttClient*) obj;
  Log(TM_LOG_DEBUG, "mqtt message %s %s (%d)", msg->topic, (const char*)msg->payload, msg->payloadlen);

  TelldusClient* telldusclient = TelldusClient_GetInstance();
  if( !TelldusClient_IsConnected(telldusclient) )
  {
    Log(TM_LOG_WARNING, "mqttclient on_message, not connected to telldus => ignore");
    return;
  }

  TelldusDevice* device = TelldusDevice_GetTopic(msg->topic);
  if( device == NULL )
  {
    Log(TM_LOG_ERROR, "mqttclient on_message, device new? => ignore");
    return;
  }

  TelldusDevice_Action(device, msg->payload);
}

