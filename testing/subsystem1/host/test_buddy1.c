#include <assert.h>
#include <setjmp.h>
#include "../../../app_program/subsystem1/s1_comm.h"
#include "../../../app_program/subsystem2/subsystem2.h"
#include <stdio.h>
#include <string.h>
#include <tk/tkernel.h>
#include "mock_lwip.h"
#include "../../../app_program/subsystem1/s1_wire.h"
#include "../../../lib/liblwip/sysdepend/pico_rp2040/mqtt_bridge.h"
/* Include service to allow deterministic callback/clock fault injection. */
#include "../../../lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c"

static struct {
 unsigned char data[32][MQTT_BRIDGE_FRAME_MAX];
 int lengths[32]; unsigned count; unsigned used; int alive;
} queues[8];
static int next_id = 1;
static int fail_second = 1;
static int deletions;
ID tk_cre_mbf(const T_CMBF *config)
{
 assert(config->bufsz == 256 && config->maxmsz == 33);
 if (next_id == 2 && fail_second) { fail_second = 0; return E_NOMEM; }
 queues[next_id].alive = 1;
 return next_id++;
}
ER tk_del_mbf(ID id) { queues[id].alive = 0; ++deletions; return 0; }
ER tk_snd_mbf(ID id, const void *message, INT length, int timeout)
{
 unsigned size = ((unsigned)length + 3u) / 4u * 4u + 4u;
 assert(timeout == TMO_POL && queues[id].alive);
 if (queues[id].used + size > 256u) { return E_TMOUT; }
 assert(queues[id].count < 32);
 memcpy(queues[id].data[queues[id].count], message, (unsigned)length);
 queues[id].lengths[queues[id].count++] = length;
 queues[id].used += size;
 return E_OK;
}
INT tk_rcv_mbf(ID id, void *message, int timeout)
{
 unsigned i;
 int length;
 assert(timeout == TMO_POL && queues[id].alive);
 if (!queues[id].count) { return E_TMOUT; }
 length = queues[id].lengths[0];
 memcpy(message, queues[id].data[0], (unsigned)length);
 --queues[id].count;
 queues[id].used -= ((unsigned)length + 3u) / 4u * 4u + 4u;
 for (i = 0; i < queues[id].count; ++i) {
  memcpy(queues[id].data[i], queues[id].data[i + 1], MQTT_BRIDGE_FRAME_MAX);
  queues[id].lengths[i] = queues[id].lengths[i + 1];
 }
 return length;
}

static mqtt_client_t fake_client;
static u32_t clock_ms;
static unsigned connects, subscribes, published, disconnects;
static err_t publish_result, connect_result, subscribe_result;
static unsigned char last_payload[32];
static unsigned last_length;
static char last_topic[64];
mqtt_client_t *mqtt_client_new(void) { return &fake_client; }
int mqtt_client_is_connected(mqtt_client_t *c) { return c->connected; }
err_t mqtt_client_connect(mqtt_client_t *c,const ip_addr_t *a,u16_t port,
 mqtt_connection_cb_t cb,void *arg,const struct mqtt_connect_client_info_t *info)
{
 (void)c; (void)a; (void)cb; (void)arg;
 assert(port == 1883 && info->will_msg_len == 1 && info->will_msg[0] == 0);
 ++connects; return connect_result;
}
void mqtt_disconnect(mqtt_client_t *c) { ++disconnects; c->connected = 0; }
void mqtt_set_inpub_callback(mqtt_client_t *c,mqtt_incoming_publish_cb_t p,mqtt_incoming_data_cb_t d,void *arg)
{ (void)c; (void)arg; assert(p == incoming_publish_cb && d == incoming_data_cb); }
err_t mqtt_subscribe(mqtt_client_t *c,const char *topic,u8_t qos,mqtt_request_cb_t cb,void *arg)
{ (void)c; (void)cb; (void)arg; assert(!qos && !strcmp(topic,"robot/test/v1/command")); ++subscribes; return subscribe_result; }
err_t mqtt_publish(mqtt_client_t *c,const char *topic,const void *data,u16_t len,u8_t qos,u8_t retain,mqtt_request_cb_t cb,void *arg)
{
 (void)c; (void)retain; (void)cb; (void)arg; assert(!qos && len <= 32);
 ++published; memcpy(last_payload,data,len); last_length = len;
 strcpy(last_topic,topic); return publish_result;
}
int ip4addr_aton(const char *text,ip_addr_t *address) { (void)text; (void)address; return 1; }
u32_t sys_now(void) { return clock_ms; }

