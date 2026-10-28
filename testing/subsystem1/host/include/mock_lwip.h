#ifndef MOCK_LWIP_H
#define MOCK_LWIP_H
#include <stdint.h>
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef int err_t;
typedef struct { int connected; } mqtt_client_t;
typedef struct { unsigned value; } ip_addr_t;
typedef int mqtt_connection_status_t;
#define ERR_OK 0
#define ERR_MEM (-1)
#define ERR_CONN (-11)
#define MQTT_CONNECT_ACCEPTED 0
#define MQTT_DATA_FLAG_LAST 1
#define IPADDR_TYPE_V4 0
#define IP_SET_TYPE(address, type) ((void)(address), (void)(type))
#define ip_2_ip4(address) (address)
struct mqtt_connect_client_info_t {
 const char *client_id; u16_t keep_alive;
 const char *will_topic; const char *will_msg; u8_t will_msg_len; u8_t will_retain;
};
typedef void (*mqtt_connection_cb_t)(mqtt_client_t *,void *,mqtt_connection_status_t);
typedef void (*mqtt_request_cb_t)(void *,err_t);
typedef void (*mqtt_incoming_publish_cb_t)(void *,const char *,u32_t);
typedef void (*mqtt_incoming_data_cb_t)(void *,const u8_t *,u16_t,u8_t);
mqtt_client_t *mqtt_client_new(void);
int mqtt_client_is_connected(mqtt_client_t *client);
err_t mqtt_client_connect(mqtt_client_t *,const ip_addr_t *,u16_t,mqtt_connection_cb_t,void *,const struct mqtt_connect_client_info_t *);
void mqtt_disconnect(mqtt_client_t *);
void mqtt_set_inpub_callback(mqtt_client_t *,mqtt_incoming_publish_cb_t,mqtt_incoming_data_cb_t,void *);
err_t mqtt_subscribe(mqtt_client_t *,const char *,u8_t,mqtt_request_cb_t,void *);
err_t mqtt_publish(mqtt_client_t *,const char *,const void *,u16_t,u8_t,u8_t,mqtt_request_cb_t,void *);
int ip4addr_aton(const char *,ip_addr_t *);
u32_t sys_now(void);
#endif
