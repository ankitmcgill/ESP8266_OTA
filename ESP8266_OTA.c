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
*   MODULES		:= $(MODULES) user/libs/ESP8266_OTA/rboot-sample
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
static uint8_t* _esp8266_ota_server_ip;
static uint16_t _esp8266_ota_server_port;
static char* _esp8266_ota_server_path;

//CALLBACK FUNCTION POINTERS
//END LOCAL LIBRARY VARIABLES/////////////////////////////////

//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_OTA_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)

    _esp8266_ota_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_OTA_Initialize(uint8_t* server_ip, uint16_t server_port, char* server_path)
{
    //INITIALIZE ESP8266 OTA PARAMETERS

    os_printf("ESP8266 : OTA : To set ota server parameters, edit rboot-ota.h\n");
}

void ICACHE_FLASH_ATTR ESP8266_OTA_Start(void)
{
    //START THE OTA PROCESS
    
    os_printf("ESP8266 : OTA : Starting...\n");
    rboot_ota_start(&_esp8266_ota_done_cb);
}

void ICACHE_FLASH_ATTR _esp8266_ota_done_cb(bool result, uint8_t rom_slot)
{
    //RBOOT OTA CB FUNCTION
    char message[40];

    if(result)
    {
        os_sprintf("ESP8266 : OTA : Firmware updated. rebooting from rom %u\n", rom_slot);
        os_printf(message);
        rboot_set_current_rom(rom_slot);
        system_restart();
    }
    else
    {
        os_sprintf("ESP8266 : OTA : Firmware update failed !\n");
        os_printf(message);
    }
    
}