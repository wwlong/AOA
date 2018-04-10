/**
 *
 * filename: usbacc.c
 * brief: Use libusb to emulate usb android open accesory
 *
 */

#include <stdio.h>
#include <libusb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "raspberry_gpio_op.h"

#define EP_IN 0x81  
#define EP_OUT 0x02

/* Android device vendor/product */
//#define AD_VID 0x2717 
//#define PID 0xff48

static unsigned int AD_VID = 0x12d1; //0x0bb4;
static unsigned int PID = 0x107e; //0x0dc4;
// #define AD_VID 0x12d1 
// #define PID 0x107e

#define GOOGLE_VID 0x18d1
#define ACCESSORY_PID 0x2d01 /* accessory with adb */
#define ACCESSORY_PID_ALT 0x2d00 /* accessory without adb */

#define ACCESSORY_GET_PROTOCOL  51
#define ACCESSORY_SEND_STRING   52
#define ACCESSORY_START         53
/*64 bytes for USB full-speed accessories
512 bytes for USB high-speed accessories
The Android accessory protocol supports packet buffers up to 16384 bytes*/
#define AOA_BUFF_MAX 16384
#define LEN 2
#define LIGHT_LED 17 
#define PIR_GPIO 4

int init(void);
int deInit(void);
int setupAccessory(void);
int usbSendCtrl(char *buff, int req, int index);
void error(int code);
void status(int code);
void *usbRWHdlr(void * threadarg);

struct libusb_device_handle* handle;
char stop;
char success = 0;

//struct usbAccessory {
//	char* manufacturer;
//	char* modelName;
//	char* version;
//};

struct usbAccessory {
    char* manufacturer;
    char* modelName;
    char* description;
    char* version;
    char* uri;
    char* serialNumber;
};

struct usbAccessory gadgetAccessory = {
    "deepglint",    //manufacture
    "dgaoa",    //model
    "AOA Demo",
    "0.1",  //version 
    "www.deepglint.com",
    "0123456789"
};
/*
 *  创建一个线程进行GPIO的点亮操作
 *  拉高拉低raspberry GPIO11
 * */
sem_t sem_gpio_high; //通知拉高GPIO
sem_t sem_gpio_low; //通知拉低GPIO
void  *thread_gpio_high(void *args) {
    while(1) {
        pthread_detach(pthread_self());
        sem_wait(&sem_gpio_high);
        if(-1 == GPIORead(LIGHT_LED)) {
            GPIOExport(LIGHT_LED);
            GPIODirection(LIGHT_LED, OUT);
            GPIOWrite(LIGHT_LED, HIGH);
        }
        else if(0 == GPIORead(LIGHT_LED)) {
            GPIOWrite(LIGHT_LED, HIGH);
        }
        else {
            printf("already on\n");
        }
    }
}

void *thread_gpio_low(void *args) {
    pthread_detach(pthread_self());
    while(1) {
        sem_wait(&sem_gpio_low);
        if(-1 != GPIORead(LIGHT_LED)) {
            GPIOUnexport(LIGHT_LED);
        }
//      GPIOExport(LIGHT_LED);
//      GPIODirection(LIGHT_LED, OUT);
//      GPIOWrite(LIGHT_LED, LOW);

    }
}

