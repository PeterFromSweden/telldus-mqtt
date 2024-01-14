#include <stdlib.h>
#include "log.h"
#include "telldusclient.h"
#include "config.h"
#include "mqttclient.h"

static void on_connect(struct mosquitto* mosq, void* obj, int reason_code);
static void on_publish(struct mosquitto* mosq, void* obj, int mid);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);

void MqttClient_Init(MqttClient* self)
{
  *self = (MqttClient) {0};
  
  // Init mosq
  int major, minor, revision;
  mosquitto_lib_init();
  mosquitto_lib_version(&major, &minor, &revision);
  Log(TM_LOG_INFO, "Mosquitto %i.%i.%i", major, minor, revision);

  self->mosq = mosquitto_new(NULL /*serial*/, true, (void*) self);
  if ( self->mosq == NULL ) 
  {
    Log(TM_LOG_ERROR, "Error: Out of memory.");
    exit(1);
  }
  mosquitto_connect_callback_set(self->mosq, on_connect);
  mosquitto_publish_callback_set(self->mosq, on_publish);
  mosquitto_message_callback_set(self->mosq, on_message);

  self->config = Config_GetInstance();
}

void MqttClient_Destroy(MqttClient* self)
{
  mosquitto_destroy(self->mosq);
  mosquitto_lib_cleanup();
}

bool MqttClient_IsConnected(MqttClient *self)
{
  return self->mqconn != TM_MQCONN_NONE;
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

void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor)
{

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
  }

  /* You may wish to set a flag here to indicate to your application that the
   * client is now connected. */
  self->mqconn = TM_MQCONN_OK;
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
  Log(TM_LOG_DEBUG, "NYI: mqtt message %s %s (%d)", msg->topic, (const char*)msg->payload, msg->payloadlen);

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

