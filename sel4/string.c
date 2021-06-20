/**
 * simple libc string replacement functions
 * @author Tobias Weber
 * @date mar-21
 * @license GPLv3, see 'LICENSE' file
 */

#include "string.h"


void reverse_str(i8* buf, u64 len)
{
	for(u64 i=0; i<len/2; ++i)
	{
		u64 j = len-i-1;
		//if(j <= i) break;

		i8 c = buf[i];
		buf[i] = buf[j];
		buf[j] = c;
	}
}


i8 digit_to_char(u8 num, u64 base)
{
	u64 mod = num % base;

	if(mod <= 9)
		return (i8)mod + '0';
	else
		return (i8)(mod-10) + 'a';
}


void uint_to_str(u64 num, u64 base, i8* buf)
{
	u64 idx = 0;
	while(1)
	{
		u64 mod = num % base;
		num /= base;

		if(num==0 && mod==0)
		{
			if(idx == 0)
				buf[idx++] = '0';
			break;
		}

		if(mod <= 9)
			buf[idx] = (i8)mod + '0';
		else
			buf[idx] = (i8)(mod-10) + 'a';
		++idx;
	}

	buf[idx] = 0;

	reverse_str(buf, idx);
}


void int_to_str(i64 num, u64 base, i8* buf)
{
	u64 idx = 0;
	u64 beg = 0;

	if(num < 0)
	{
		buf[idx] = '-';
		num = -num;

		++idx;
		++beg;
	}

	while(1)
	{
		i64 mod = num % base;
		num /= base;

		if(num==0 && mod==0)
		{
			if(idx == 0)
				buf[idx++] = '0';

			break;
		}

		if(mod <= 9)
			buf[idx] = (i8)mod + '0';
		else
			buf[idx] = (i8)(mod-10) + 'a';
		++idx;
	}

	buf[idx] = 0;

	reverse_str(buf+beg, idx-beg);
}


void real_to_str(f64 num, u64 base, i8* buf, u8 decimals)
{
	const f64 eps = 1e-8;

	// negative number?
	u64 idx = 0;
	if(num < 0)
	{
		buf[idx++] = '-';
		num = -num;
	}

	// get number before decimal point
	uint_to_str((u64)num, base, buf+idx);

	// get number after decimal point
	i8 buf_decimals[64];
	for(u8 dec=0; dec<decimals; ++dec)
	{
		// strip away digits before decimal point
		num -= (u64)num;

		// get next decimal
		num *= base;
		// for numeric stability
		if(num >= base - eps)
			num = 0;

		u8 digit = (u8)num;
		// for numeric stability
		if(num >= (f64)digit + 1 - eps)
			++digit;

		buf_decimals[dec] = digit_to_char(digit, base);
	}
	buf_decimals[decimals] = 0;

	// strip away trailing '0's
	for(i16 dec=decimals-1; dec>=0; --dec)
	{
		if(buf_decimals[dec] == '0')
			buf_decimals[dec] = 0;
		else
			break;
	}

	if(my_strlen(buf_decimals))
	{
		strncat_char(buf, '.', 64);
		my_strncat(buf, buf_decimals, 64);
	}
}


u64 my_strlen(const i8 *str)
{
	u64 len = 0;

	while(str[len])
		++len;

	return len;
}


void my_memset(i8* mem, i8 val, u64 size)
{
	for(u64 i=0; i<size; ++i)
		mem[i] = val;
}


void my_memset_interleaved(i8* mem, i8 val, u64 size, u8 interleave)
{
	for(u64 i=0; i<size; i+=interleave)
		mem[i] = val;
}


void my_memcpy(i8* mem_dst, i8* mem_src, u64 size)
{
	for(u64 i=0; i<size; ++i)
		mem_dst[i] = mem_src[i];
}


void my_memcpy_interleaved(i8* mem_dst, i8* mem_src, u64 size, u8 interleave)
{
	for(u64 i=0; i<size; i+=interleave)
		mem_dst[i] = mem_src[i];
}


void my_strncpy(i8* str_dst, const i8* str_src, u64 max_len)
{
	for(u64 i=0; i<max_len; ++i)
	{
		i8 c = str_src[i];
		str_dst[i] = c;

		if(c == 0)
			break;
	}
}


void my_strncat(i8* str_dst, const i8* str_src, u64 max_len)
{
	u64 len = my_strlen(str_dst);
	my_strncpy(str_dst + len, str_src, max_len - len);
}


void strncat_char(i8* str, i8 c, u64 max_len)
{
	u64 len = my_strlen(str);
	if(len+1 < max_len)
	{
		str[len] = c;
		str[len+1] = 0;
	}
}