static void wire_tests(void)
{
 unsigned a, b, op, period;
 unsigned char data[6];
 for (a = 0; a < 256; ++a) {
  data[0] = (unsigned char)a;
  assert(s1_command_decode(data,1,&op,&period) == (a <= 1u));
  for (b = 0; b < 256; ++b) {
   unsigned ticks;
   int valid;
   data[1] = (unsigned char)b;
   ticks = ((a >> 2u) & 63u) | ((b & 1u) << 6u);
   valid = ((a & 3u) == S1_CMD_PERIOD && b <= 1u && ticks >= 1u && ticks <= 100u);
   assert(s1_command_decode(data,2,&op,&period) == valid);
   if (valid) { assert(op == 2u && period == ticks); }
  }
 }
 assert(!s1_command_decode(0,1,&op,&period));
 assert(!s1_command_decode(data,0,&op,&period));
 assert(!s1_command_decode(data,3,&op,&period));
 for (a = 0; a <= 2000u; ++a) {
  for (b = 0; b <= 5u; ++b) {
   assert(s1_wire_terrain(data,a,b));
   assert(s1_wire_get(data,0,11) == a && s1_wire_get(data,11,3) == b);
   assert((data[1] & 0xc0u) == 0);
  }
 }
 assert(!s1_wire_terrain(data,2001,0) && !s1_wire_terrain(data,0,6));
 for (a = 0; a < 8; ++a) for (b = 0; b <= 4; ++b) {
  assert(s1_wire_line(data,a,b) && data[0] == (a | (b << 3u)));
 }
 assert(!s1_wire_line(data,8,0) && !s1_wire_line(data,0,5));
 for (a = 0; a <= 4000u; ++a) {
  assert(s1_wire_obstacle(data,a,a / 2u,4000u-a,a));
  assert(s1_wire_get(data,0,12) == a && s1_wire_get(data,12,11) == a / 2u);
  assert(s1_wire_get(data,23,12) == 4000u-a && s1_wire_get(data,35,12) == a);
  assert((data[5] & 0x80u) == 0);
 }
 assert(!s1_wire_obstacle(data,4001,0,0,0));
 assert(!s1_wire_obstacle(data,0,2001,0,0));
 assert(!s1_wire_obstacle(data,0,0,4001,0));
 assert(!s1_wire_obstacle(data,0,0,0,4001));
 puts("PASS wire: exhaustive command bytes, all terrain values, line values, obstacle boundaries");
}

static void bridge_tests(void)
{
 unsigned char data[33] = {0, 255, 0};
 unsigned char frame[MQTT_BRIDGE_FRAME_MAX];
 unsigned sent = 0;
 assert(mqtt_bridge_send(1,data,3) == E_OBJ);
 mqtt_utk_poll(1); assert(connects == 0);
 assert(mqtt_bridge_init() == E_NOMEM && !mqtt_bridge_ready() && deletions == 1);
 assert(mqtt_bridge_init() == E_OK && mqtt_bridge_ready());
 assert(mqtt_bridge_init() == E_OK && next_id == 4);
 assert(mqtt_bridge_send(MQTT_ROUTE_COMMAND,data,1) == E_PAR);
 assert(mqtt_bridge_send(1,data,0) == E_PAR);
 assert(mqtt_bridge_send(1,data,33) == E_PAR);
 assert(mqtt_bridge_send(1,0,1) == E_PAR);
 assert(mqtt_bridge_send(1,data,3) == E_OK);
 data[1] = 3;
 assert(mqtt_bridge_service_take(frame) == 4 && frame[2] == 255 && frame[3] == 0);
 while (mqtt_bridge_send(1,data,32) == E_OK) { ++sent; }
 assert(sent == 6); /* 4-byte header + 36-byte aligned frame. */
 while (mqtt_bridge_service_take(frame) > 0) { }
 assert(mqtt_bridge_service_deliver(1,data,1) == E_PAR);
 puts("PASS bridge: startup rollback, init ordering, copy ownership, bounds, full queue, polling only");
}

