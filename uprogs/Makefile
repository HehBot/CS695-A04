FSROOT = fsroot
CFLAGS += -I..
ASFLAGS += -I..

UPROGS_FULL_PATH = $(UPROGS:%=$(FSROOT)/%)

ULIB = ulib.a

all: $(UPROGS_FULL_PATH)

$(FSROOT)/forktest: forktest.o ulib.o usys.o
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS)  -N -e main -Ttext 0 -o $@ $^

$(FSROOT)/%: %.o $(ULIB)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$(ULIB): ulib.o usys.o printf.o umalloc.o
	$(AR) -rcs $@ $^

clean:
	$(RM) *.a *.o *.asm *.sym
	$(RM) -r $(FSROOT)

.PHONY: all clean
.PRECIOUS: %.o
