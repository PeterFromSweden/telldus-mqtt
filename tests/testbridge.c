// End-to-end test: TellStick simulator <-> telldus-mqtt <-> real mosquitto broker.
// Usage: testbridge <path-to-mosquitto-broker>
// Returns 77 (skipped) if the broker cannot be started.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>
#include <telldus-core.h>
#include "telldussim/telldussim.h"
#include "telldusclient.h"
#include "mqttclient.h"
#include "config.h"
#include "status.h"
#include "mythread.h"
#include "log.h"

#define SKIP 77
#define SERIAL "SIMSERIAL"
#define CHECK(c) if(!(c)) { printf("FAIL %s:%i: %s\n", __FILE__, __LINE__, #c); res = 1; goto cleanup; }

// ---- Observer: a second MQTT client recording everything on the broker ----

#define MAX_MESSAGES 200

typedef struct {
  char topic[200];
  char payload[1024];
  bool retain;
} Message;

static pthread_mutex_t msgLock = PTHREAD_MUTEX_INITIALIZER;
static Message messages[MAX_MESSAGES];
static int messageCount;
static volatile bool observerConnected;

static void observerOnConnect(struct mosquitto* mosq, void* obj, int rc)
{
  if( rc == 0 )
  {
    mosquitto_subscribe(mosq, NULL, "#", 0);
  }
}

static void observerOnSubscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos)
{
  observerConnected = true;
}

static void observerOnMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
  pthread_mutex_lock(&msgLock);
  if( messageCount < MAX_MESSAGES )
  {
    Message* m = &messages[messageCount++];
    snprintf(m->topic, sizeof(m->topic), "%s", msg->topic);
    snprintf(m->payload, sizeof(m->payload), "%.*s", msg->payloadlen, msg->payload ? (char*) msg->payload : "");
    m->retain = msg->retain;
  }
  pthread_mutex_unlock(&msgLock);
}

// Wait for a message on topic whose payload contains needle (NULL = any payload)
static bool waitFor(const char* topic, const char* needle)
{
  for( int t = 0; t < 50; t++ )
  {
    pthread_mutex_lock(&msgLock);
    for( int i = 0; i < messageCount; i++ )
    {
      if( strcmp(messages[i].topic, topic) == 0 &&
          (needle == NULL || strstr(messages[i].payload, needle) != NULL) )
      {
        pthread_mutex_unlock(&msgLock);
        return true;
      }
    }
    pthread_mutex_unlock(&msgLock);
    MyThread_Sleep(100);
  }
  printf("Timeout waiting for %s %s\n", topic, needle ? needle : "");
  return false;
}

// Wait for a message with empty payload on topic, i.e. a cleared retained topic
static bool waitForCleared(const char* topic)
{
  for( int t = 0; t < 50; t++ )
  {
    pthread_mutex_lock(&msgLock);
    for( int i = 0; i < messageCount; i++ )
    {
      if( strcmp(messages[i].topic, topic) == 0 && messages[i].payload[0] == '\0' )
      {
        pthread_mutex_unlock(&msgLock);
        return true;
      }
    }
    pthread_mutex_unlock(&msgLock);
    MyThread_Sleep(100);
  }
  printf("Timeout waiting for %s to be cleared\n", topic);
  return false;
}

static bool wasCleared(const char* topic)
{
  bool cleared = false;
  pthread_mutex_lock(&msgLock);
  for( int i = 0; i < messageCount; i++ )
  {
    if( strcmp(messages[i].topic, topic) == 0 && messages[i].payload[0] == '\0' )
    {
      cleared = true;
    }
  }
  pthread_mutex_unlock(&msgLock);
  return cleared;
}

static void dumpMessages(void)
{
  pthread_mutex_lock(&msgLock);
  for( int i = 0; i < messageCount; i++ )
  {
    printf("  %s%s: %.80s\n", messages[i].topic, messages[i].retain ? " (retained)" : "", messages[i].payload);
  }
  pthread_mutex_unlock(&msgLock);
}

// ---- Status file ----

static cJSON* readStatus(const char* path)
{
  char text[16384] = "";
  FILE* f = fopen(path, "rb");
  if( f == NULL )
  {
    return NULL;
  }
  fread(text, 1, sizeof(text) - 1, f);
  fclose(f);
  return cJSON_Parse(text);
}

