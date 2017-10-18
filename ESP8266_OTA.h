/****************************************************************
* ESP8266 OTA UPDATE LIBRARY
*
* REQUIRES rBoot
*
* OCTOBER 11 2017
*
* ANKIT BHATNAGAR
* ANKIT.BHATNAGARINDIA@GMAIL.COM
*
* FOR A SUCCESSFULL COMPILATION, NEED TO ADD THE FOLLOWING MODULES TO THE MAKEFILE
*   MODULES		:= $(MODULES) user/libs/ESP8266_OTA/rboot
*   MODULES		:= $(MODULES) user/libs/ESP8266_OTA/rboot/appcode
*   MODULES		:= $(MODULES) user/libs/ESP8266_OTA/rboot/build
*
* TO CHANGE THE SERVER PARAMETERS (IP / PORT / FILENAME), EDIT 
* ./rboot-sample/rboot-ota.h
*
* NOTE : THE CODE FOR THE ACTUAL TCP HANDLING OF DATA, BURNING TO FLASH ETC IS COPIED
*        VERBATIM FROM THE REPO rboot-sample
*
*        ONCE YOU FLASH THE INITIAL FIRMWARE THROUGH SERIAL, MAKE SURE TO POWER RECYCLE
*        THE BOARD. IF NOT DONE, ANY FUTURE OTA WILL UPGRADE THE FIRMWARE BUT WILL GIVE
*        A wdt reset WHEN SOFT RESTARTING THE UNIT. AT THAT POINT YOU WOULD NEED TO POWER
*        RECYCLE THE UNIT FOR THE NEW FIRMWARE TO LOAD. TO AVOID THIS, USE ABOVE
*
* REFERENCES
* ------------
*		(1) rBoot
*				https://github.com/raburton/rboot
*				https://richard.burtons.org/2015/05/18/rboot-a-new-boot-loader-for-esp8266/
*				https://richard.burtons.org/2015/05/23/rboot-tutorial-for-esp8266-ota-updates/
****************************************************************/

#ifndef _ESP8266_OTA_H_
#define _ESP8266_OTA_H_

#include "osapi.h"
#include "ets_sys.h"
#include "espconn.h"
#include "os_type.h"
#include "rboot-api.h"

//USER FIRMWARE VERSION NUMBER
#define ESP8266_OTA_USER_FW_VERSION_MAJ 1
#define ESP8266_OTA_USER_FW_VERSION_MIN 0

#define ESP8266_VERSION_FILENAME    "app.ver"

#define ESP8266_OTA_HTTP_HEADER     "Connection: keep-alive\r\n"\
                                    "Cache-Control: no-cache\r\n"\
                                    "User-Agent: rBoot-Sample/1.0\r\n"\
                                    "Accept: */*\r\n\r\n"
#define ESP8266_OTA_HTTP_STRING     "GET %s%s HTTP/1.1\r\nHost: \"%s\"\r\n"

#define ESP8266_OTA_UPGRADE_FLAG_IDLE		0x00
#define ESP8266_OTA_UPGRADE_FLAG_START		0x01
#define ESP8266_OTA_UPGRADE_FLAG_FINISH		0x02

#define ESP8266_OTA_FILE "file.bin"

//TIMEOUT FOR INITIAL CONNECT & FIR EACH RCV
#define ESP8266_OTA_NETWORK_TIMEOUT_MS  10000

// USED TO INDICATE NON ROM FLASH
#define ESP8266_OTA_FLASH_BY_ADDR       0xFF

//CUSTOM VARIABLE STRUCTURES/////////////////////////////
//END CUSTOM VARIABLE STRUCTURES/////////////////////////
//USER CB FUNTION FORMAT TYPEDEF
typedef void (*ESP8266_OTA_CALLBACK)(bool result, uint8 rom_slot);

typedef enum
{
    ESP8266_OTA_SERVER_OPERATION_GET_FILE_VERSION=0,
    ESP8266_OTA_SERVER_OPERATION_GET_FILE_FW
} ESP8266_OTA_OPERATION;

typedef struct {
	uint8 rom_slot;   // rom slot to update, or FLASH_BY_ADDR
	ESP8266_OTA_CALLBACK callback;  // user callback when completed
	uint32 total_len;
	uint32 content_len;
	struct espconn *conn;
	ip_addr_t ip;
	rboot_write_status write_status;
} ESP8266_OTA_UPGRADE_STATUS;

//FUNCTION PROTOTYPES/////////////////////////////////////
//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_OTA_SetDebug(uint8_t debug_on);
void ICACHE_FLASH_ATTR ESP8266_OTA_Initialize(char* server, 
                                                uint16_t server_port, 
                                                char* server_path,
                                                char* name_rom0,
                                                char* name_rom1);
//CONTROL FUNCTIONS
bool ICACHE_FLASH_ATTR ESP8266_OTA_Start();
//END FUNCTION PROTOTYPES/////////////////////////////////
#endif
