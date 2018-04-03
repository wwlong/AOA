CC=gcc 
RM=rm 
SRC += ./usbacc.c
SRC += /workspace/github_work/raspberry_test/raspberry_gpio/raspberry_gpio_op_c_api/raspberry_gpio_op.c 

INC += -I/usr/include/ -o usbacc -lusb-1.0 -I/usr/include/ -I/usr/include/libusb-1.0 -I/usr/local/include/libusb-1.0
INC += -I/workspace/github_work/raspberry_test/raspberry_gpio/raspberry_gpio_op_c_api 
LIBS += -lusb-1.0 -lpthread 
CFLAGS += ${INC}
CFLAGS += -g  
all:usbacc
usbacc:${SRC}
	${CC} ${CFLAGS} $^ -o $@ ${LIBS}

.PHONY:clean
clean:
	${RM} usbacc *~ -rf
