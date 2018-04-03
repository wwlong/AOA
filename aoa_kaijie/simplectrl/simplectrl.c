/*
 * simplectrl.c
 * This file is part of OsciPrime
 *
 * Copyright (C) 2011 - Manuel Di Cerbo
 *
 * OsciPrime is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * OsciPrime is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OsciPrime; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <ctype.h>
#include <stdio.h>
#include <libusb.h>
#include <string.h>
#include <unistd.h>

#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>


/*This should be read by libusb, not by hardcode.*/
#define EP_IN 0x81
#define EP_OUT 0x02
#define AOA_BUFF_MAX 1024
/* Android device vendor/product */
/*#define VID 0x0bb4*/
/*#define PID 0x0f63*/
static unsigned int VID = 0x12d1; //0x0bb4;
static unsigned int PID = 0x107e; //0x0dc4;
static unsigned int interfaceId = 1;

#define ACCESSORY_VID 0x18D1

// If fail to connect to ACCESSORY_PID_NO_ADB, try ACCESSORY_PID_ADB
#define ACCESSORY_PID_NO_ADB 0x2D00  /* accessory without adb */
#define ACCESSORY_PID_ADB 0x2D01  /* accessory with adb */
#define ACCESSORY_PID ACCESSORY_PID_ADB

#define LEN 2

/* from android kerenel: include/linux/usb/f_accessory.h */
#define ACCESSORY_GET_PROTOCOL  51
#define ACCESSORY_SEND_STRING   52
#define ACCESSORY_START         53

/*
If you are on Ubuntu you will require libusb as well as the headers...
We installed the headers with "apt-get source libusb"
gcc simplectrl.c -I/usr/include/ -o simplectrl -lusb-1.0 -I/usr/include/ -I/usr/include/libusb-1.0

Tested for Nexus S with Gingerbread 2.3.4
*/

static int mainPhase();
static int init(void);
static int deInit(void);
static void error(int code);
static void status(int code);
static int setupAccessory(
        const char* manufacturer,
        const char* modelName,
        const char* description,
        const char* version,
        const char* uri,
        const char* serialNumber);

//static
static struct libusb_device_handle* handle;
static char stop;
static char success = 0;

int main (int argc, char *argv[]){
    //return moveMouse(argc, argv);
    if (argc == 2) {
        sscanf(argv[1], "%4x:%4x", &VID, &PID);
    }
    if (argc == 3) {
        sscanf(argv[1], "%4x", &VID);
        sscanf(argv[2], "%4x", &PID);
    }

    handle = NULL;
    int ret = init();
    if (ret < 0) {
        if (ret == -2)
            goto main_phase;
        return 0;
    }
    //doTransfer();
    if(setupAccessory(
                "Deepglint",
                "dgaoa",
                "Simple AOA Test",
                "0.1",
                "http://www.deepglint.com",
                "1234567890") < 0){
        fprintf(stdout, "Error setting up accessory\n");
        deInit();
        return -1;
    };

main_phase:
    if(mainPhase() < 0){
        fprintf(stdout, "Error during main phase\n");
        deInit();
        return -1;
    }
    deInit();
    fprintf(stdout, "Done, no errors\n");
    return 0;
}

