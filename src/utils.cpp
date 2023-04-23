#include "utils.h"

bool debounce(uint32_t debounce_timestamp, uint32_t current_time)
{
    if (debounce_timestamp > current_time) {
        // Timer rollover happened, move everything back. Should be rare.
        return debounce_timestamp - 0x7FFFFFFF < (current_time - TIME_DEBOUNCE - 0x7FFFFFF);
    }
    return debounce_timestamp < (current_time - TIME_DEBOUNCE);
}