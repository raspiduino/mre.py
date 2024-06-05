#include <unistd.h>
#include "py/mpconfig.h"

// Console header
#include "console/Console_io.h"

/*
 * Core UART functions to implement for a port
 */

// Receive single character
int mp_hal_stdin_rx_chr(void) {
    return 0;
}

// Send string of given length
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    console_str_with_length_in(str, (int)len);
    return len;
}
