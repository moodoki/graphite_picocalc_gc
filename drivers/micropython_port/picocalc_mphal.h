/* Graphite's mphalport for the MicroPython embed port (Phase 6B.1).
 *
 * The embed port ships its own port/mphalport.h, which is a single line
 * (`#define mp_hal_pin_obj_t`) and declares none of the HAL functions the
 * py/ core calls when features above the bare minimum are switched on.
 * mpconfigport.h repoints MICROPY_MPHALPORT_H here so we can add them
 * without editing the submodule.
 *
 * Definitions live in src/scripting/mp_port.c.
 */

#ifndef PICOCALC_MPHAL_H
#define PICOCALC_MPHAL_H

#include <port/mphalport.h>

#include <stdint.h>

/* Required by MICROPY_KBD_EXCEPTION, which backs micropython.kbd_intr().
 * We interrupt from the VM hook on ESC rather than on a serial character,
 * so this records the character and is otherwise inert — but py/ calls it
 * unconditionally when the feature is on. */
void mp_hal_set_interrupt_char(int c);

#endif /* PICOCALC_MPHAL_H */