static cJSON* findDevice(cJSON* status, const char* type, const char* id)
{
  cJSON* device;
  cJSON_ArrayForEach(device, cJSON_GetObjectItem(status, "devices"))
  {
    const char* t = cJSON_GetStringValue(cJSON_GetObjectItem(device, "type"));
    const char* i = cJSON_GetStringValue(cJSON_GetObjectItem(device, "id"));
    if( t != NULL && i != NULL && strcmp(t, type) == 0 && strcmp(i, id) == 0 )
    {
      return device;
    }
  }
  return NULL;
}

static bool hasString(cJSON* object, const char* name, const char* value)
{
  const char* s = cJSON_GetStringValue(cJSON_GetObjectItem(object, name));
  return s != NULL && strcmp(s, value) == 0;
}

static bool brokerConnected(const char* path)
{
  cJSON* status = readStatus(path);
  bool connected = cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(status, "broker"), "connected"));
  cJSON_Delete(status);
  return connected;
}

// ---- Broker ----

static pid_t startBroker(const char* path, int port)
{
  pid_t pid = fork();
  if( pid == 0 )
  {
    char portStr[10];
    snprintf(portStr, sizeof(portStr), "%d", port);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execl(path, path, "-p", portStr, (char*) NULL);
    _exit(127);
  }
  return pid;
}

// Topics that a device without a name may not use when filtered
static bool hasTopicWith(const char* part)
{
  bool found = false;
  pthread_mutex_lock(&msgLock);
  for( int i = 0; i < messageCount; i++ )
  {
    if( strstr(messages[i].topic, part) != NULL )
    {
      found = true;
    }
  }
  pthread_mutex_unlock(&msgLock);
  return found;
}

// Modes: "translation" names the devices with topic-translation,
// "devices" with the devices list (same result expected), "filter" as
// "devices" with filter-non-transled: devices without a name are not published.
static bool writeConfig(const char* filename, int port, const char* mode)
{
  FILE* f = fopen(filename, "wt");
  if( f == NULL )
  {
    return false;
  }
  fprintf(f,
    "{\n"
    "  \"host\": \"127.0.0.1\",\n"
    "  \"port\": %d,\n"
    "  \"user\": \"\",\n"
    "  \"pass\": \"\",\n"
    "  \"sensor-offline-seconds\": 600,\n", port);
  if( strcmp(mode, "translation") == 0 )
  {
    fprintf(f,
      "  \"topic-translation\": [\n"
      "    {\n"
      "      \"telldus\": \"telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_135\",\n"
      "      \"mqtt\": \"Home/Shed\",\n"
      "      \"name\": \"Shed\"\n"
      "    },\n"
      "    {\n"
      "      \"telldus\": \"telldus/" SERIAL "/switch/2\",\n"
      "      \"mqtt\": \"Home/Lamp\",\n"
      "      \"name\": \"Lamp\"\n"
      "    }\n"
      "  ]\n"
      "}\n");
  }
  else
  {
    // Entries for another TellStick (gateway) must not be used
    fprintf(f,
      "  %s"
      "  \"devices\": [\n"
      "    { \"type\": \"switch\", \"id\": \"2\", \"name\": \"Other\", \"topic\": \"Other/Lamp\", \"gateway\": \"OTHERSERIAL\" },\n"
      "    { \"type\": \"sensor\", \"id\": \"fineoffset_temperaturehumidity_77\", \"name\": \"Other\", \"topic\": \"Other/77\", \"gateway\": \"OTHERSERIAL\" },\n"
      "    { \"type\": \"sensor\", \"id\": \"fineoffset_temperaturehumidity_135\", \"name\": \"Shed\", \"topic\": \"Home/Shed\" },\n"
      "    { \"type\": \"switch\", \"id\": \"2\", \"name\": \"Lamp\", \"topic\": \"Home/Lamp\", \"gateway\": \"" SERIAL "\" }\n"
      "  ],\n"
      // devices wins over topic-translation
      "  \"topic-translation\": [\n"
      "    { \"telldus\": \"telldus/" SERIAL "/switch/2\", \"mqtt\": \"Old/Lamp\", \"name\": \"Old\" }\n"
      "  ]\n"
      "}\n",
      strcmp(mode, "filter") == 0 ? "\"filter-non-transled\": \"yes\",\n" : "");
  }
  fclose(f);
  return true;
}

