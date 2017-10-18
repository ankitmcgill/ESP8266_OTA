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

#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "ESP8266_OTA.h"

//LOCAL LIBRARY VARIABLES/////////////////////////////////////
//DEBUG RELATED
static uint8_t _esp8266_ota_debug;

//OTA SERVER IP RELATED
static char* _esp8266_ota_server;
static uint16_t _esp8266_ota_server_port;
static char* _esp8266_ota_server_path;
static char* _esp8266_ota_filename_rom0;
static char* _esp8266_ota_filename_rom1;

//TIMER RELATED
static os_timer_t _esp8266_ota_timer;

//UPGRADE RELATED
static ESP8266_OTA_OPERATION _esp8266_ota_current_operation;
static ESP8266_OTA_UPGRADE_STATUS* _esp8266_ota_upgrade;

//rboot RELATED
static void ICACHE_FLASH_ATTR _esp8266_ota_done_cb(bool result, uint8_t rom_slot);
void ICACHE_FLASH_ATTR _esp8266_ota_rboot_ota_deinit();
static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_recvcb(void *arg, char *pusrdata, unsigned short length);
static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_disconcb(void *arg);
static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_connect_cb(void *arg);
static void ICACHE_FLASH_ATTR _esp8266_ota_connect_timeout_cb();
static const char* ICACHE_FLASH_ATTR _esp8266_ota_esp_errstr(int8_t err);
static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_recon_cb(void *arg, int8_t errType);
static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_resolved(const char *name, ip_addr_t *ip, void *arg);
bool ICACHE_FLASH_ATTR _esp8266_ota_rboot_ota_start(ESP8266_OTA_CALLBACK callback);
bool ICACHE_FLASH_ATTR _esp8266_ota_is_server_fw_version_higher(uint8_t server_major, uint8_t server_minor);
//END LOCAL LIBRARY VARIABLES/////////////////////////////////

//CONFIGURATION FUNCTIONS
void ICACHE_FLASH_ATTR ESP8266_OTA_SetDebug(uint8_t debug_on)
{
    //SET DEBUG PRINTF ON(1) OR OFF(0)

    _esp8266_ota_debug = debug_on;
}

void ICACHE_FLASH_ATTR ESP8266_OTA_Initialize(char* server, 
                                                uint16_t server_port, 
                                                char* server_path,
                                                char* name_rom0,
                                                char* name_rom1)
{
    //INITIALIZE ESP8266 OTA PARAMETERS

    _esp8266_ota_server = server;
    _esp8266_ota_server_port = server_port;
    _esp8266_ota_server_path = server_path;
    _esp8266_ota_filename_rom0 = name_rom0;
    _esp8266_ota_filename_rom1 = name_rom1;
    os_printf("ESP8266 : OTA : To set ota server parameters, edit rboot-ota.h\n");
}

bool ICACHE_FLASH_ATTR ESP8266_OTA_Start()
{
    //START THE OTA PROCESS
    
    os_printf("ESP8266 : OTA : Starting...\n");
    if(_esp8266_ota_rboot_ota_start(_esp8266_ota_done_cb))
    {
        os_printf("ESP8266 : OTA : Updating...\n");
    }
    else
    {
        os_printf("ESP8266 : OTA : Updating failed !\n");
    }
}

static void ICACHE_FLASH_ATTR _esp8266_ota_done_cb(bool result, uint8_t rom_slot)
{
    //RBOOT OTA CB FUNCTION
    char* message = (char*)os_zalloc(80);

    if(result)
    {
        os_sprintf(message, "ESP8266 : OTA : Firmware updated. rebooting from rom %u\n", rom_slot);
        os_printf(message);
        rboot_set_current_rom(rom_slot);
        system_restart();
    }
    else
    {
        os_sprintf(message, "ESP8266 : OTA : Firmware update failed !\n");
        os_printf(message);
    }
    os_free(message);
}

