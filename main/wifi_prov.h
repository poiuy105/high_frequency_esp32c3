#ifndef WIFI_PROV_H
#define WIFI_PROV_H

#include <stdbool.h>

bool wifi_is_connected(void);
bool wifi_prov_is_running(void);
void wifi_prov_start(void);
void wifi_start_sta(void);
void wifi_force_reprovision(void);

#endif
