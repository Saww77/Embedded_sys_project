#ifndef MQTT_UTK_H
#define MQTT_UTK_H
/* Wi-Fi service context ONLY. Call every service iteration, including link down.
 * wifi_up means CYW43_LINK_UP (associated with an IP address). */
void mqtt_utk_poll(int wifi_up);
/* Compatibility entry point; service polling is still required. */
void mqtt_utk_start(void);
#endif