static void receive_tests(void)
{
 unsigned char data[] = {2,0,255,0};
 unsigned char frame[MQTT_BRIDGE_FRAME_MAX];
 char topic[] = "robot/test/v1/command";
 incoming_publish_cb(0,topic,4);
 memset(topic,'X',sizeof(topic)-1);
 incoming_data_cb(0,data,2,0);
 assert(mqtt_bridge_receive(frame) == E_TMOUT);
 incoming_data_cb(0,data+2,2,MQTT_DATA_FLAG_LAST);
 memset(data,0,sizeof(data));
 assert(mqtt_bridge_receive(frame) == 5 && frame[0] == MQTT_ROUTE_COMMAND && frame[3] == 255);
 incoming_publish_cb(0,"robot/test/v1/command",33);
 incoming_data_cb(0,data,4,1);
 assert(mqtt_bridge_receive(frame) == E_TMOUT);
 incoming_publish_cb(0,"robot/test/v1/command",4);
 incoming_data_cb(0,data,3,1);
 assert(mqtt_bridge_receive(frame) == E_TMOUT);
 incoming_publish_cb(0,"robot/test/v1/command",2);
 incoming_data_cb(0,data,3,1);
 assert(mqtt_bridge_receive(frame) == E_TMOUT);
 incoming_publish_cb(0,"other",1); incoming_data_cb(0,data,1,1);
 assert(mqtt_bridge_receive(frame) == E_TMOUT);
 memset(topic,'X',sizeof(topic)-1);
 {
  char huge[100]; memset(huge,'X',99); huge[99] = 0;
  incoming_publish_cb(0,huge,1); incoming_data_cb(0,data,1,1);
  assert(mqtt_bridge_receive(frame) == E_TMOUT);
 }
 while (mqtt_bridge_service_deliver(MQTT_ROUTE_COMMAND,data,4) == 0) { }
 incoming_publish_cb(0,"robot/test/v1/command",4); incoming_data_cb(0,data,4,1);
 assert(flags & MQTT_LINK_RX_DROP);
 while (mqtt_bridge_receive(frame) > 0) { }
 puts("PASS RX: fragments, topic/payload ownership, binary NUL, oversize, length mismatch, wrong topic, full queue");
}

static void service_tests(void)
{
 unsigned char data[] = {0,255};
 unsigned before;
 clock_ms = 10000; connect_result = ERR_CONN;
 mqtt_utk_poll(1); assert(connects == 1 && !connecting);
 clock_ms += 1; mqtt_utk_poll(1); assert(connects == 1);
 clock_ms += RETRY_MS; connect_result = 0;
 mqtt_utk_poll(1); assert(connects == 2 && connecting);
 fake_client.connected = 1;
 connection_cb(&fake_client,0,MQTT_CONNECT_ACCEPTED);
 subscribe_result = ERR_MEM;
 mqtt_utk_poll(1); assert(subscribes == 1 && !subscribing);
 clock_ms += 1; mqtt_utk_poll(1); assert(subscribes == 1);
 clock_ms += RETRY_MS; subscribe_result = 0;
 mqtt_utk_poll(1); assert(subscribes == 2 && subscribing);
 subscribe_done_cb(0,ERR_CONN);
 clock_ms += RETRY_MS; mqtt_utk_poll(1); assert(subscribes == 3);
 subscribe_done_cb(0,0); mqtt_utk_poll(1);
 assert((mqtt_bridge_status() & 7u) == 7u);
 assert(mqtt_bridge_send(MQTT_ROUTE_LINE,data,2) == 0);
 publish_result = ERR_MEM; mqtt_utk_poll(1); assert(pending_size == 3);
 publish_result = 0; mqtt_utk_poll(1);
 assert(!pending_size && last_length == 2 && last_payload[1] == 255);
 assert(!strcmp(last_topic,"robot/test/v1/line"));
 assert(mqtt_bridge_send(1,data,2) == 0);
 assert(mqtt_bridge_send(1,data,2) == 0);
 assert(mqtt_bridge_send(1,data,2) == 0);
 before = published; mqtt_utk_poll(1); assert(published - before == TX_BUDGET);
 mqtt_utk_poll(1);
 assert(mqtt_bridge_send(1,data,2) == 0);
 publish_result = ERR_MEM; mqtt_utk_poll(1);
 clock_ms += PENDING_MS; mqtt_utk_poll(1); assert(!pending_size);
 publish_result = 0;
 connection_cb(&fake_client,0,1);
 before = connects; mqtt_utk_poll(1); assert(connects == before);
 clock_ms += RETRY_MS; mqtt_utk_poll(1); assert(connects == before + 1);
 connection_cb(&fake_client,0,0); mqtt_utk_poll(1); assert(subscribes == 4);
 subscribe_done_cb(0,0);
 assert(mqtt_bridge_send(1,data,2) == 0);
 mqtt_utk_poll(0); assert(disconnects == 1 && (mqtt_bridge_status() & 7u) == 0 && !pending_size);
 /* Wrap-safe backoff/heartbeat comparisons. */
 assert(due(3u,0xfffffff0u) && !due(0xfffffff0u,3u));
 puts("PASS service: connect/subscription backoff, resubscribe, binary heartbeat/LWT, bounded TX, ERR_MEM, expiry, link loss, clock wrap");
}