void ICACHE_FLASH_ATTR _esp8266_ota_rboot_ota_deinit()
{
    //CLEAN UP AT THE END OF THE UPDARE
    //CALL BACK USER CB FUNCTION TO INDICATE COMPLETION
    
    bool result;
    uint8_t rom_slot;
    ESP8266_OTA_CALLBACK callback;
    struct espconn *conn;

    os_timer_disarm(&_esp8266_ota_timer);
    //SAVE ONLY REMAINING BITS OF INTEREST FROM UPGRADE STRUCT
    //THEN WE CAN CLEAN IT UP EARLY, SO DISCONNECT CALLBACK
    //CAN DISTINGUISH BETWEEN US CALLING IT AFTER UPDATE FINISHED
    //OR BEING CALLED EARLIER IN THE UPDATE PROCESS
    conn = _esp8266_ota_upgrade->conn;
    rom_slot = _esp8266_ota_upgrade->rom_slot;
    callback = _esp8266_ota_upgrade->callback;

    // clean up
    os_free(_esp8266_ota_upgrade);
    _esp8266_ota_upgrade = 0;

    // if connected, disconnect and clean up connection
    if (conn) espconn_disconnect(conn);

    // check for completion
    if (system_upgrade_flag_check() == ESP8266_OTA_UPGRADE_FLAG_FINISH)
    {
        result = true;
    }
    else
    {
        system_upgrade_flag_set(ESP8266_OTA_UPGRADE_FLAG_IDLE);
        result = false;
    }

    // call user call back
    if (callback) {
        callback(result, rom_slot);
    }
}

