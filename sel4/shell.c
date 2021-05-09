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

#include "shell.h"
#include "string.h"
#include "expr_parser.h"


void run_calc_shell(seL4_SlotPos start_notify, i8 *charout, seL4_SlotPos endpoint)
{
	printf("Start of calculator thread, endpoint: %ld.\n", endpoint);
	seL4_Signal(start_notify);

	i32 x_min = 1, y_min = 2;
	i32 x_max = SCREEN_COL_SIZE-1;

	i32 x=x_min, y=y_min;
	i32 x_prev=x, y_prev=y;

	// init parser
	init_symbols();

	clear_scr(ATTR_NORM, charout, SCREEN_SIZE);
	write_str("Seminar 1914             seL4 Calculator Shell ver. 0.2                   tweber",
		ATTR_INV, charout);

	while(1)
	{
		// cursor
		charout[(y_prev*SCREEN_COL_SIZE + x_prev)*2 + 1] = ATTR_NORM;
		charout[(y*SCREEN_COL_SIZE + x)*2 + 1] = ATTR_INV;

		x_prev = x;
		y_prev = y;

		word_t badge = 0;
		seL4_MessageInfo_t msg = seL4_Recv(endpoint, &badge);

		// get the key code from the message register
		i16 key = seL4_GetMR(0);
		seL4_Reply(msg);

		if(key == 0x1c)	// enter
		{
			// read current line
			i8 line[SCREEN_COL_SIZE+1];
			read_str(line, charout+y*SCREEN_COL_SIZE*2 + x_min*2, SCREEN_COL_SIZE);
			t_value val = parse(line);

			i8 numbuf[32];
			int_to_str(val, 10, numbuf);
			write_str(numbuf, ATTR_BOLD, charout + (y+1)*SCREEN_COL_SIZE*2 + x_min*2);

			print_symbols();

			// new line
			y += 2;
			x = x_min;

			// scroll
			if(y >= SCREEN_ROW_SIZE - 2)
			{
				// reset cursor
				charout[(y_prev*SCREEN_COL_SIZE + x_prev)*2 + 1] = ATTR_NORM;

				for(u32 _y = 3; _y < SCREEN_ROW_SIZE; ++_y)
				{
					my_memcpy(
						charout+SCREEN_COL_SIZE*(_y-2)*2,
						charout+SCREEN_COL_SIZE*_y*2,
						SCREEN_COL_SIZE*2);
				}

				y -= 2;

				// clear last lines
				clear_scr(ATTR_NORM, charout+(y)*SCREEN_COL_SIZE*2, SCREEN_COL_SIZE*2);
			}
		}
		else if(key == 0x0e && x >= x_min+1)	// backspace
		{
			--x;
			write_char(' ', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
		}
		else if(x < x_max)
		{
			// digit
			u64 num = key - 0x01;
			if(num == 10)
				num = 0;
			if(num >= 0 && num <= 9)
				write_char((i8)(num+0x30), ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x39)	// space
				write_char(' ', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x27 || key == 0x0d)	// +
				write_char('+', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x28 || key == 0x0c)	// -
				write_char('-', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x34)	// *
				write_char('*', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x28 || key == 0x35)	// /
				write_char('/', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x29)	// ^
				write_char('^', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x1a)	// (
				write_char('(', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x1b)	// )
				write_char(')', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x2b || key == 0x33)	// =
				write_char('=', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x10)	// q
				write_char('q', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x11)	// w
				write_char('w', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x12)	// e
				write_char('e', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x13)	// r
				write_char('r', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x14)	// t
				write_char('t', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x15)	// y
				write_char('y', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x16)	// u
				write_char('u', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x17)	// i
				write_char('i', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x18)	// o
				write_char('o', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x19)	// p
				write_char('p', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x1e)	// a
				write_char('a', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x1f)	// s
				write_char('s', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x20)	// d
				write_char('d', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x21)	// f
				write_char('f', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x22)	// g
				write_char('g', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x23)	// h
				write_char('h', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x24)	// j
				write_char('j', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x25)	// k
				write_char('k', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x26)	// l
				write_char('l', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x2c)	// z
				write_char('z', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x2d)	// x
				write_char('x', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x2e)	// c
				write_char('c', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x2f)	// v
				write_char('v', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x30)	// b
				write_char('b', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x31)	// n
				write_char('n', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else if(key == 0x32)	// m
				write_char('m', ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
			else
				continue;
			++x;
		}
	}

	// deinit parser
	deinit_symbols();

	printf("End of calculator thread.\n");
	while(1) seL4_Yield();
}
