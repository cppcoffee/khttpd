TARGET=khttpd
obj-m += $(TARGET).o
$(TARGET)-objs := src/socket.o \
	src/daemon.o \
	src/http_string.o \
	src/http_parser.o \
	src/http_request.o \
	src/http_response.o \
	src/http_chunk.o \
	src/http_directory.o \
	src/utils.o \
	src/khttpd.o
EXTRA_CFLAGS := -O2 -I$(PWD)/include

KERNEL := /lib/modules/$(shell uname -r)/build
KDIR := $(KERNEL)
PWD := $(shell pwd)

all:
	KCPPFLAGS="${KCPPFLAGS}" $(MAKE) -C $(KDIR) M=$(PWD) modules
	rm -fr *.o .*.cmd Module.symvers modules.order $(TARGET).mod.c

clean:
	$(MAKE) -C $(KERNEL) M=$(PWD) clean

