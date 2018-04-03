all: simplectrl

simplectrl: simplectrl.c
	gcc $^ -I/usr/include/ -o $@ -lusb-1.0 -I/usr/include/ -I/usr/include/libusb-1.0

clean:
	rm simplectrl