static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    //CALLED WHEN CONNECTION RECEIVES DATA (HOPEFULLY THE ROM)
    
    char *ptrData, *ptrLen, *ptr;

    //DISARM THE TIMER
    os_timer_disarm(&_esp8266_ota_timer);

    // FIRST REPLY?
    if (_esp8266_ota_upgrade->content_len == 0)
    {
        //VALID HTTP RESPONSE?
        if ((ptrLen = (char*)os_strstr(pusrdata, "Content-Length: ")) &&
            (ptrData = (char*)os_strstr(ptrLen, "\r\n\r\n")) &&
            (os_strncmp(pusrdata + 9, "200", 3) == 0))
        {
            //END OF HEADER/START OF DATA
            ptrData += 4;
            //LENGTH OF DATA AFTER HEADER IN THIS CHUNK
            length -= (ptrData - pusrdata);

            //CHECK WEATHER VERSION DATA OR FIRMWARE DATA
            if(_esp8266_ota_current_operation == ESP8266_OTA_SERVER_OPERATION_GET_FILE_VERSION)
            {
                //VERSION DATA. WILL FIT IN 1 HTTP CHUNK
                //PROCESS IT
                char* ver_data = (char*)os_zalloc(50);
                os_memcpy(ver_data, (uint8_t*)ptrData, length);
                os_printf("ESP8266 : OTA : Version Data : %s\n", ver_data);
                
                //EXTRACT VERSION DATA
                uint32_t version_maj = 0, version_min = 0;
                
                uint8_t counter= 7;
                while(ver_data[counter] != ',')
                {
                    version_maj = (version_maj * 10) + (ver_data[counter]-48);
                    counter++;
                }

                counter = 15;
                while(ver_data[counter] != ',')
                {
                    version_min = (version_min * 10) + (ver_data[counter]-48);
                    counter++;
                }
                
                os_printf("ESP8266 : OTA : Extracted version info : major = %u, minor = %u\n", version_maj, version_min);
                os_printf("ESP8266 : OTA : Running version info : major = %u, minor = %u\n", ESP8266_OTA_USER_FW_VERSION_MAJ, ESP8266_OTA_USER_FW_VERSION_MIN);

                if(_esp8266_ota_is_server_fw_version_higher(version_maj, version_min)==true)
                {
                    //SERVER HAS NEWER FIRMWARE
                    //NEED TO DO OTA
                    os_printf("ESP8266 : OTA : Server FW is newer than current. Proceeding !\n");
                    _esp8266_ota_current_operation = ESP8266_OTA_SERVER_OPERATION_GET_FILE_FW;
                    //_esp8266_ota_upgrade_connect_cb(NULL);
                    char* request = (char*)os_malloc(512);
                    os_sprintf((char*)request,
                            "GET %s%s HTTP/1.0\r\nHost: " IPSTR "\r\n" ESP8266_OTA_HTTP_HEADER,
                            _esp8266_ota_server_path,
                            (_esp8266_ota_upgrade->rom_slot == ESP8266_OTA_FLASH_BY_ADDR ? ESP8266_OTA_FILE : (_esp8266_ota_upgrade->rom_slot == 0 ? _esp8266_ota_filename_rom0 : _esp8266_ota_filename_rom1)),
                            IP2STR(_esp8266_ota_server));
        
                    os_printf("HTTP REQUEST\n--------\n%s\n", request);
        
                    //SEND THE HTTP REQUEST, WITH TIMEOUT FOR REPLY
                    os_timer_setfn(&_esp8266_ota_timer, (os_timer_func_t *)_esp8266_ota_rboot_ota_deinit, 0);
                    os_timer_arm(&_esp8266_ota_timer, ESP8266_OTA_NETWORK_TIMEOUT_MS, 0);
                    espconn_sent(_esp8266_ota_upgrade->conn, request, os_strlen((char*)request));
                    os_free(request);
                }
                else
                {
                    //SERVER HAS OLDER FIRMWARE
                    //NO NEED TO DO OTA
                    os_printf("ESP8266 : OTA : Server FW is older than current. Ending !\n");
                    _esp8266_ota_rboot_ota_deinit();
                }
                return;
            }
            
            //FIRMWARE DATA
            //RUNNING TOTAL OF DOWNLOAD LENGTH
            _esp8266_ota_upgrade->total_len += length;
            //PROCESS CURRENT CHUNK
            if (!rboot_write_flash(&_esp8266_ota_upgrade->write_status, (uint8_t*)ptrData, length))
            {
                //WRITE ERROR
                _esp8266_ota_rboot_ota_deinit();
                return;
            }
            
            //WORK OUT TOTAL DOWNLOAD SIZE
            ptrLen += 16;
            ptr = (char *)os_strstr(ptrLen, "\r\n");
            *ptr = '\0'; // DESTRUCTIVE
            _esp8266_ota_upgrade->content_len = atoi(ptrLen);
        } 
        else
        {
            //FAIL, NOT A VALID HTTP HEADER/NON-200 RESPONSE/ETC.
            _esp8266_ota_rboot_ota_deinit();
            return;
        }
    }
    else
    {
        //NOT THE FIRST CHUNK, PROCESS IT
        _esp8266_ota_upgrade->total_len += length;
        if (!rboot_write_flash(&_esp8266_ota_upgrade->write_status, (uint8_t*)pusrdata, length))
        {
            //WRITE ERROR
            _esp8266_ota_rboot_ota_deinit();
            return;
        }
    }

    //CHECK IF WE ARE FINISHED
    if (_esp8266_ota_upgrade->total_len == _esp8266_ota_upgrade->content_len)
    {
        system_upgrade_flag_set(ESP8266_OTA_UPGRADE_FLAG_FINISH);
        //CLEAN UP
        _esp8266_ota_rboot_ota_deinit();
    }
    else if (_esp8266_ota_upgrade->conn->state != ESPCONN_READ)
    {
        //FAIL, BUT HOW DO WE GET HERE? PREMATURE END OF STREAM?
        _esp8266_ota_rboot_ota_deinit();
    }
    else
    {
        //TIMER FOR NEXT RECV
        os_timer_setfn(&_esp8266_ota_timer, (os_timer_func_t *)_esp8266_ota_rboot_ota_deinit, 0);
        os_timer_arm(&_esp8266_ota_timer, ESP8266_OTA_NETWORK_TIMEOUT_MS, 0);
    }
}

