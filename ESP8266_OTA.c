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
* REFERENCES
* ------------
*		(1) rBoot
*				https://github.com/raburton/rboot
*				https://richard.burtons.org/2015/05/18/rboot-a-new-boot-loader-for-esp8266/
*				https://richard.burtons.org/2015/05/23/rboot-tutorial-for-esp8266-ota-updates/
****************************************************************/

#include "ESP8266_OTA.h"

//LOCAL LIBRARY VARIABLES/////////////////////////////////////
//DEBUG RELATED
static uint8_t _esp8266_ota_debug;

//OTA SERVER IP RELATED
static ip_addr_t _esp8266_ota_server_ip;
static uint16_t _esp8266_ota_server_port;
static char* _esp8266_ota_server_path;

//CALLBACK FUNCTION POINTERS
static void (*_esp8266_ota_done_user_cb)(bool);
//END LOCAL LIBRARY VARIABLES/////////////////////////////////

//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_OTA_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)

    _esp8266_ota_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_OTA_Initialize(ip_addr_t server_ip, uint16_t server_port, char* server_path)
{
    //INITIALIZE ESP8266 OTA PARAMETERS

    _esp8266_ota_server_ip = server_ip;
    _esp8266_ota_server_port = server_port;
    _esp8266_ota_server_path = server_path;

    if(_esp8266_ota_debug)
	{
		os_printf("ESP8266 : OTA : Initialized ota with specified!\n");
		os_printf("ESP8266 : OTA : server ip = %d.%d.%d.%d\n", IP2STR(&_esp8266_ota_server_ip));
		os_printf("ESP8266 : OTA : server port = %u\n", _esp8266_ota_server_port);
		os_printf("ESP8266 : OTA : server path = %s\n", _esp8266_ota_server_path);
	}
}

void ICACHE_FLASH_ATTR ESP8266_OTA_SetCallbackFunctions(void (*ota_done_user_cb)(bool))
{
    //SET USER CB FUNCTION

    if(ota_done_user_cb != NULL)
    {
        _esp8266_ota_done_user_cb = ota_done_user_cb;
        if(_esp8266_ota_debug)
        {
            os_printf("ESP8266 : OTA : User cb function set!\n");
        }
    }
}