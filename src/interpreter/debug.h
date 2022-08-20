#ifndef _VMI_THREAD_DEBUG_H_
#define _VMI_THREAD_DEBUG_H_

#include "config.h"
#include "stack.h"

// Print debug information about the current instruction
extern void vmi_debug_instruction(vmi_ip ip);

// Print debug information about the stack
extern void vmi_debug_stack(const vmi_stack* s);

#endif
