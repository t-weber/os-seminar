/**
 * simple libc string replacement functions
 * @author Tobias Weber
 * @date mar-21
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __MY_STRING_H__
#define __MY_STRING_H__

#if __has_include("defines.h") && __has_include(<sel4/sel4.h>)
	#include "defines.h"
#else
	typedef unsigned char u8;
	typedef char i8;
	typedef unsigned short u16;
	typedef short i16;
	typedef unsigned int u32;
	typedef int i32;
	typedef unsigned long u64;
	typedef long i64;
	typedef float f32;
	typedef double f64;
#endif


extern void reverse_str(i8* buf, u64 len);

extern i8 digit_to_char(u8 num, u64 base);
extern void uint_to_str(u64 num, u64 base, i8* buf);
extern void int_to_str(i64 num, u64 base, i8* buf);
extern void real_to_str(f64 num, u64 base, i8* buf, u8 decimals);

extern i64 my_atoi(const i8* str, i64 base);
extern f64 my_atof(const i8* str, i64 base);

extern void my_strncpy(i8* str_dst, const i8* str_src, u64 max_len);
extern void my_strncat(i8* str_dst, const i8* str_src, u64 max_len);
extern void strncat_char(i8* str, i8 c, u64 max_len);

extern i8 my_strncmp(const i8* str1, const i8* str2, u64 max_len);
extern i8 my_strcmp(const i8* str1, const i8* str2);

extern u64 my_strlen(const i8* str);
extern void my_memset(i8* mem, i8 val, u64 size);
extern void my_memset_interleaved(i8* mem, i8 val, u64 size, u8 interleave);
extern void my_memcpy(i8* mem_dst, i8* mem_src, u64 size);
extern void my_memcpy_interleaved(i8* mem_dst, i8* mem_src, u64 size, u8 interleave);

extern i64 my_max(i64 a, i64 b);

extern i8 my_isupperalpha(i8 c);
extern i8 my_isloweralpha(i8 c);
extern i8 my_isalpha(i8 c);
extern i8 my_isdigit(i8 c, i8 hex);

extern void write_char(i8 ch, u8 attrib, i8 *addr);
extern void write_str(i8 *str, u8 attrib, i8 *addr);
extern void read_str(i8 *str, const i8 *addr, u32 len);
extern void clear_scr(u8 attrib, i8 *addr, u64 size);

#endif
