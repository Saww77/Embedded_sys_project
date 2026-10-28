/* Size comparison reference, not a claim these native layouts existed before. */
#include <stdint.h>
#include <stdio.h>
#include "../../../app_program/subsystem1/s1_wire.h"
int main(void)
{
    struct vehicle_reference { uint32_t state, motor, encoder, imu, echo, wifi; };
    struct line_reference { uint32_t ir, barcode; };
    struct terrain_reference { uint32_t height, phase; };
    struct obstacle_reference { uint32_t nearest, width, left, right; };
    struct command_reference { uint32_t opcode; };
    unsigned char line[1], terrain[2], obstacle[6], command[1];
    puts("Reference native uint32_t fields vs implemented binary payload bytes:");
    printf("vehicle: %zu -> 1\n", sizeof(struct vehicle_reference));
    printf("line: %zu -> %zu\n", sizeof(struct line_reference), sizeof(line));
    printf("terrain: %zu -> %zu\n", sizeof(struct terrain_reference), sizeof(terrain));
    printf("obstacle: %zu -> %zu\n", sizeof(struct obstacle_reference), sizeof(obstacle));
    printf("START/STOP command: %zu -> %zu\n", sizeof(struct command_reference), sizeof(command));
    return 0;
}
