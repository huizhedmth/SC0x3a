#ifndef MEM_H
#define MEM_H

#include "hardware.h"

/* return frame number upon success, otherwise return 0 
 * since frame 0 is definitely not available
 * pick 0 as return value
 */
int grab_a_frame();

#endif // MEM_H