static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_disconcb(void *arg)
{
    //DISCONNECT CALLBACK, CLEAN UP THE CONNECTION
    //WE ALSO CALL THIS OURSELVES
    //USE PASSED PTR, AS UPGRADE STRUCT MAY HAVE GONE BY NOW
	struct espconn *conn = (struct espconn*)arg;

	os_timer_disarm(&_esp8266_ota_timer);
    if (conn)
    {
        if (conn->proto.tcp)
        {
			os_free(conn->proto.tcp);
		}
		os_free(conn);
	}

	//IS UPGRADE STRUCT STILL AROUND?
	//IF SO DISCONNECT WAS FROM REMOTE END, OR WE CALLED
	//OURSELVES TO CLEANUP A FAILED CONNECTION ATTEMPT
	//MUST ENSURE DISCONNECT WAS FOR THIS UPGRADE ATTEMPT,
	//NOT A PREVIOUS ONE! THIS CALL BACK IS ASYNC SO ANOTHER
	//UPGRADE STRUCT MAY HAVE BEEN CREATED ALREADY
    if (_esp8266_ota_upgrade && (_esp8266_ota_upgrade->conn == conn))
    {
		//MARK CONNECTION AS GONE
		_esp8266_ota_upgrade->conn = 0;
		//END THE UPDATE PROCESS
		_esp8266_ota_rboot_ota_deinit();
	}
}

static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_connect_cb(void *arg)
{
    //SUCCESSFULLY CONNECTED TO UPDATE SERVER, SEND THE REQUEST
    uint8_t *request;

    //DISABLE THE TIMEOUT
    os_timer_disarm(&_esp8266_ota_timer);

    //REGISTER CONNECTION CALLBACKS
    espconn_regist_disconcb(_esp8266_ota_upgrade->conn, _esp8266_ota_upgrade_disconcb);
    espconn_regist_recvcb(_esp8266_ota_upgrade->conn, _esp8266_ota_upgrade_recvcb);

    //HTTP REQUEST STRING
    request = (uint8_t *)os_malloc(512);
    if (!request) {
        os_printf("No ram!\r\n");
        _esp8266_ota_rboot_ota_deinit();
        return;
    }

    if(_esp8266_ota_current_operation == ESP8266_OTA_SERVER_OPERATION_GET_FILE_VERSION)
    {
        os_sprintf((char*)request,
                "GET %s%s HTTP/1.0\r\nHost: " IPSTR "\r\n" ESP8266_OTA_HTTP_HEADER,
                _esp8266_ota_server_path,
                ESP8266_VERSION_FILENAME,
                IP2STR(_esp8266_ota_server));
    }
    else
    {
        os_free(request);
        _esp8266_ota_rboot_ota_deinit();
        return;
    }
    
    os_printf("HTTP REQUEST\n--------\n%s\n", request);
    
    //SEND THE HTTP REQUEST, WITH TIMEOUT FOR REPLY
    os_timer_setfn(&_esp8266_ota_timer, (os_timer_func_t *)_esp8266_ota_rboot_ota_deinit, 0);
    os_timer_arm(&_esp8266_ota_timer, ESP8266_OTA_NETWORK_TIMEOUT_MS, 0);
    espconn_sent(_esp8266_ota_upgrade->conn, request, os_strlen((char*)request));
    os_free(request);
}

static void ICACHE_FLASH_ATTR _esp8266_ota_connect_timeout_cb()
{
    //CONNECTION ATTEMPT TIMED OUT
	os_printf("Connect timeout.\r\n");
	//NOT CONNECTED SO DON'T CALL DISCONNECT ON THE CONNECTION
	//BUT CALL OUR OWN DISCONNECT CALLBACK TO DO THE CLEANUP
	_esp8266_ota_upgrade_disconcb(_esp8266_ota_upgrade->conn);
}