int main(int argc, char* argv[])
{
  if( argc < 2 )
  {
    printf("Usage: testbridge <mosquitto> [translation|devices|filter]\n");
    return SKIP;
  }
  const char* mode = argc > 2 ? argv[2] : "translation";
  bool filter = strcmp(mode, "filter") == 0;
  if( !filter && strcmp(mode, "translation") != 0 && strcmp(mode, "devices") != 0 )
  {
    printf("Unknown mode %s\n", mode);
    return 1;
  }

  int res = 0;
  int port = 20000 + (getpid() % 20000);
  char configFile[64];
  snprintf(configFile, sizeof(configFile), "testbridge-%d.json", port);
  char statusFile[64];
  snprintf(statusFile, sizeof(statusFile), "testbridge-%d-status.json", port);
  cJSON* status = NULL;
  struct mosquitto* observer = NULL;
  MqttClient* mqttclient = NULL;

  Log_Init(TM_LOG_CONSOLE, "testbridge", TM_LOG_DEBUG, false);
  if( !writeConfig(configFile, port, mode) )
  {
    printf("Could not write %s\n", configFile);
    return 1;
  }

  pid_t broker = startBroker(argv[1], port);
  if( broker < 0 )
  {
    return SKIP;
  }

  mosquitto_lib_init();
  observer = mosquitto_new("testbridge-observer", true, NULL);
  mosquitto_connect_callback_set(observer, observerOnConnect);
  mosquitto_subscribe_callback_set(observer, observerOnSubscribe);
  mosquitto_message_callback_set(observer, observerOnMessage);
  bool brokerUp = false;
  for( int t = 0; t < 50 && !brokerUp; t++ )
  {
    MyThread_Sleep(100);
    brokerUp = mosquitto_connect(observer, "127.0.0.1", port, 60) == MOSQ_ERR_SUCCESS;
  }
  if( !brokerUp )
  {
    printf("Could not start broker %s on port %d => skip\n", argv[1], port);
    res = SKIP;
    goto cleanup;
  }
  mosquitto_loop_start(observer);
  for( int t = 0; t < 50 && !observerConnected; t++ )
  {
    MyThread_Sleep(100);
  }
  CHECK( observerConnected );

  // TellStick with one configured switch
  TelldusSim_Reset();
  TelldusSim_SetController(5, TELLSTICK_CONTROLLER_TELLSTICK_DUO, SERIAL, true);
  TelldusSim_AddDevice(1);
  TelldusSim_AddDevice(2);

  // Retained topics left from before the translations were added (issue #6)
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_135/status", 6, "online", 0, true);
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/2/state", 2, "ON", 0, true);
  CHECK( waitFor("telldus/" SERIAL "/switch/2/state", "ON") );

  CHECK( Config_Load(Config_GetInstance(), configFile) == 0 );
  CHECK( Status_Init(statusFile) == 0 );
  TelldusClient* telldusclient = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(telldusclient) == 0 );
  mqttclient = MqttClient_GetInstance();
  CHECK( MqttClient_Connect(mqttclient) == 0 );

  // A translated device clears its old default retained state topic
  CHECK( waitFor("homeassistant/switch/" SERIAL "_device_2/config",
                 "\"command_topic\":\t\"Home/Lamp/set\"") );
  CHECK( waitFor("homeassistant/switch/" SERIAL "_device_2/config",
                 "\"name\":\t\"Lamp\"") );
  CHECK( waitForCleared("telldus/" SERIAL "/switch/2/state") );
  CHECK( !wasCleared("telldus/" SERIAL "/switch/1/state") );

  // Devices are announced to Home Assistant on connect, unnamed ones only without filter
  if( filter )
  {
    CHECK( !hasTopicWith("_device_1/") );
  }
  else
  {
    CHECK( waitFor("homeassistant/switch/" SERIAL "_device_1/config",
                   "\"command_topic\":\t\"telldus/" SERIAL "/switch/1/set\"") );
  }

  // A sensor without translation gets the default topics (without filter)
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 77, TELLSTICK_HUMIDITY, "55");
  if( !filter )
  {
    CHECK( waitFor("homeassistant/sensor/" SERIAL "_fineoffset_temperaturehumidity_77_Humidity/config", NULL) );
    CHECK( waitFor("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_77/Humidity", "55") );
    CHECK( waitFor("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_77/status", "online") );
  }

  // A translated sensor gets the user defined topic and name
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 135, TELLSTICK_TEMPERATURE, "21.5");
  CHECK( waitFor("homeassistant/sensor/" SERIAL "_fineoffset_temperaturehumidity_135_Temperature/config",
                 "\"name\":\t\"Shed\"") );
  CHECK( waitFor("homeassistant/sensor/" SERIAL "_fineoffset_temperaturehumidity_135_Temperature/config",
                 "\"state_topic\":\t\"Home/Shed/Temperature\"") );
  CHECK( waitFor("Home/Shed/Temperature", "21.5") );
  CHECK( waitFor("Home/Shed/status", "online") );
  CHECK( waitForCleared("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_135/status") );
  CHECK( !wasCleared("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_77/status") );
  CHECK( !wasCleared("homeassistant/sensor/" SERIAL "_fineoffset_temperaturehumidity_135_Temperature/config") );

  // Home Assistant switches the device on: TellStick sends, state is reported back
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 2, "ON", 0, false);
  if( filter )
  {
    // Not subscribed: no command handling. Use the named switch instead.
    mosquitto_publish(observer, NULL, "Home/Lamp/set", 2, "ON", 0, false);
    CHECK( waitFor("Home/Lamp/state", "ON") );
    CHECK( TelldusSim_GetTurnOnCount(2) >= 1 );
    CHECK( TelldusSim_GetTurnOnCount(1) == 0 );
  }
  else
  {
    CHECK( waitFor("telldus/" SERIAL "/switch/1/state", "ON") );
    CHECK( TelldusSim_GetTurnOnCount(1) >= 1 );

    // An empty payload (e.g. clearing a retained topic) must be ignored
    mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 0, NULL, 0, false);
    mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 3, "OFF", 0, false);
    CHECK( waitFor("telldus/" SERIAL "/switch/1/state", "OFF") );
    CHECK( TelldusSim_GetTurnOffCount(1) >= 1 );
  }

  // A remote control press is forwarded to MQTT
  TelldusSim_DeviceEvent(1, TELLSTICK_TURNON);
  CHECK( MqttClient_IsConnected(mqttclient) );
  if( filter )
  {
    // Unnamed: nothing published. The named switch after it shows the order.
    TelldusSim_DeviceEvent(2, TELLSTICK_TURNOFF);
    CHECK( waitFor("Home/Lamp/state", "OFF") );
    CHECK( !hasTopicWith("/switch/1/state") ); // The observer itself sent switch/1/set
    CHECK( !hasTopicWith("_77") );
  }
  CHECK( !hasTopicWith("Other/") );
  CHECK( !hasTopicWith("Old/") );

  // Status file: broker state and names from the same config
  CHECK( Status_Write() == 0 );
  status = readStatus(statusFile);
  CHECK( status != NULL );
  cJSON* brokerStatus = cJSON_GetObjectItem(status, "broker");
  CHECK( cJSON_IsTrue(cJSON_GetObjectItem(brokerStatus, "connected")) );
  CHECK( hasString(brokerStatus, "host", "127.0.0.1") );
  CHECK( cJSON_GetNumberValue(cJSON_GetObjectItem(brokerStatus, "port")) == port );
  CHECK( cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(status, "service"), "connected")) );
  CHECK( cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(status, "gateway"), "found")) );
  cJSON* lamp = findDevice(status, "switch", "2");
  CHECK( hasString(lamp, "name", "Lamp") );
  CHECK( hasString(lamp, "topic", "Home/Lamp") );
  CHECK( hasString(cJSON_GetObjectItem(findDevice(status, "switch", "1"), "values"), "state", "ON") );
  cJSON* shed = findDevice(status, "sensor", "fineoffset_temperaturehumidity_135");
  CHECK( hasString(shed, "name", "Shed") );
  CHECK( hasString(shed, "topic", "Home/Shed") );
  CHECK( hasString(cJSON_GetObjectItem(shed, "values"), "temperature", "21.5") );
  cJSON* sensor77 = findDevice(status, "sensor", "fineoffset_temperaturehumidity_77");
  CHECK( sensor77 != NULL );
  CHECK( cJSON_GetObjectItem(sensor77, "name") == NULL );
  CHECK( hasString(cJSON_GetObjectItem(sensor77, "values"), "humidity", "55") );

  // Broker disconnect is shown
  MqttClient_Disconnect(mqttclient);
  CHECK( Status_Write() == 0 );
  CHECK( !brokerConnected(statusFile) );

cleanup:
  if( res == 1 )
  {
    printf("Messages seen on the broker:\n");
    dumpMessages();
  }
  if( observer != NULL )
  {
    mosquitto_disconnect(observer);
    mosquitto_loop_stop(observer, true);
    mosquitto_destroy(observer);
  }
  if( mqttclient != NULL )
  {
    MqttClient_Destroy(mqttclient);
  }
  kill(broker, SIGTERM);
  waitpid(broker, NULL, 0);
  cJSON_Delete(status);
  Status_Destroy();
  remove(statusFile);
  remove(configFile);
  printf("%s\n", res == 0 ? "PASSED" : res == SKIP ? "SKIPPED" : "FAILED");
  return res;
}
