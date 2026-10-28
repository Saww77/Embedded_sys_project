/* Pico-only API dispatch test. Compiled as definitions ONLY in API-test mode.
 * The makefile excludes all real subsystem2 sources in that mode, so motor
 * PWM, encoder interrupts and PID are neither initialized nor executed.
 * Names/signatures match Buddy 2's public interface; these are not its real
 * implementation. USB serial output demonstrates that MQTT reaches the API.
 */
#include "../subsystem2/subsystem2.h"

#if APP_BUDDY1_API_TEST
/* Set once before task startup, then accessed exclusively by s1_comm_task.
 * The test profile starts no other motion users, so there is no shared RMW. */
static struct {
    unsigned char moving : 1;
    unsigned char level : 3; /* 0..4 */
} simulated;
_Static_assert(sizeof(simulated) == 1u, "simulated motion must fit one byte");

EXPORT ER subsystem2_init(void)
{
    simulated.moving = 0;
    simulated.level = 0;
    return E_OK;
}

EXPORT ER subsystem2_start(void)
{
    tm_printf((UB*)"[API TEST] ready: motion functions are serial-only stubs\n");
    return E_OK;
}

EXPORT void subsystem2_test(void)
{
    tm_printf((UB*)"[API TEST] real Buddy 2 hardware tests are not running\n");
}

bool motion_set_speed(unsigned level)
{
    if (level > 4u) { return false; }
    /* Renewals at the same level are deliberately quiet. */
    if (simulated.level != level) {
        tm_printf((UB*)"[API TEST] motion_set_speed(%u) called\n", level);
    }
    simulated.level = (unsigned char)level;
    if (level == 0u) { simulated.moving = 0; }
    return true;
}

bool motion_forward(void)
{
    tm_printf((UB*)"[API TEST] motion_forward() called\n");
    if (simulated.level == 0u || simulated.moving) { return false; }
    simulated.moving = 1;
    return true;
}

void motion_stop(void)
{
    tm_printf((UB*)"[API TEST] motion_stop() called\n");
    simulated.moving = 0;
}

bool motion_get_status(S2_STATUS *out)
{
    unsigned i;
    if (out == 0) { return false; }
    for (i = 0; i < sizeof(out->bytes); ++i) { out->bytes[i] = 0; }
    /* Simulate only the mode and level the command owner needs to maintain
     * an active START. No sensor values are published as real measurements. */
    s2_put_bits(out, 0u, 2u, simulated.moving ? 1u : 0u);
    s2_put_bits(out, 2u, 3u, simulated.level);
    return true;
}

bool motion_busy(void)
{
    return simulated.moving != 0u;
}
#endif
