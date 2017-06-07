
# Makefile for  mcp3008 module driver
KER_DIR_4_9 = /home/prityaa/documents/workspace/embeded/raspbery_pi/linux_source/codebase/source_codes/linux_4.9
CFLAGS_aartyaa_lcd.o := -DDEBUG
obj-m += aartyaa_mcp3008.o
CROSS_COMPILE=arm-linux-gnueabihf-
RPI_COMPILE_OPTION = ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)

all : 
	make $(RPI_COMPILE_OPTION) -C $(KER_DIR_4_9) M=$(PWD) modules 
clean:	
	make $(RPI_COMPILE_OPTION) -C $(KER_DIR_4_9) M=$(PWD) modules clean


