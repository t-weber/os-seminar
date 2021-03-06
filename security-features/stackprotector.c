/**
 * demonstrates and tests the GCC stack protector canary
 *
 * @author Tobias Weber
 * @date may-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * @see https://en.wikipedia.org/wiki/Buffer_overflow_protection
 *
 * no stack overflow protection:
 *	gcc -fno-stack-protector -Wall -Wextra -S -o unsafe.s stackprotector.c
 * stack overflow protection:
 *	gcc -fstack-protector -Wall -Wextra -S -o safe.s stackprotector.c
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define NUM_ADDRS           1 + 3
#define SIMPLE_CANARY_VALUE 0x12345678      /* additional simple canary value */
#ifndef SIMPLE_CANARY
	#define SIMPLE_CANARY   0               /* additional simple canary */
#endif


static void my_memcpy(int8_t* buf_dst, int8_t* buf_src, uint64_t num)
{
	for(uint64_t i=0; i<num; ++i)
		buf_dst[i] = buf_src[i];
}


void forbidden_func()
{
	printf("In %s, shouldn't get here.\n", __PRETTY_FUNCTION__);
	exit(0);
}


void unsafe_func(int8_t *buffer, uint64_t size)
{
#if SIMPLE_CANARY == 1
	uint64_t _canary = SIMPLE_CANARY_VALUE;
#endif
	printf("In %s\n", __PRETTY_FUNCTION__);

	int8_t local_buffer[sizeof(uint64_t)];
	my_memcpy(local_buffer, buffer, size);

#if SIMPLE_CANARY == 1
	if(_canary != SIMPLE_CANARY_VALUE)
	{
		puts("Stack has been corrupted, exiting.");
		exit(-1);
	}
#endif
}


int main()
{
	void* addr = &forbidden_func;
	void* addrbuf[NUM_ADDRS];
	for(uint64_t i=0; i<NUM_ADDRS; ++i)
		addrbuf[i] = addr;

	unsafe_func((int8_t*)addrbuf, sizeof(addrbuf));
	puts("unsafe_func returned normally.");
	return 0;
}
