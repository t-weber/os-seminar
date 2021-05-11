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

	struct ParserContext ctx;
	init_parser(&ctx);

	clear_scr(ATTR_NORM, charout, SCREEN_SIZE);
	write_str("Seminar 1914             seL4 Calculator Shell ver. 0.2                   tweber",
		ATTR_INV, charout);

	u64 output_num = 1;
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
			t_value val = parse(&ctx, line);

			i8 outnumbuf[64];
			int_to_str(output_num, 10, outnumbuf);

			i8 numbuf[64];
			my_strncpy(numbuf, "[out ", sizeof(numbuf));
			my_strncat(numbuf, outnumbuf, sizeof(numbuf));
			my_strncat(numbuf, "] ", sizeof(numbuf));
			u64 numbuf_idx = my_strlen(numbuf);

#ifdef USE_INTEGER
			int_to_str(val, 10, numbuf+numbuf_idx);
#else
			real_to_str(val, 10, numbuf+numbuf_idx, 8);
#endif
			write_str(numbuf, ATTR_BOLD, charout + (y+1)*SCREEN_COL_SIZE*2 + x_min*2);

			print_symbols(&ctx);

			// new line
			y += 2;
			x = x_min;
			++output_num;

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

			i8 ch = 0;

			if(num >= 0 && num <= 9)
				ch = (i8)(num+0x30);
			else
			{
				switch(key)
				{
					case 0x39: ch = ' '; break;
					case 0x34: ch = '.'; break;
					case 0x33: ch = ','; break;

					case 0x0d: ch = '+'; break;
					case 0x0c: ch = '-'; break;
					case 0x28: ch = '*'; break;
					case 0x35: ch = '/'; break;
					case 0x29: ch = '^'; break;

					case 0x1a: ch = '('; break;
					case 0x1b: ch = ')'; break;
					case 0x2b: ch = '='; break;

					case 0x10: ch = 'q'; break;
					case 0x11: ch = 'w'; break;
					case 0x12: ch = 'e'; break;
					case 0x13: ch = 'r'; break;
					case 0x14: ch = 't'; break;
					case 0x15: ch = 'y'; break;
					case 0x16: ch = 'u'; break;
					case 0x17: ch = 'i'; break;
					case 0x18: ch = 'o'; break;
					case 0x19: ch = 'p'; break;

					case 0x1e: ch = 'a'; break;
					case 0x1f: ch = 's'; break;
					case 0x20: ch = 'd'; break;
					case 0x21: ch = 'f'; break;
					case 0x22: ch = 'g'; break;
					case 0x23: ch = 'h'; break;
					case 0x24: ch = 'j'; break;
					case 0x25: ch = 'k'; break;
					case 0x26: ch = 'l'; break;

					case 0x2c: ch = 'z'; break;
					case 0x2d: ch = 'x'; break;
					case 0x2e: ch = 'c'; break;
					case 0x2f: ch = 'v'; break;
					case 0x30: ch = 'b'; break;
					case 0x31: ch = 'n'; break;
					case 0x32: ch = 'm'; break;
				}
			}

			if(ch)
			{
				write_char(ch, ATTR_NORM, charout + y*SCREEN_COL_SIZE*2 + x*2);
				++x;
			}
		}
	}

	deinit_parser(&ctx);

	printf("End of calculator thread.\n");
	while(1) seL4_Yield();
}

