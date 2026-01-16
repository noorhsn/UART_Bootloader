#include "config.h"
#include "uart.h"
#include "protocol.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Minimal bootloader main
   - Decide bootloader vs application via a simple GPIO/flag check (stubbed)
   - If bootloader, run protocol
   - On success, jump to application
*/

int boot_pin_asserted(void) {
    // TODO: read a GPIO or flash flag. For simulation return 1 to always stay in bootloader.
#ifdef USE_SIM
    return 1;
#else
    // read MCU GPIO input here
    return 0;
#endif
}

typedef void (*pfunc)(void);

void jump_to_app(void) {
    uint32_t app_stack = *(uint32_t *)APP_START;
    uint32_t reset_vector = *(uint32_t *)(APP_START + 4);
    if (app_stack == 0xFFFFFFFF || reset_vector == 0xFFFFFFFF) return;
#ifdef USE_SIM
    // In simulation we cannot set MSP or jump to raw addresses; instead just print and exit.
    (void)app_stack; (void)reset_vector;
    uart_tx_blocking("[SIM] Jump to application requested\n");
    exit(0);
#else
    // set MSP (hardware)
    __asm volatile ("msr msp, %0" : : "r" (app_stack) : );
    pfunc reset_handler = (pfunc)reset_vector;
    reset_handler();
#endif
}

int main(void) {
    uart_init();
    if (!boot_pin_asserted()) {
        // attempt to jump to application
        jump_to_app();
    }

    // otherwise remain in bootloader and run protocol
    while (1) {
        int r = protocol_run();
        if (r == 0) {
            // protocol succeeded; attempt jump
            jump_to_app();
        }
        // On failure, stay in bootloader and wait for next transfer
    }
    return 0;
}