static const char* ICACHE_FLASH_ATTR _esp8266_ota_esp_errstr(int8_t err)
{
    //CONNECTION ATTEMPT TIMED OUT
    switch(err)
    {
		case ESPCONN_OK:
			return "No error, everything OK.";
		case ESPCONN_MEM:
			return "Out of memory error.";
		case ESPCONN_TIMEOUT:
			return "Timeout.";
		case ESPCONN_RTE:
			return "Routing problem.";
		case ESPCONN_INPROGRESS:
			return "Operation in progress.";
		case ESPCONN_ABRT:
			return "Connection aborted.";
		case ESPCONN_RST:
			return "Connection reset.";
		case ESPCONN_CLSD:
			return "Connection closed.";
		case ESPCONN_CONN:
			return "Not connected.";
		case ESPCONN_ARG:
			return "Illegal argument.";
		case ESPCONN_ISCONN:
			return "Already connected.";
	}
}

static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_recon_cb(void *arg, int8_t errType)
{
    //CALL BACK FOR LOST CONNECTION
	os_printf("Connection error: ");
	os_printf(_esp8266_ota_esp_errstr(errType));
	os_printf("\r\n");
	//NOT CONNECTED SO DON'T CALL DISCONNECT ON THE CONNECTION
	//BUT CALL OUR OWN DISCONNECT CALLBACK TO DO THE CLEANUP
	_esp8266_ota_upgrade_disconcb(_esp8266_ota_upgrade->conn);
}

static void ICACHE_FLASH_ATTR _esp8266_ota_upgrade_resolved(const char *name, ip_addr_t *ip, void *arg)
{
    //CALL BACK FOR DNS LOOKUP
    
    if (ip == 0)
    {
        os_printf("DNS lookup failed for: ");
        os_printf(_esp8266_ota_server);
        os_printf("\r\n");
        //NOT CONNECTED SO DON'T CALL DISCONNECT ON THE CONNECTION
        //BUT CALL OUR OWN DISCONNECT CALLBACK TO DO THE CLEANUP
        _esp8266_ota_upgrade_disconcb(_esp8266_ota_upgrade->conn);
        return;
    }
    
    //SET UP CONNECTION
    _esp8266_ota_upgrade->conn->type = ESPCONN_TCP;
    _esp8266_ota_upgrade->conn->state = ESPCONN_NONE;
    _esp8266_ota_upgrade->conn->proto.tcp->local_port = espconn_port();
    _esp8266_ota_upgrade->conn->proto.tcp->remote_port = _esp8266_ota_server_port;
    *(ip_addr_t*)_esp8266_ota_upgrade->conn->proto.tcp->remote_ip = *ip;
    
    //SET CONNECTION CALL BACKS
    espconn_regist_connectcb(_esp8266_ota_upgrade->conn, _esp8266_ota_upgrade_connect_cb);
    espconn_regist_reconcb(_esp8266_ota_upgrade->conn, _esp8266_ota_upgrade_recon_cb);

    //TRY TO CONNECT
    espconn_connect(_esp8266_ota_upgrade->conn);

    //SET CONNECTION TIMEOUT TIMER
    os_timer_disarm(&_esp8266_ota_timer);
    os_timer_setfn(&_esp8266_ota_timer, (os_timer_func_t *)_esp8266_ota_connect_timeout_cb, 0);
    os_timer_arm(&_esp8266_ota_timer, ESP8266_OTA_NETWORK_TIMEOUT_MS, 0);
}

