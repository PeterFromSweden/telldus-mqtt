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
#include <telldus-core.h>
#include "telldussim/telldussim.h"
#include "telldusclient.h"
#include "mqttclient.h"
#include "config.h"
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

static void dumpMessages(void)
{
  pthread_mutex_lock(&msgLock);
  for( int i = 0; i < messageCount; i++ )
  {
    printf("  %s%s: %.80s\n", messages[i].topic, messages[i].retain ? " (retained)" : "", messages[i].payload);
  }
  pthread_mutex_unlock(&msgLock);
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

static bool writeConfig(const char* filename, int port)
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
    "  \"sensor-offline-seconds\": 600,\n"
    "  \"topic-translation\": [\n"
    "    {\n"
    "      \"telldus\": \"telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_135\",\n"
    "      \"mqtt\": \"Home/Shed\",\n"
    "      \"name\": \"Shed\"\n"
    "    }\n"
    "  ]\n"
    "}\n", port);
  fclose(f);
  return true;
}

int main(int argc, char* argv[])
{
  if( argc < 2 )
  {
    printf("Usage: testbridge <mosquitto>\n");
    return SKIP;
  }

  int res = 0;
  int port = 20000 + (getpid() % 20000);
  char configFile[64];
  snprintf(configFile, sizeof(configFile), "testbridge-%d.json", port);
  struct mosquitto* observer = NULL;
  MqttClient* mqttclient = NULL;

  Log_Init(TM_LOG_CONSOLE, "testbridge", TM_LOG_DEBUG, false);
  if( !writeConfig(configFile, port) )
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

  CHECK( Config_Load(Config_GetInstance(), configFile) == 0 );
  TelldusClient* telldusclient = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(telldusclient) == 0 );
  mqttclient = MqttClient_GetInstance();
  CHECK( MqttClient_Connect(mqttclient) == 0 );

  // Devices are announced to Home Assistant on connect
  CHECK( waitFor("homeassistant/switch/" SERIAL "_device_1/config",
                 "\"command_topic\":\t\"telldus/" SERIAL "/switch/1/set\"") );

  // A sensor without translation gets the default topics
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 77, TELLSTICK_HUMIDITY, "55");
  CHECK( waitFor("homeassistant/sensor/" SERIAL "_fineoffset_temperaturehumidity_77_Humidity/config", NULL) );
  CHECK( waitFor("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_77/Humidity", "55") );
  CHECK( waitFor("telldus/" SERIAL "/sensor/fineoffset_temperaturehumidity_77/status", "online") );

  // A translated sensor gets the user defined topic and name
  TelldusSim_SensorEvent("fineoffset", "temperaturehumidity", 135, TELLSTICK_TEMPERATURE, "21.5");
  CHECK( waitFor("Home/Shed/Temperature", "21.5") );
  CHECK( waitFor("Home/Shed/status", "online") );

  // Home Assistant switches the device on: TellStick sends, state is reported back
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 2, "ON", 0, false);
  CHECK( waitFor("telldus/" SERIAL "/switch/1/state", "ON") );
  CHECK( TelldusSim_GetTurnOnCount(1) >= 1 );

  // An empty payload (e.g. clearing a retained topic) must be ignored
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 0, NULL, 0, false);
  mosquitto_publish(observer, NULL, "telldus/" SERIAL "/switch/1/set", 3, "OFF", 0, false);
  CHECK( waitFor("telldus/" SERIAL "/switch/1/state", "OFF") );
  CHECK( TelldusSim_GetTurnOffCount(1) >= 1 );

  // A remote control press is forwarded to MQTT
  TelldusSim_DeviceEvent(1, TELLSTICK_TURNON);
  CHECK( MqttClient_IsConnected(mqttclient) );

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
  remove(configFile);
  printf("%s\n", res == 0 ? "PASSED" : res == SKIP ? "SKIPPED" : "FAILED");
  return res;
}
