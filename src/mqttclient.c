#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "asrt.h"
#include "telldusclient.h"
#include "config.h"
#include "configjson.h"
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

void replaceWords(char *buffer, const char *find, const char *replace) {
    char *pos = buffer;
    int findLen = strlen(find);
    int replaceLen = strlen(replace);

    while ((pos = strstr(pos, find)) != NULL) {
        memmove(pos + replaceLen, pos + findLen, strlen(pos + findLen) + 1);
        memcpy(pos, replace, replaceLen);
        pos += replaceLen;
    }
}

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
  ConfigJson_LoadContent(&cj, "telldus-sensor.json");
  json_keywordexpansion(sensor, sernoPtr, ConfigJson_GetContent(&cj));
  // Check for errors.  
  ConfigJson_ParseContent(&cj);
  
  ConfigJson cjTopic;
  ConfigJson_Init(&cjTopic);
  ConfigJson_LoadContent(&cjTopic, "homeassistant-sensor.json");
  json_keywordexpansion(sensor, sernoPtr, ConfigJson_GetContent(&cjTopic));
  // Check for errors.  
  ConfigJson_ParseContent(&cjTopic);
  
  char* payload = ConfigJson_GetContent(&cj);
  //Log(TM_LOG_DEBUG, "%s", ConfigJson_GetStrPtr(&cjTopic, "topic"));
  strcpy(sensor->state_topic, ConfigJson_GetStrPtr(&cj, "state_topic"));
  strcpy(sensor->availability,  ConfigJson_GetSubStrPtr(&cj, "availability", "topic"));

  mosquitto_publish(
    self->mosq, 
    NULL, 
    ConfigJson_GetStrPtr(&cjTopic, "topic"), 
    strlen(payload), 
    payload, 
    0, // qos
    true //retain
  );
  
  ConfigJson_Destroy(&cj);
  ConfigJson_Destroy(&cjTopic);
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

static void json_devicekeywordexpansion(TelldusDevice* device, char* sernoPtr, char* buf)
{
  const char* wordsToReplace[] = {
    "{device_no}", "{serno}", ""
  };
  char* replacement[] = {
    TelldusDevice_ItemToString(device, TM_DEVICE_CONTENT_DEVICENO),
    sernoPtr
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
  ConfigJson_LoadContent(&cj, "telldus-device.json");
  json_devicekeywordexpansion(device, sernoPtr, ConfigJson_GetContent(&cj));
  // Check for errors.  
  ConfigJson_ParseContent(&cj);
  
  ConfigJson cjTopic;
  ConfigJson_Init(&cjTopic);
  ConfigJson_LoadContent(&cjTopic, "homeassistant-device.json");
  json_devicekeywordexpansion(device, sernoPtr, ConfigJson_GetContent(&cjTopic));
  // Check for errors.  
  ConfigJson_ParseContent(&cjTopic);
  
  char* payload = ConfigJson_GetContent(&cj);
  //Log(TM_LOG_DEBUG, "%s", ConfigJson_GetStrPtr(&cjTopic, "topic"));
  strcpy(device->state_topic, ConfigJson_GetStrPtr(&cj, "state_topic"));
  strcpy(device->command_topic, ConfigJson_GetStrPtr(&cj, "command_topic"));

  mosquitto_publish(
    self->mosq, 
    NULL, 
    ConfigJson_GetStrPtr(&cjTopic, "topic"), 
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
  ConfigJson_Destroy(&cjTopic);
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

  /*
  char telldusTopic[70];
  if ( Config_GetTopicTranslation(&config, "mqtt", msg->topic, "telldus", telldusTopic, sizeof(telldusTopic)) )
  {
    // No match
    strncpy(telldusTopic, msg->topic, sizeof(telldusTopic));
  }
  
  int rc;
  if ( msg->payloadlen == 2 )
  {
    // on
    //rc = tdTurnOn(1);
  }
  else
  {
    // on
    //rc = tdTurnOff(1);
  }
  // if ( rc != TELLSTICK_SUCCESS )
  // {
  //   fprintf(stderr, "Turn on/off %s\r\n", tdGetErrorString(rc));
  // }
  */
}

