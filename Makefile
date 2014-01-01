CC = 410-gcc
AR = 410-ar
#LDFLAGS = -static (see "-static" below)

# -O0 is needed so that gcc doesn't "helpfully" optimize the
# calling convention for static functions.
CFLAGS = -Wall -Werror -gdwarf-2 -O0 -m32 -fno-stack-protector -fno-omit-frame-pointer -Itraceback/

# If you don't do this, GCC4 will emit code which causes main() to align its
# stack to a 16-byte boundary. This will cause the base pointer for that
# function to point into the middle of some padding space allocated to align the
# stack. GCC4 accesses the arguments to main through an offset stored in %ecx
# instead of %ebp.
# n.b.: -mpreferred-stack-boundary=n aligns the stack to 2^n bytes in all
# functions.
CFLAGS += -mpreferred-stack-boundary=2

DOC = doxygen

include config.mk

TRACEBACK_OBJS = traceback/traceback_globals.o \
				$(MY_TRACEBACK_OBJS:%=traceback/%)
TEST_PROGS := $(TEST_PROGS:%=tests/%)

.PHONY: update query_update html_doc print clean verify veryclean

all: query_update $(TEST_PROGS) tests/add_one_test tests/verify_test

#doupdate:
#	./update.pl $(UPDATE_METHOD)

update:
	./update.sh $(UPDATE_METHOD)

query_update:
	./update.sh $(UPDATE_METHOD) query

%.o: %.c
	@expand -t $(TABSTOP) $< | awk 'length > 80 \
	  { err++; \
	    errs[err] = "Warning: line " NR " in $< is longer than 80 characters"; \
	  } END { \
	    if (err > 2 && err < 1000) {for (i = 1; i <= err; i++) print errs[i]; system("sleep 5");}\
	    exit(err > 10 && err < 1000) }'
	$(CC) -c -o $@ $(CFLAGS) $(LDFLAGS) $^

%.o: %.s
	@echo 'You should use the .S file extension rather than .s'
	@echo '.s does not support preprocessor directives (like #include)'
	@false

%.o: %.S
	$(CC) -c -o $@ -DASSEMBLER $(CFLAGS) $(LDFLAGS) $^

libtraceback.a: $(TRACEBACK_OBJS)
	$(AR) rc libtraceback.a $(TRACEBACK_OBJS)

$(TEST_PROGS) tests/verify_test: %: %.o libtraceback.a
	$(CC) -o $@ $@.o -L. libtraceback.a $(CFLAGS) $(LDFLAGS) $(LIBS) -static
	python ./symtabgen.py $@

tests/add_one_test: tests/add_one.o tests/add_one_test.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

html_doc:
	$(DOC) doxygen.conf

PRINTOUT=traceback.ps

print:
	enscript -2rG -fCourier7 -FCourier-Bold10 -p $(PRINTOUT) \
		$(if $(strip $(TABSTOP)),-T $(TABSTOP),) \
			`find ./traceback/ -type f -regex  '.*\.[chS]' | egrep -v 'contracts.h|traceback_globals|traceback_internal' | sort`

clean:
	rm -rf traceback/*.o tests/*.o \
			libtraceback.a \
			$(TEST_PROGS) tests/add_one_test tests/verify_test \
			*~
	rm -f tests/tmp-our-output tests/tmp-sample-output
	rm -f MAKE.* RUN.*
	rm -f */*.pyc */*/*.pyc */*/*/*.pyc

veryclean: clean

# Compare the first four lines of tests/sample.output
# to the first four lines of output from verify_test.
# What about the rest of the lines?  Take a look at
# tests/sample.output.

FIRSTLINES=sed -n '1,4p'
tests/tmp-our-output tests/tmp-sample-output: tests/verify_test

tests/tmp-sample-output: tests/sample.output
	@$(FIRSTLINES) < $< > $@

tests/tmp-our-output:
	@tests/verify_test | $(FIRSTLINES) >  $@

verify: tests/tmp-our-output tests/tmp-sample-output
	@diff -bBw tests/tmp-sample-output tests/tmp-our-output
