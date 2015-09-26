
/*****************************************************************************/
/*                              Legal                                        */
/*****************************************************************************/

/*
** Copyright �2015. Lantronix, Inc. All Rights Reserved.
** By using this software, you are agreeing to the terms of the Software
** Development Kit (SDK) License Agreement included in the distribution package
** for this software (the �License Agreement�).
** Under the License Agreement, this software may be used solely to create
** custom applications for use on the Lantronix xPico Wi-Fi product.
** THIS SOFTWARE AND ANY ACCOMPANYING DOCUMENTATION IS PROVIDED "AS IS".
** LANTRONIX SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
** TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT AND FITNESS
** FOR A PARTICULAR PURPOSE.
** LANTRONIX HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
** ENHANCEMENTS, OR MODIFICATIONS TO THIS SOFTWARE.
** IN NO EVENT SHALL LANTRONIX BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
** SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
** ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
** LANTRONIX HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*****************************************************************************/
/*                             Includes                                      */
/*****************************************************************************/

#include "vc0706_module_defs.h" /* Automatically generated by make. */
#include "ltrx_compile_defines.h" /* Delivered with SDK. */
#include "ltrx_line.h"		/* Delivered with SDK. */
#include "main_module_libs.h" /* Delivered with SDK. */
#include "string.h"

/*****************************************************************************/
/*                              Defines                                      */
/*****************************************************************************/

#define EXPECTED_MAIN_INTERFACE_VERSION 1

/*****************************************************************************/
/*                            Prototypes                                     */
/*****************************************************************************/
void sendCommand(char cmd, char* args, uint8_t argn);
bool cameraBuffCtrl(char command);
static bool myCBfunc(struct ltrx_http_client *client);
uint32_t getFrameLength(void);
uint8_t* readPicture(uint8_t n);
bool setImageSize(uint8_t size);
bool resizeCB(struct ltrx_http_client *client);

/*****************************************************************************/
/*                         Local Constants                                   */
/*****************************************************************************/


/*****************************************************************************/
/*                         Local Variables                                   */
/*****************************************************************************/


/*****************************************************************************/
/*                              Globals                                      */
/*****************************************************************************/

const struct main_external_functions *g_mainExternalFunctionEntry_pointer = 0;
uint16_t frameptr = 0;

/*****************************************************************************/
/*                               Code                                        */
/*****************************************************************************/


static const struct ltrx_http_dynamic_callback cb = {
	.uriPath  = "/camera.jpg",
	.callback = myCBfunc
};

static const struct ltrx_http_dynamic_callback cb1 = {
	.uriPath = "/resize1",
	.callback = resizeCB
};

static const char* http_header = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n";


void sendCommand(char cmd, char* args, uint8_t argn) {
	char head[] = {0x56,0x0};
	ltrx_line_write(0,&head,2, NULL);
	ltrx_line_write(0,&cmd,1, NULL);
	ltrx_line_write(0,&args[0],argn,NULL);
}
bool setImageSize(uint8_t size) {
	char args[] = {0x05,0x04, 0x01, 0x00, 0x19, 0x22};
	uint8_t *buf;

	if (size == 2) {
		args[5] = 0x11;
	} else if (size == 3) {
		args[5] = 0x0;
	}
	sendCommand(0x31, args, sizeof(args));
	if ( 	(ltrx_line_read(0,&buf,5,NULL,200) != 5) ||
			(buf[0] != 0x76) || 
			(buf[1] != 0x00) || 
			(buf[2] != 0x31) ||
			(buf[3] != 0x0) ) 
				return false;
	return true;
}
bool cameraBuffCtrl(char command) {
	char args[] = {0x1,command};
	uint8_t *buf;
	sendCommand(0x36, args, sizeof(args));
	if ( 	(ltrx_line_read(0,&buf,5,NULL,200) != 5) ||
			(buf[0] != 0x76) || 
			(buf[1] != 0x00) || 
			(buf[2] != 0x36) ||
			(buf[3] != 0x0) ) 
				return false;
	return true;
}
uint32_t getFrameLength() {
	char args[] = {0x1, 0x00};
	uint8_t *camerabuff;
	
	sendCommand(0x34, args, sizeof(args));
	ltrx_line_read(0,&camerabuff,9, NULL, 2000);	
	
	uint32_t len;
	len = camerabuff[5];
	len <<= 8;
	len |= camerabuff[6];
	len <<= 8;
	len |= camerabuff[7];
	len <<= 8;
	len |= camerabuff[8];

	return len;
}
uint8_t* readPicture(uint8_t n) {
	uint8_t *camerabuff;
	char args[] = {0x0C, 0x0, 0x0A, 
                    0, 0, frameptr >> 8, frameptr & 0xFF, 
                    0, 0, 0, n, 
                    0, 0x10};

	while (true) {
		sendCommand(0x32, args, sizeof(args));
		ltrx_line_read(0,&camerabuff,5,NULL, 2000);
		if (camerabuff[3] != 0x0) {
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Frame error");
			ltrx_thread_sleep(100);
			ltrx_line_purge(0);
			continue;
		}
		if ((ltrx_line_read(0,&camerabuff,n+5,NULL, 2000) != n+5)) {
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Could not read buffer");
			ltrx_thread_sleep(100);
			ltrx_line_purge(0);
			continue;
		}
		break;
	};
	frameptr+=n;
	return camerabuff;
}
bool resizeCB(struct ltrx_http_client *client) {
	struct ltrx_ip_socket *socket = ltrx_http_get_socket(client);
	if (ltrx_line_open(0,1000)) {
		if (!setImageSize(2)) {
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"Couldn't set image resolution");
			ltrx_line_close(0);
			return true;
		}
	}
	ltrx_line_close(0);
	ltrx_ip_socket_send(socket, "Hello",6, true);
	return true;
}
bool myCBfunc(struct ltrx_http_client *client)
{
	char buf[200];
	struct ltrx_ip_socket *socket = ltrx_http_get_socket(client);
	if (ltrx_line_open(0,1000))
	{
		TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "Opened Line 1"
        );
		if (!cameraBuffCtrl(0x0)) {		// Stop the Frame Buffer
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"Couldn't stop buffer");
			ltrx_line_close(0);
			return true;
		}
		uint32_t jpglen = getFrameLength();
		TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"Buffer size: %d", jpglen);
		snprintf(buf, 200, http_header,jpglen);
		ltrx_ip_socket_send(socket, buf, strlen(buf), false);
		frameptr = 0;
		while (jpglen > 0) {
			uint8_t *buffer;
			uint8_t bytesToRead = 32;
			if (jpglen < 32) { bytesToRead = jpglen; }
			buffer = readPicture(bytesToRead);
			ltrx_tcp_socket_send(socket, buffer, bytesToRead, true);
			jpglen -= bytesToRead;
		}
		cameraBuffCtrl(0x3);	// Restart the frame buffer
	}
	ltrx_line_close(0);
	TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "Closed Line 1"
        );
	return true;
}

void vc0706_module_initialization(const struct main_external_functions *mef)
{
	if(
		mef &&
		mef->current_interface_version >= EXPECTED_MAIN_INTERFACE_VERSION &&
        mef->backward_compatible_down_to_version <= EXPECTED_MAIN_INTERFACE_VERSION
    )
    {
        g_mainExternalFunctionEntry_pointer = mef;
		ltrx_http_dynamic_callback_register(&cb);
		ltrx_http_dynamic_callback_register(&cb1);
    }
}