bool ICACHE_FLASH_ATTR _esp8266_ota_rboot_ota_start(ESP8266_OTA_CALLBACK callback)
{   
    //START THE OTA PROCESS, WITH USER SUPPLIED OPTIONS
    
    uint8_t slot;
    rboot_config bootconf;
    err_t result;

    //CHECK NOT ALREADY UPDATING
    if (system_upgrade_flag_check() == ESP8266_OTA_UPGRADE_FLAG_START)
    {
        return false;
    }

    //CREATE UPGRADE STATUS STRUCTURE
    _esp8266_ota_upgrade = (ESP8266_OTA_UPGRADE_STATUS*)os_zalloc(sizeof(ESP8266_OTA_UPGRADE_STATUS));
    if (!_esp8266_ota_upgrade)
    {
        os_printf("No ram!\r\n");
        return false;
    }

    //STORE THE CALLBACK
    _esp8266_ota_upgrade->callback = callback;

    //GET DETAILS OF ROM SLOT TO UPDATE
    bootconf = rboot_get_config();
    slot = bootconf.current_rom;
    if (slot == 0) slot = 1; else slot = 0;
    _esp8266_ota_upgrade->rom_slot = slot;

    //FLASH TO ROM SLOT
    _esp8266_ota_upgrade->write_status = rboot_write_init(bootconf.roms[_esp8266_ota_upgrade->rom_slot]);
    //TO FLASH A FILE (E.G. CONTAINING A FILESYSTEM) TO AN ARBITRARY LOCATION
    //(E.G. 0X40000 BYTES AFTER THE START OF THE ROM) USE CODE THIS LIKE INSTEAD:
    //NOTE: ADDRESS MUST BE START OF A SECTOR (MULTIPLE OF 4K)!
    //UPGRADE->WRITE_STATUS = RBOOT_WRITE_INIT(BOOTCONF.ROMS[UPGRADE->ROM_SLOT] + 0X40000);
    //UPGRADE->ROM_SLOT = FLASH_BY_ADDR;

    //CREATE CONNECTION
    _esp8266_ota_upgrade->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (!_esp8266_ota_upgrade->conn)
    {
        os_printf("No ram!\r\n");
        os_free(_esp8266_ota_upgrade);
        return false;
    }
    _esp8266_ota_upgrade->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    if (!_esp8266_ota_upgrade->conn->proto.tcp) 
    {
        os_printf("No ram!\r\n");
        os_free(_esp8266_ota_upgrade->conn);
        os_free(_esp8266_ota_upgrade);
        return false;
    }

    //SET UPDATE FLAG
    system_upgrade_flag_set(ESP8266_OTA_UPGRADE_FLAG_START);

    //SET OPERATION
    _esp8266_ota_current_operation = ESP8266_OTA_SERVER_OPERATION_GET_FILE_VERSION;

    //DNS LOOKUP
    result = espconn_gethostbyname(_esp8266_ota_upgrade->conn,
                                    _esp8266_ota_server, 
                                    &_esp8266_ota_upgrade->ip,
                                    _esp8266_ota_upgrade_resolved);
    if (result == ESPCONN_OK)
    {
        //HOSTNAME IS ALREADY CACHED OR IS ACTUALLY A DOTTED DECIMAL IP ADDRESS
        _esp8266_ota_upgrade_resolved(0, &_esp8266_ota_upgrade->ip, _esp8266_ota_upgrade->conn);
    }
    else if (result == ESPCONN_INPROGRESS)
    {
        //LOOKUP TAKING PLACE, WILL CALL UPGRADE_RESOLVED ON COMPLETION
    }
    else 
    {
        os_printf("DNS error!\r\n");
        os_free(_esp8266_ota_upgrade->conn->proto.tcp);
        os_free(_esp8266_ota_upgrade->conn);
        os_free(_esp8266_ota_upgrade);
        return false;
    }

    return true;
}

bool ICACHE_FLASH_ATTR _esp8266_ota_is_server_fw_version_higher(uint8_t server_major, uint8_t server_minor)
{
    //CHECKS IF THE OTA SERVER FIRMWARE VERSION IS HIGHER THAN THE CURRENTLY
    //RUNNING FIRMWARE
    //TRUE : SERVER FIRMWARE VERSION HIGHER
    //FALSE : CURRENT RUNNING VERSION HIGHER

    if(server_major > ESP8266_OTA_USER_FW_VERSION_MAJ)
        return true;
    else if (server_major == ESP8266_OTA_USER_FW_VERSION_MAJ)
    {
        if(server_minor > ESP8266_OTA_USER_FW_VERSION_MIN)
            return true;
        else
            return false;
    }
    else
        return false;
}
