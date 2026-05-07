#ifndef MQTT_HA_HARMONY_H
#define MQTT_HA_HARMONY_H

#include <stdbool.h>

bool mqtt_is_connected(void);
void mqtt_client_init(void);
void mqtt_publish_device_status(void);

#endif