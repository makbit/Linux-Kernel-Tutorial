CC = gcc
CFLAGS = -Wall -Werror -Wextra 
# CFLAGS = -Wall -Werror -Wextra -DDEBUG -g
# CONFIG_DEBUG_INFO=n
# CONFIG_DEBUG_FS=n
CONFIG_DEBUG_KERNEL=n
CONFIG_MODULE_SIG=n
obj-m += test2.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