static int mainPhase() {
    fprintf(stdout, "Enter mainPhase()\n");
    unsigned char buffer[500000];
    int response = 0;
    static int transferred;
    int i;
    int length = 0;
    union {
        char c[4];
        int i;
    } udata;

    // receive length and data number;
    
	unsigned int index = 0;
	unsigned char inBuff[AOA_BUFF_MAX] = {0};
        unsigned char outBuff[AOA_BUFF_MAX] = {0};

	for (;;) {
        //接收长度
        memset(inBuff, 0, sizeof(inBuff));
		response =
			libusb_bulk_transfer(handle, EP_IN, inBuff, sizeof(inBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return -1;
		}
		fprintf(stdout, "msg: %s\n", inBuff);
        //接收数据
        memset(inBuff, 0, sizeof(inBuff));
		response =
			libusb_bulk_transfer(handle, EP_IN, inBuff, sizeof(inBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return -1;
		}
		fprintf(stdout, "msg: %s\n", inBuff);
    }
 //   memset(buffer, 0, 500000);
 //   response = libusb_bulk_transfer(handle,IN,buffer,16384, &transferred,0);
 //   if(response < 0){error(response);return -1;}
 //   buffer[transferred] = 0;
 //   length = atoi(buffer);

 //   memset(buffer, 0, 500000);
 //   response = libusb_bulk_transfer(handle,IN,buffer,500000, &transferred,0);
 //   if(response < 0){error(response);return -1;}
 //   if (transferred != length)
 //       fprintf(stdout, "\ndata size not equal\n");
 //   fprintf(stdout, "data is: ");
 //   for (i = 0; i < transferred; i++) {
 //       unsigned char num = buffer[i];
 //       if (isdigit(num))
 //           putchar(num);
 //       else
 //           break;
 //   }
    putchar('\n');
}

static int getInterfaceNumber(struct libusb_device_handle * handle) {
    struct libusb_device * dev;
    struct libusb_device_descriptor deviceDesc;
    struct libusb_config_descriptor * configDesc;
    const struct libusb_interface *interface;
    const char strAndroidAccessoryInterface[] = "Android Accessory Interface";
    char interfaceString[255];
    int foundConfig = 0;
    int accessoryInterfaceNumber = -1;
    int i, j, k;

    fprintf(stdout, "find interface with %s:\n", strAndroidAccessoryInterface);

    dev = libusb_get_device(handle);
    if (dev == NULL) {
        fprintf(stdout, "Problem get libusb_device\n");
        return -1;
    }
    if (libusb_get_device_descriptor(dev, &deviceDesc) < 0) {
        fprintf(stdout, "Problem get libusb_device_descriptor\n");
        return -1;
    }
    fprintf(stdout, "bNumConfigurations %d\n", deviceDesc.bNumConfigurations);
    for (i = 0; i < deviceDesc.bNumConfigurations; i++) {
        //printf("%d:\n", i);
        if (libusb_get_config_descriptor(dev, i, &configDesc) < 0) {
            fprintf(stdout, "Problem get libusb_config_descriptor\n");
            return -1;
        }
        for (j = 0; j < configDesc->bNumInterfaces; j++) {
            //printf("%d:%d:\n", i, j);
            interface = &(configDesc->interface[j]);
            for (k = 0; k < interface->num_altsetting; k++) {
                //printf("%d:%d:%d:\n", i, j, k);
                int len = libusb_get_string_descriptor_ascii(
                            handle, interface->altsetting[k].iInterface,
                            interfaceString, sizeof(interfaceString) - 1);
                if (len < 0)
                    return -1;
                interfaceString[len] = 0;
                if ((len == sizeof(strAndroidAccessoryInterface) - 1) &&
                        !strcmp(strAndroidAccessoryInterface, interfaceString)) {
                    foundConfig = 1;
                    accessoryInterfaceNumber = interface->altsetting[k].bInterfaceNumber;
                    goto found;
                }
            }
        }
        libusb_free_config_descriptor(configDesc);
    }
found:
    if (foundConfig == 0)
        return -1;
    libusb_free_config_descriptor(configDesc);
    return accessoryInterfaceNumber;
}


static int init() {
    libusb_init(NULL);
    if ((fprintf(stdout, "Try open device\n"), handle = libusb_open_device_with_vid_pid(NULL, VID, PID)) != NULL) {
        if (libusb_kernel_driver_active(handle, interfaceId))
            if (libusb_detach_kernel_driver(handle, interfaceId) != 0) {
                fprintf(stdout, "Problem detach kernel\n");
                return -1;
            }
        libusb_claim_interface(handle, interfaceId);
        return 0;
    } else if ((fprintf(stdout, "Try open accessory\n"), handle = libusb_open_device_with_vid_pid(NULL, ACCESSORY_VID, ACCESSORY_PID)) != NULL) {
        int id = getInterfaceNumber(handle);
        if (id < 0) {
            fprintf(stdout, "Problem search interface\n");
            return -1;
        }
        interfaceId = id;
        printf("interface number id is %d\n", interfaceId);
        if (libusb_kernel_driver_active(handle, interfaceId))
            if (libusb_detach_kernel_driver(handle, interfaceId) != 0) {
                fprintf(stdout, "Problem detach kernel\n");
                return -1;
            }
        fprintf(stdout, "Already in accessory mode\n");
        libusb_claim_interface(handle, interfaceId);
        return -2;
    }
    fprintf(stdout, "Problem acquireing handle\n");
    return -1;
}

static int deInit(){
    //TODO free all transfers individually...
    //if(ctrlTransfer != NULL)
    //	libusb_free_transfer(ctrlTransfer);
    if(handle != NULL)
        libusb_release_interface(handle, interfaceId);
    libusb_exit(NULL);
    return 0;
}

static int setupAccessory(
        const char* manufacturer,
        const char* modelName,
        const char* description,
        const char* version,
        const char* uri,
        const char* serialNumber){

    unsigned char ioBuffer[2];
    int devVersion;
    int response;
    int tries = 5;

    response = libusb_control_transfer(
                handle, //handle
                LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, //bmRequestType
                ACCESSORY_GET_PROTOCOL, //bRequest
                0, //wValue
                0, //wIndex
                ioBuffer, //data
                2, //wLength
                0 //timeout
                );

    if(response < 0){error(response);return-1;}

    devVersion = ioBuffer[1] << 8 | ioBuffer[0];
    fprintf(stdout,"Verion Code Device: %d\n", devVersion);

    usleep(1000);//sometimes hangs on the next transfer :(

    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,0,(char*)manufacturer,strlen(manufacturer),0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,1,(char*)modelName,strlen(modelName)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,2,(char*)description,strlen(description)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,3,(char*)version,strlen(version)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,4,(char*)uri,strlen(uri)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,5,(char*)serialNumber,strlen(serialNumber)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_START,0,0,NULL,0,0);
    if(response < 0){error(response);return -1;}

    fprintf(stdout,"Attempted to put device into accessory mode %d\n", devVersion);

    if(handle != NULL)
        libusb_release_interface(handle, interfaceId);


    for (;;) { //attempt to connect to new PID, if that doesn't work try ACCESSORY_PID_ADB
        tries--;
        if ((handle = libusb_open_device_with_vid_pid(NULL, ACCESSORY_VID, ACCESSORY_PID)) == NULL) {
            if (tries < 0){
                return -1;
            }
        }else{
            break;
        }
        sleep(3);
    }
//    int id = getInterfaceNumber(handle);
//    if (id < 0) {
//        fprintf(stdout, "Problem search interface\n");
//        return -1;
//    }
//    interfaceId = id;
    interfaceId = 0;
    libusb_claim_interface(handle, interfaceId);
    fprintf(stdout, "Interface claimed, ready to transfer data\n");
    return 0;
}

static void error(int code){
    fprintf(stdout,"\n");
    switch(code){
    case LIBUSB_ERROR_IO:
        fprintf(stdout,"Error: LIBUSB_ERROR_IO\nInput/output error.\n");
        break;
    case LIBUSB_ERROR_INVALID_PARAM:
        fprintf(stdout,"Error: LIBUSB_ERROR_INVALID_PARAM\nInvalid parameter.\n");
        break;
    case LIBUSB_ERROR_ACCESS:
        fprintf(stdout,"Error: LIBUSB_ERROR_ACCESS\nAccess denied (insufficient permissions).\n");
        break;
    case LIBUSB_ERROR_NO_DEVICE:
        fprintf(stdout,"Error: LIBUSB_ERROR_NO_DEVICE\nNo such device (it may have been disconnected).\n");
        break;
    case LIBUSB_ERROR_NOT_FOUND:
        fprintf(stdout,"Error: LIBUSB_ERROR_NOT_FOUND\nEntity not found.\n");
        break;
    case LIBUSB_ERROR_BUSY:
        fprintf(stdout,"Error: LIBUSB_ERROR_BUSY\nResource busy.\n");
        break;
    case LIBUSB_ERROR_TIMEOUT:
        fprintf(stdout,"Error: LIBUSB_ERROR_TIMEOUT\nOperation timed out.\n");
        break;
    case LIBUSB_ERROR_OVERFLOW:
        fprintf(stdout,"Error: LIBUSB_ERROR_OVERFLOW\nOverflow.\n");
        break;
    case LIBUSB_ERROR_PIPE:
        fprintf(stdout,"Error: LIBUSB_ERROR_PIPE\nPipe error.\n");
        break;
    case LIBUSB_ERROR_INTERRUPTED:
        fprintf(stdout,"Error:LIBUSB_ERROR_INTERRUPTED\nSystem call interrupted (perhaps due to signal).\n");
        break;
    case LIBUSB_ERROR_NO_MEM:
        fprintf(stdout,"Error: LIBUSB_ERROR_NO_MEM\nInsufficient memory.\n");
        break;
    case LIBUSB_ERROR_NOT_SUPPORTED:
        fprintf(stdout,"Error: LIBUSB_ERROR_NOT_SUPPORTED\nOperation not supported or unimplemented on this platform.\n");
        break;
    case LIBUSB_ERROR_OTHER:
        fprintf(stdout,"Error: LIBUSB_ERROR_OTHER\nOther error.\n");
        break;
    default:
        fprintf(stdout, "Error: unkown error\n");
    }
}

static void status(int code){
    fprintf(stdout,"\n");
    switch(code){
    case LIBUSB_TRANSFER_COMPLETED:
        fprintf(stdout,"Success: LIBUSB_TRANSFER_COMPLETED\nTransfer completed.\n");
        break;
    case LIBUSB_TRANSFER_ERROR:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_ERROR\nTransfer failed.\n");
        break;
    case LIBUSB_TRANSFER_TIMED_OUT:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_TIMED_OUT\nTransfer timed out.\n");
        break;
    case LIBUSB_TRANSFER_CANCELLED:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_CANCELLED\nTransfer was cancelled.\n");
        break;
    case LIBUSB_TRANSFER_STALL:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_STALL\nFor bulk/interrupt endpoints: halt condition detected (endpoint stalled).\nFor control endpoints: control request not supported.\n");
        break;
    case LIBUSB_TRANSFER_NO_DEVICE:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_NO_DEVICE\nDevice was disconnected.\n");
        break;
    case LIBUSB_TRANSFER_OVERFLOW:
        fprintf(stdout,"Error: LIBUSB_TRANSFER_OVERFLOW\nDevice sent more data than requested.\n");
        break;
    default:
        fprintf(stdout,"Error: unknown error\nTry again(?)\n");
        break;
    }
}


int moveMouse(int argc, char** argv) {
    struct input_event event, event_end;
    int i;

    if (argc != 2)
        return -1;
    int fd = open(argv[1], O_RDWR);
    if(!fd){
        printf("Errro open mouse:%s\n", strerror(errno));
        return -1;
    }
    memset(&event, 0, sizeof(event));
    memset(&event, 0, sizeof(event_end));
    gettimeofday(&event.time, NULL);
    event.type = EV_REL;
    event.code = REL_X;
    event.value = 100;
    gettimeofday(&event_end.time, NULL);
    event_end.type = EV_SYN;
    event_end.code = SYN_REPORT;
    event_end.value = 0;
    for(i=0; i<5; i++){
        write(fd, &event, sizeof(event));// Move the mouse
        write(fd, &event_end, sizeof(event_end));// Show move
        sleep(1);// wait
    }
    close(fd);
    return 0;
}