/**
* @brief thread_pir_report 
*       轮训PIR的信号引脚电平信号,如果是低电平,继续轮询,如果是高电平,通过USB将信号传递到Android设备,告知有人靠近
* @param 
*       void *args ,没有用上
* @retval 
*   无
*   {"PIR":"TRIG"}
*/
void *thread_pir_report(void *args) {
    pthread_detach(pthread_self());
    int response, transferred;
    char *cmd = "{\"PIR\":\"TRIG_ON\"}";
    /*
     *  初始化PIR使用的GPIO -- GPIO4
     *  direction -- in 
     * */
    GPIOExport(PIR_GPIO);
    GPIODirection(PIR_GPIO, IN);
    /*
     *  轮询PIR_GPIO的电平
     * */
    while(1) {
        int pir_gpio_value = GPIORead(PIR_GPIO);
        if(1 == pir_gpio_value) {
            printf("pir triged\n");
            response =
                libusb_bulk_transfer(handle, EP_OUT, cmd, strlen(cmd), &transferred, 0);
            if (response < 0) {
                error(response);
                return NULL;
            }
            sleep(5);
        }
        else {
            usleep(500*1000);
        }
    }
}
void usage(void) {
    printf("./usbacc 2717:ff48\n");
    printf("or\n ./usbacc 2717 ff48\n");
}
int main(int argc, char *argv[])
{
	pthread_t tid;
    if(argc < 2) {
        usage();
        return -1;
    }
    if (argc == 2) {
        sscanf(argv[1], "%4x:%4x", &AD_VID, &PID);
    }
    if (argc == 3) {
        sscanf(argv[1], "%4x", &AD_VID);
        sscanf(argv[2], "%4x", &PID);
    }

    if (init() < 0) {
		return -1;
        printf("init failed\n");
    }

	if (setupAccessory() < 0) {
		fprintf(stdout, "Error setting up accessory\n");
		deInit();
		return -1;
	}
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;
	pthread_create(&thread1, NULL, thread_gpio_high, NULL);
	pthread_create(&thread2, NULL, thread_gpio_low, NULL);
	pthread_create(&thread3, NULL, thread_pir_report, NULL);
	pthread_create(&tid, NULL, usbRWHdlr, NULL);
	pthread_join(tid, NULL);

	deInit();
	fprintf(stdout, "Done, no errors\n");
	return 0;
}

void *usbRWHdlr(void * threadarg)
{
	int response, transferred;
	int index = 0;
	unsigned char inBuff[AOA_BUFF_MAX] = {0};
    char outBuff[AOA_BUFF_MAX] = {0};
    char *light_off = "0";
	for (;;) {
		response =
			libusb_bulk_transfer(handle, EP_IN, inBuff, sizeof(inBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return NULL;
		}
		fprintf(stdout, "msg: %s\n", inBuff);
		sprintf(outBuff, "ACK: %07d", index++);

        if(0 == memcmp(inBuff, light_off, strlen(light_off))) {
            printf("light off\n");
            sem_post(&sem_gpio_low);
        }
        else {
            sem_post(&sem_gpio_high);
        }
		response =
			libusb_bulk_transfer(handle, EP_OUT, outBuff, strlen(outBuff), &transferred, 0);
		if (response < 0) {
			error(response);
			return NULL;
		}
    }
}

int init()
{
    int ret = 0;
	ret = libusb_init(NULL);
    if (ret < 0) {
        fprintf(stderr, "failed to initialise libusb\n");
        exit(1);
    }
    if ((handle = libusb_open_device_with_vid_pid(NULL, AD_VID, PID)) == NULL) {
        fprintf(stdout, "Problem acquireing handle\n");
        return -1;
    }
	libusb_claim_interface(handle, 0);
	return 0;
}

int deInit()
{
	if (handle != NULL) {
		libusb_release_interface(handle, 0);
		libusb_close(handle);
	}
	libusb_exit(NULL);
	return 0;
}

int usbSendCtrl(char *buff, int req, int index)
{
	int response = 0;

	if (NULL != buff) {
		response =
			libusb_control_transfer(handle, 0x40, req, 0, index, buff, strlen(buff) + 1 , 0);
	} else {
		response =
			libusb_control_transfer(handle, 0x40, req, 0, index, buff, 0, 0);
	}

	if (response < 0) {
		error(response);
		return -1;
	}	
}

int setupAccessory()
{
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
	fprintf(stdout,"Verion Code Device: %d \n", devVersion);
	
	usleep(1000);//sometimes hangs on the next transfer :(

    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,0,gadgetAccessory.manufacturer,strlen(gadgetAccessory.manufacturer),0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,1,gadgetAccessory.modelName,strlen(gadgetAccessory.modelName)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,2,gadgetAccessory.description,strlen(gadgetAccessory.description)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,3,gadgetAccessory.version,strlen(gadgetAccessory.version)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,4,gadgetAccessory.uri,strlen(gadgetAccessory.uri)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_SEND_STRING,0,5,gadgetAccessory.serialNumber,strlen(gadgetAccessory.serialNumber)+1,0);
    if(response < 0){error(response);return -1;}
    response = libusb_control_transfer(handle,LIBUSB_REQUEST_TYPE_VENDOR,ACCESSORY_START,0,0,NULL,0,0);
    if(response < 0){error(response);return -1;}
	fprintf(stdout,"Accessory Identification sent\n");

	if (usbSendCtrl(NULL, 53, 0) < 0) {
		return -1;
	}

	fprintf(stdout,"Attempted to put device into accessory mode\n");

	if (handle != NULL) {
		libusb_release_interface (handle, 0);
	}

//    int ret = 0;
//	ret = libusb_init(NULL);
//    if (ret < 0) {
//        fprintf(stderr, "failed to initialise libusb\n");
//        exit(1);
//    }
	for (;;) {
		tries--;
		if ((handle = libusb_open_device_with_vid_pid(NULL, GOOGLE_VID, ACCESSORY_PID)) == NULL) {
			if (tries < 0) {
                printf("libusb_open_device_with_vid_pid failed\n");
				return -1;
			}
		} else {
			break;
		}
		sleep(1);
	}

	libusb_claim_interface(handle, 0);
	fprintf(stdout, "Interface claimed, ready to transfer data\n");
	return 0;
}