static jmp_buf task_done;
static unsigned stops;
static unsigned snapshots;
ER tk_dly_tsk(unsigned delay) { assert(delay == 100u); longjmp(task_done,1); return 0; }
void motion_stop(void) { ++stops; }
bool motion_get_status(S2_STATUS *out)
{
    unsigned i;
    for (i = 0; i < 9; ++i) { out->bytes[i] = (unsigned char)i; }
    out->bytes[8] &= 7u;
    ++snapshots;
    return true;
}
static void run_app_once(void)
{
    if (setjmp(task_done) == 0) { s1_comm_task(0,0); }
}
static void application_tests(void)
{
    unsigned char frame[MQTT_BRIDGE_FRAME_MAX];
    unsigned char stop = 0;
    unsigned char snapshot = 1;
    unsigned char invalid = 0x80;
    unsigned char period[] = {0x92,1}; /* 100 ticks = 10 seconds */
    assert(s1_publish_vehicle(8,0) == E_PAR);
    assert(s1_publish_vehicle(0,32) == E_PAR);
    assert(s1_publish_vehicle(7,31) == 0);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[1] == 255);
    assert(s1_publish_line(7,4) == 0);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[1] == 39);
    assert(s1_publish_terrain(2001,0) == E_PAR);
    assert(s1_publish_obstacle(4001,0,0,0) == E_PAR);
    assert(mqtt_bridge_service_deliver(MQTT_ROUTE_COMMAND,&stop,1) == 0);
    run_app_once(); assert(stops == 1 && snapshots == 0);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[0] == MQTT_ROUTE_ACK && frame[1] == 0);
    assert(mqtt_bridge_service_deliver(MQTT_ROUTE_COMMAND,&invalid,1) == 0);
    run_app_once(); assert(stops == 1 && snapshots == 0);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[1] == 4);
    assert(mqtt_bridge_service_deliver(MQTT_ROUTE_COMMAND,&snapshot,1) == 0);
    run_app_once(); assert(snapshots == 1);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[1] == 1);
    assert(mqtt_bridge_service_take(frame) == 10 && frame[0] == MQTT_ROUTE_MOTION);
    assert(mqtt_bridge_service_deliver(MQTT_ROUTE_COMMAND,period,2) == 0);
    run_app_once(); assert(snapshots == 2);
    assert(mqtt_bridge_service_take(frame) == 2 && frame[1] == 2);
    assert(mqtt_bridge_service_take(frame) == 10);
    puts("PASS application: public API bounds, STOP dispatch, malformed rejection, snapshot, interval command, binary ACK");
}

int main(void)
{
 wire_tests(); bridge_tests(); receive_tests(); service_tests(); application_tests();
 puts("All Buddy 1 host test groups passed (mock kernel/lwIP; no hardware claims).");
 return 0;
}
