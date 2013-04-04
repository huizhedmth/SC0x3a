#include "hardware.h"

void trap_kernel_hdl(UserContext* u_context);
void trap_clock_hdl(UserContext* u_context);
void trap_illegal_hdl(UserContext* u_context);
void trap_memory_hdl(UserContext* u_context);
void trap_math_hdl(UserContext* u_context);
void trap_tty_receive_hdl(UserContext* u_context);
void trap_tty_transmit_hdl(UserContext* u_context);
void trap_disk_hdl(UserContext* u_context);

void trap_undefined_hdl(UserContext* u_context);