i8 my_strncmp(const i8* str1, const i8* str2, u64 max_len)
{
	for(u64 i=0; i<max_len; ++i)
	{
		i8 c1 = str1[i];
		i8 c2 = str2[i];

		if(c1 == c2 && c1 != 0)
			continue;
		else if(c1 < c2)
			return -1;
		else if(c1 > c2)
			return 1;
		else if(c1 == 0 && c2 == 0)
			return 0;
	}

	return 0;
}


i8 my_strcmp(const i8* str1, const i8* str2)
{
	u64 len1 = my_strlen(str1);
	u64 len2 = my_strlen(str2);

	return my_strncmp(str1, str2, my_max(len1, len2));
}


i64 my_max(i64 a, i64 b)
{
	if(b > a)
		return b;
	return a;
}


i8 my_isupperalpha(i8 c)
{
	return (c>='A' && c<='Z');
}


i8 my_isloweralpha(i8 c)
{
	return (c>='a' && c<='z');
}


i8 my_isalpha(i8 c)
{
	return my_isupperalpha(c) || my_isloweralpha(c);
}


i8 my_isdigit(i8 c, i8 hex)
{
	i8 is_num = (c>='0' && c<='9');
	if(hex)
	{
		is_num = is_num &&
			((c>='a' && c<='f') || (c>='A' && c<='F'));
	}

	return is_num;
}


i64 my_atoi(const i8* str, i64 base)
{
	u64 len = my_strlen(str);
	i64 num = 0;

	for(u64 i=0; i<len; ++i)
	{
		i64 digit = 0;
		if(my_isdigit(str[i], 0))
			digit = str[i] - '0';
		else if(my_isupperalpha(str[i]))
			digit = (str[i] - 'A') + 10;
		else if(my_isloweralpha(str[i]))
			digit = (str[i] - 'a') + 10;

		num = num*base + digit;
	}

	return num;
}


f64 my_atof(const i8* str, i64 base)
{
	u64 len = my_strlen(str);
	f64 num = 0, decimal = 0;
	i8 in_num = 1;
	i64 denom_pos = 1;

	for(u64 i=0; i<len; ++i)
	{
		if(str[i] == '.')
		{
			in_num = 0;
			continue;
		}

		f64 digit = 0;
		if(my_isdigit(str[i], 0))
			digit = str[i] - '0';
		else if(my_isupperalpha(str[i]))
			digit = (str[i] - 'A') + 10;
		else if(my_isloweralpha(str[i]))
			digit = (str[i] - 'a') + 10;

		// before decimal point
		if(in_num)
		{
			num = num*((f64)base) + digit;
		}

		// after decimal point
		else
		{
			for(i64 j=0; j<denom_pos; ++j)
				digit /= (f64)base;
			decimal += digit;

			++denom_pos;
		}
	}

	return num + decimal;
}


void write_char(i8 ch, u8 attrib, i8 *addr)
{
	*addr++ = ch;
	*addr++ = attrib;
}


void write_str(i8 *str, u8 attrib, i8 *addr)
{
	u64 len = my_strlen(str);

	for(u64 i=0; i<len; ++i)
	{
		*addr++ = str[i];
		*addr++ = attrib;
	}
}


void read_str(i8 *str, const i8 *addr, u32 len)
{
	for(u64 i=0; i<len; ++i)
	{
		str[i] = *addr;
		addr += 2;
	}
}


void clear_scr(u8 attrib, i8 *addr, u64 size)
{
	for(u64 i=0; i<size; ++i)
	{
		*addr++ = 0;
		*addr++ = attrib;
	}
}


/**
 * print a byte size
 */
void write_size(u64 size, i8* str, u64 max_len)
{
	const u64 sizes[4] = { 1024*1024*1024, 1024*1024, 1024, 1 };
	const i8* size_names[4] = { " GB ", " MB ", " kB ", " B" };

	for(u16 i=0; i<sizeof(sizes)/sizeof(*sizes); ++i)
	{
		u16 sz = size / sizes[i];
		size %= sizes[i];

		if(!sz)
			continue;

		uint_to_str(sz, 10, str);
		my_strncat(str, size_names[i], max_len);
		u64 len = my_strlen(str);
		str += len;
		max_len -= len;
	}
}


// test
/*#include <stdio.h>
int main()
{
	i8 buf[64];
	real_to_str(-987.01020300, 10, buf, 10);
	puts(buf);

	i8 buf2[64];
	write_size(5*1024*1024 + 4*1024 + 3, buf2, sizeof(buf2));
	puts(buf2);
}*/