void error(int code)
{
	fprintf(stdout,"\n");
	switch (code) {
	case LIBUSB_ERROR_IO:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_IO\n"
			"Input/output error.\n");
		break;
	case LIBUSB_ERROR_INVALID_PARAM:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_INVALID_PARAM\n"
			"Invalid parameter.\n");
		break;
	case LIBUSB_ERROR_ACCESS:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_ACCESS\n"
			"Access denied (insufficient permissions).\n");
		break;
	case LIBUSB_ERROR_NO_DEVICE:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_NO_DEVICE\n"
			"No such device (it may have been disconnected).\n");
		break;
	case LIBUSB_ERROR_NOT_FOUND:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_NOT_FOUND\n"
			"Entity not found.\n");
		break;
	case LIBUSB_ERROR_BUSY:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_BUSY\n"
			"Resource busy.\n");
		break;
	case LIBUSB_ERROR_TIMEOUT:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_TIMEOUT\n"
			"Operation timed out.\n");
		break;
	case LIBUSB_ERROR_OVERFLOW:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_OVERFLOW\n"
			"Overflow.\n");
		break;
	case LIBUSB_ERROR_PIPE:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_PIPE\n"
			"Pipe error.\n");
		break;
	case LIBUSB_ERROR_INTERRUPTED:
		fprintf(stdout,
			"Error:LIBUSB_ERROR_INTERRUPTED\n"
			"System call interrupted (perhaps due to signal).\n");
		break;
	case LIBUSB_ERROR_NO_MEM:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_NO_MEM\n"
			"Insufficient memory.\n");
		break;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_NOT_SUPPORTED\n"
			"Operation not supported or unimplemented on this platform.\n");
		break;
	case LIBUSB_ERROR_OTHER:
		fprintf(stdout,
			"Error: LIBUSB_ERROR_OTHER\n"
			"Other error.\n");
		break;
	default:
		fprintf(stdout,
			"Error: unkown error\n");
	}
}

void status(int code)
{
	fprintf(stdout,"\n");
	switch (code) {
		case LIBUSB_TRANSFER_COMPLETED:
			fprintf(stdout,
				"Success: LIBUSB_TRANSFER_COMPLETED\n"
				"Transfer completed.\n");
			break;
		case LIBUSB_TRANSFER_ERROR:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_ERROR\n"
				"Transfer failed.\n");
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_TIMED_OUT\n"
				"Transfer timed out.\n");
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_CANCELLED\n"
				"Transfer was cancelled.\n");
			break;
		case LIBUSB_TRANSFER_STALL:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_STALL\n"
				"For bulk/interrupt endpoints: halt condition detected.\n"
				"For control endpoints: control request not supported.\n");
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_NO_DEVICE\n"
				"Device was disconnected.\n");
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			fprintf(stdout,
				"Error: LIBUSB_TRANSFER_OVERFLOW\n"
				"Device sent more data than requested.\n");
			break;
		default:
			fprintf(stdout,
				"Error: unknown error\nTry again(?)\n");
			break;
	}
}
