#include <stdio.h>
#include <mosquitto.h>
#include <mqtt_protocol.h>
#include <telldus-core.h>

int main(int argc, char *argv[])
{
  printf("main\r\n");
  
  // Mosquitto lib
  mosquitto_lib_init();
  int major, minor, revision;
  mosquitto_lib_version(&major, &minor, &revision);
  printf("mosquitto version %d.%d.%d\r\n", major, minor, revision);

  // Telldus-core lib
  tdInit();
  printf("Assuming telldus-core init was Ok...\r\n");
  
  return 0;
}