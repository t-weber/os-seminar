#
# demonstrates and tests the GCC stack protector canary
#
# @author Tobias Weber
# @date may-2021
# @license GPLv3, see 'LICENSE' file
#
# @see https://en.wikipedia.org/wiki/Buffer_overflow_protection
#

# -----------------------------------------------------------------------------
# tools
# -----------------------------------------------------------------------------
CC = gcc
ASM = gcc
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# meta rules
# -----------------------------------------------------------------------------
.PHONY: all clean

# make all binaries
all: unsafe safe safe_simple

# clean generated files
clean:
	rm -vf *.s
	rm -vf *.o
	rm -vf safe
	rm -vf safe_simple
	rm -vf unsafe
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# create assembler code
# -----------------------------------------------------------------------------
# unsafe function without stack protection canary
unsafe.s: stackprotector.c
	$(CC) -fno-stack-protector -Wall -Wextra -S -o $@ $<

# using gcc-provided canary
safe.s: stackprotector.c
	$(CC) -fstack-protector -Wall -Wextra -S -o $@ $<

# using custom canary
safe_simple.s: stackprotector.c
	$(CC) -fno-stack-protector -DSIMPLE_CANARY=1 -Wall -Wextra -S -o $@ $<
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# assemble and link
# -----------------------------------------------------------------------------
%: %.s
	$(ASM) -Wall -Wextra -o $@ $<
# -----------------------------------------------------------------------------
