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
static void on_disconnect(struct mosquitto* mosq, void* obj, int reason_code);
static void on_publish(struct mosquitto* mosq, void* obj, int mid);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);

MqttClient* MqttClient_GetInstance(void)
{
  MqttClient* self = &themqttclient;

  if( created == false )
  {
    created = true;
    *self = (MqttClient) {0};
    
    // Init mosq
    int major, minor, revision;
    mosquitto_lib_init();
    mosquitto_lib_version(&major, &minor, &revision);
    Log(TM_LOG_INFO, "Mosquitto %i.%i.%i", major, minor, revision);

    char* serial = TelldusClient_GetControllerSerial(TelldusClient_GetInstance());
    ASRT( *serial != '\0' );
    self->mosq = mosquitto_new(serial, true, (void*) self);
    if ( self->mosq == NULL ) 
    {
      Log(TM_LOG_ERROR, "Error: Out of memory.");
      exit(1);
    }
    mosquitto_connect_callback_set(self->mosq, on_connect);
    mosquitto_disconnect_callback_set(self->mosq, on_disconnect);
    mosquitto_publish_callback_set(self->mosq, on_publish);
    mosquitto_message_callback_set(self->mosq, on_message);

    self->config = Config_GetInstance();
  }
  return self;
}

void MqttClient_Destroy(MqttClient* self)
{
  MqttClient_Disconnect( self );
  Log(TM_LOG_DEBUG, "Mosquitto destroy.");
  mosquitto_destroy(self->mosq);
  Log(TM_LOG_DEBUG, "Mosquitto cleanup.");
  mosquitto_lib_cleanup();
  Log(TM_LOG_DEBUG, "Mosquitto no more");
}

bool MqttClient_IsConnected(MqttClient *self)
{
  return !(self->mqconn == TM_MQCONN_NONE);
}

int MqttClient_Connect(MqttClient *self)
{
  self->mqconn = TM_MQCONN_START;

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
      Log(TM_LOG_ERROR, "Failed to connect to MQTT broker %s:%i", 
        Config_GetStrPtr(self->config, "host"), 
        Config_GetInt(self->config, "port"));
      exit(1);
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
  self->mutelog = false;
  return 0;
}

void MqttClient_Disconnect(MqttClient *self)
{
  mosquitto_disconnect( self->mosq );
  Log(TM_LOG_DEBUG, "mqtt disconnect");
  self->mqconn = TM_MQCONN_NONE;
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
    Log(TM_LOG_ERROR, "MQTT is not connected => ignore");
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
    (int) strlen(payload),
    payload, 
    0, // qos
    true //retain
  );
  
  ConfigJson_Destroy(&cj);
}

void MqttClient_SensorOnline(MqttClient* self, TelldusSensor* sensor, bool online)
{
  const char *offon[] = { "offline", "online" };
  const char * const status = offon[online];

  mosquitto_publish(
    self->mosq, 
    NULL, 
    sensor->availability, 
    (int) strlen(status), 
    status, 
    0, // qos
    true //retain
  );
}

void MqttClient_SensorValue(MqttClient* self, TelldusSensor* sensor)
{
  mosquitto_publish(
    self->mosq, 
    NULL, 
    sensor->state_topic, 
    (int) strlen(sensor->value), 
    sensor->value, 
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
    (int) strlen(payload), 
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
    (int) strlen(device->value), 
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

/* Callback called when the client receives a CONNACK message from the broker. */
static void on_disconnect(struct mosquitto* mosq, void* obj, int reason_code)
{
  MqttClient* self = (MqttClient*) obj;
  self->mqconn = TM_MQCONN_NONE;
  Log(TM_LOG_DEBUG, "on_disconnect");
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

