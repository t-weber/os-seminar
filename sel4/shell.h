/**
 * a calculator system program running on sel4
 * @author Tobias Weber
 * @date apr-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/mapping/mapping.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/untyped/untyped.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/threads/threads.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/interrupts/interrupts.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/tutorials/dynamic-2/dynamic-2.md
 *   - https://github.com/seL4/sel4-tutorials/blob/master/libsel4tutorials/src/alloc.c
 *   - https://docs.sel4.systems/projects/sel4/api-doc.html
 */

#ifndef __CALC_SHELL_H__
#define __CALC_SHELL_H__


#include "defines.h"

extern void run_calc_shell(seL4_SlotPos start_notify, i8 *charout, seL4_SlotPos endpoint);


#endif
