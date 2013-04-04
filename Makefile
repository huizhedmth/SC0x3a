# used to build yalnix kernel
# reference: ~cs58/yalnix/sample/Makefile, Sean W. Smith, Dartmouth College

ALL = $(KERNEL_ALL) $(USER_APPS)
KERNEL_ALL = yalnix

KERNEL_SRCS = kernel.c trap.c list.c mem.c loadprogram.c scheduler.c syscall.c

KERNEL_OBJS = kernel.o trap.o list.o mem.o loadprogram.o scheduler.o syscall.o

KERNEL_INCS = kernel.h trap.h list.h mem.h loadprogram.h scheduler.h syscall.h



USER_APPS = 

USER_SRCS = 

USER_OBJS = 

USER_INCS = 

YALNIX_OUTPUT = yalnix

CC = gcc

DDIR58 = /net/class/cs58/yalnix
LIBDIR = $(DDIR58)/lib
INCDIR = $(DDIR58)/include
ETCDIR = $(DDIR58)/etc

LD_EXTRA = 

KERNEL_LIBS = $(LIBDIR)/libkernel.a $(LIBDIR)/libhardware.so

KERNEL_LDFLAGS = $(LD_EXTRA) -L$(LIBDIR) -lkernel -lelf -Wl,-T,$(ETCDIR)/kernel.x -Wl,-R$(LIBDIR) -lhardware 
LINK_KERNEL = $(LINK.c)

USER_LIBS = $(LIBDIR)/libuser.a
ASFLAGS = -D__ASM__
CPPFLAGS= -m32 -fno-builtin -I. -I$(INCDIR) -g -DLINUX 

all: $(ALL)	

clean:
	rm -f *.o *~ TTYLOG* TRACE $(YALNIX_OUTPUT) $(USER_APPS)

count:
	wc $(KERNEL_SRCS) $(USER_SRCS)

list:
	ls -l *.c *.h

kill:
	killall yalnixtty yalnixnet yalnix

$(KERNEL_ALL): $(KERNEL_OBJS) $(KERNEL_LIBS) $(KERNEL_INCS)
	$(LINK_KERNEL) -o $@ $(KERNEL_OBJS) $(KERNEL_LDFLAGS)

$(USER_APPS): $(USER_OBJS) $(USER_INCS)
	$(ETCDIR)/yuserbuild.sh $@ $(DDIR58) $@.o










