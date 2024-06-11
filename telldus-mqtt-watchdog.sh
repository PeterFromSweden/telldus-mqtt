#!/bin/sh -x
# Every hour setup below:
# echo '0 * * * * /usr/bin/telldus-mqtt-watchdog.sh' >> /etc/crontabs/root

timeout_secs=300
topic=Hemma

# Wait for arriving messages
mosquitto_sub --quiet -N -F '' -C 1 -W $timeout_secs -R -t "$topic/#"
return_code=$?

# Restart if no messages
if [ $return_code -ne 0 ]; then
	logger -t "telldus-mqtt-watchdog.sh" "Message timeout => restart telldus-mqtt"
	service telldus-mqtt restart
fi
