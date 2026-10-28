/* Verify the no-hardware implementation of Buddy 2's API signatures/state. */
#include <assert.h>
#include <setjmp.h>
#include "../../../app_program/subsystem1/s1_comm.h"
#include "../../../lib/liblwip/sysdepend/pico_rp2040/mqtt_bridge.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../../app_program/subsystem2/subsystem2.h"

static char serial_log[2048];
static unsigned used;
int tm_printf(unsigned char *format, ...)
{
    va_list args;
    int count;
    va_start(args,format);
    count = vsnprintf(serial_log + used,sizeof(serial_log) - used,(char *)format,args);
    va_end(args);
    assert(count >= 0 && (unsigned)count < sizeof(serial_log) - used);
    used += (unsigned)count;
    return count;
}

static jmp_buf done;
static unsigned ticks, rx_stage, ack_count;
static unsigned char acknowledgments[2];
unsigned mqtt_bridge_status(void) { return 7u; }
int mqtt_bridge_receive(unsigned char frame[MQTT_BRIDGE_FRAME_MAX])
{
    frame[0] = MQTT_ROUTE_COMMAND;
    if (rx_stage == 0u) { frame[1] = 1; rx_stage = 1; return 2; }
    if (rx_stage == 1u && ticks >= 4u) { frame[1] = 0; rx_stage = 2; return 2; }
    return E_TMOUT;
}
int mqtt_bridge_send(unsigned route, const unsigned char *data, unsigned length)
{
    assert(route == MQTT_ROUTE_ACK && length == 1 && ack_count < 2u);
    acknowledgments[ack_count++] = data[0];
    return E_OK;
}
ER tk_dly_tsk(unsigned delay)
{
    assert(delay == 50u);
    ++ticks;
    if (ticks == 2u) { assert(motion_busy()); }
    if (ticks == 6u) { longjmp(done,1); }
    return E_OK;
}

int main(void)
{
    S2_STATUS status;
    assert(subsystem2_init() == E_OK && subsystem2_start() == E_OK);
    assert(motion_get_status(&status) && !motion_busy());
    assert(s2_get_bits(&status,0,2) == 0 && !motion_get_status(0));
    assert(!motion_forward()); /* Zero speed is rejected. */
    assert(!motion_set_speed(5));
    assert(motion_set_speed(1) && motion_forward() && motion_busy());
    assert(motion_get_status(&status) && s2_get_bits(&status,0,2) == 1);
    assert(s2_get_bits(&status,2,3) == 1 && s2_get_bits(&status,5,4) == 0);
    {
        unsigned before = used;
        assert(motion_set_speed(1));
        assert(used == before); /* Don't flood serial on each renewal. */
    }
    assert(!motion_forward());
    motion_stop();
    assert(!motion_busy() && motion_get_status(&status) && s2_get_bits(&status,0,2) == 0);
    assert(motion_forward());
    assert(motion_set_speed(0) && !motion_busy());
    subsystem2_test();
    assert(strstr(serial_log,"motion_set_speed(1) called"));
    assert(strstr(serial_log,"motion_forward() called"));
    assert(strstr(serial_log,"motion_stop() called"));
    puts("PASS API-test stub: signatures, serial logs, simulated state, bounds and quiet renewals (no real motor driver)");
    /* Feed copied START/STOP frames through the actual command task and
     * the actual target test stubs, rather than replacing motion APIs again. */
    assert(subsystem2_init() == E_OK);
    used = 0; serial_log[0] = 0;
    if (setjmp(done) == 0) { s1_comm_task(0,0); }
    assert(ack_count == 2 && acknowledgments[0] == 1 && acknowledgments[1] == 0);
    assert(!motion_busy());
    assert(strstr(serial_log,"motion_forward() called"));
    assert(strstr(serial_log,"motion_stop() called"));
    puts("PASS API dispatch: copied START/STOP -> real command task -> serial-logging test API -> accepted ACKs");
    return 0;
}
