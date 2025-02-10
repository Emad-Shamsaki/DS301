#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>

#include <linux/can.h>

MODULE_AUTHOR("Gianfranco Rosso");
MODULE_DESCRIPTION("HAL to CAN interface for RS1 milling machine");
MODULE_LICENSE("Proprietary");

/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

//vMM.mm.ff-bb (MM=major, mm=minor, ff=fix, bb=build)
#define HAL_CAN_INEL_VER	0x01000000		//v01.00.00-00

#define HCI_NAME    "HCI"

#define HCI_PRINT(fmt, args...)  rtapi_print(HCI_NAME ": " fmt, ## args)

#define HCI_ERR(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  HCI_NAME ": " fmt, ## args)
#define HCI_WARN(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, HCI_NAME ": " fmt, ## args)
#define HCI_INFO(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, HCI_NAME ": " fmt, ## args)
#define HCI_DBG(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  HCI_NAME ": " fmt, ## args)


static char *hci_halname = "hci";
RTAPI_MP_STRING(hci_halname, "Prefix for inel CAN hal names (default: hci)");

static char* hci_device = "can0";
RTAPI_MP_STRING(hci_device, "CAN device to use (can0, can1... default: can0)");

#define DIG_INPUTS		(16)
#define DIG_OUTPUTS		(16+16)
//#define AN_INPUTS		(0)
#define AN_OUTPUTS		(8)
#define COUNTERS		(2+2)
#define AXIS 2

// HAL pins/parameters and other module data/status
typedef struct 
{
    struct {
        int iCompId;
        const char* zsModName;
        const char* zsHalName;
    } Config;	
	
	int	iCanSocket;
	__u32 dwIO_UpdatePrescalerCount;
	
	//HAL parameters
	struct {
		hal_u32_t 	dwHalCanInelVer;
		
		hal_u32_t	dwReadExecCount;
		hal_u32_t	dwWriteExecCount;
		
		hal_u32_t	dwIO_UpdatePrescaler;
		hal_u32_t	dwWAGO_ID;

		// New parameters
    hal_float_t	DacScales[AN_OUTPUTS];  
    hal_float_t	AdcScales[AN_OUTPUTS];  
    hal_bit_t	IOexpInputFiltEn[DIG_INPUTS]; 
    hal_bit_t	IOexpInputsInv[DIG_INPUTS]; 
    hal_bit_t	IOexpOutputsInv[DIG_OUTPUTS]; 
	} HalPars;
	
	//HAL pins
	struct {
		hal_bit_t* pbitDigInputs[DIG_INPUTS]; 	
		hal_bit_t* pbitDigOutputs[DIG_OUTPUTS]; 
		hal_bit_t* pEnableFlags	;
		hal_bit_t* pbitMoveTrigger;
		
		hal_u32_t* pdwCounters[COUNTERS];

//		hal_float_t* pfAnInputs[AN_INPUTS];
		hal_float_t* pfAnOutputs[AN_OUTPUTS];
		hal_float_t* pfTargetPos[AXIS];
			
	} HalPins;	
	
} HCI_t;

static struct
{
	HCI_t* pHCI;
	int comp_id;
	const char* modname;
} Global =
{
	.pHCI = NULL,
	.comp_id = 0,
	.modname = "hal_can_inel",
};

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static void hci_read_all(void* _void_hci, long _period);
static void hci_write_all(void* _void_hci, long _period);
int hci_export(HCI_t* _pHCI);

void send_rxPDO1(HCI_t* _pHCI, int32_t posAx1, int32_t posAx2);
void send_rxPDO3(HCI_t* _pHCI, int32_t posAx5, uint8_t enableFlags);
/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/
int rtapi_app_main(void) 
{
	int _retval;
	int _family = PF_CAN;
	int _type = SOCK_RAW;
	int _proto = CAN_RAW;
	struct sockaddr_can _addr;
	struct ifreq _ifr;
	
    Global.comp_id = hal_init(Global.modname);
    if (Global.comp_id < 0) 
    {
        HCI_ERR("ERROR: hal_init() failed\n");
        return -1;
    }

	//allocate HAL shared memory
    Global.pHCI = hal_malloc(sizeof(HCI_t));
    if (Global.pHCI == NULL) 
	{
        HCI_ERR("ERROR: hal_malloc() failed\n");
		hal_exit(Global.comp_id);
		return -1;
    }

	//clear memory
    memset(Global.pHCI, 0, sizeof(HCI_t));

	//setup global state
    Global.pHCI->Config.iCompId = Global.comp_id;
    Global.pHCI->Config.zsModName = Global.modname;
    Global.pHCI->Config.zsHalName = hci_halname;    

	//socket can initialization
	
	if ((Global.pHCI->iCanSocket = socket(_family, _type, _proto)) < 0) 
	{
		HCI_ERR("ERROR: socket() failed\n");
		hal_exit(Global.comp_id);
		return -1;
	}

	_addr.can_family = _family;
	strcpy(_ifr.ifr_name, hci_device);
	ioctl(Global.pHCI->iCanSocket, SIOCGIFINDEX, &_ifr);
	_addr.can_ifindex = _ifr.ifr_ifindex;

	if (bind(Global.pHCI->iCanSocket, (struct sockaddr *)&_addr, sizeof(_addr)) < 0) 
	{
		HCI_ERR("ERROR: bind() to %s failed\n", hci_device);
		hal_exit(Global.comp_id);
		return -1;
	}

	//export pins/pars/functions to hal

    if ((_retval = hci_export(Global.pHCI))) 
	{
        HCI_ERR("ERROR: exporting HAL pins pars and functions failed, err: %d\n", _retval);
        hal_exit(Global.comp_id);
        return -1;
    }
 
    hci_write_all(Global.pHCI, 1000000);
        
    hal_ready(Global.comp_id);
    return 0;
}

void rtapi_app_exit(void) 
{
	//no need to free allocated mem (the entire HAL shared memory area is freed when the last component calls hal_exit)
    hal_exit(Global.comp_id);
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/
void send_rxPDO1(HCI_t* _pHCI, int32_t posAx1, int32_t posAx2) {
    struct can_frame frame;
    frame.can_id = 0x201;  // rxPDO1 ID
    frame.len = 8;

    memcpy(&frame.data[0], &posAx1, 4);  // Set Axis 1 position
    memcpy(&frame.data[4], &posAx2, 4);  // Set Axis 2 position

    write(_pHCI->iCanSocket, &frame, sizeof(frame));
}

void send_rxPDO3(HCI_t* _pHCI, int32_t posAx5, uint8_t enableFlags) {
    struct can_frame frame;
    frame.can_id = 0x203;  // rxPDO3 ID
    frame.len = 8;

    memset(frame.data, 0, 8);
    frame.data[4] = enableFlags;  // Enable axes

    write(_pHCI->iCanSocket, &frame, sizeof(frame));
}


int hci_export(HCI_t* _pHCI)
{
	int _i;
	int _retval;
	char _name[HAL_NAME_LEN + 1];
	
	_retval = hal_param_u32_newf(HAL_RO, &(_pHCI->HalPars.dwHalCanInelVer), _pHCI->Config.iCompId, "%s.version", _pHCI->Config.zsHalName);
    if (_retval < 0) {
        HCI_ERR("ERROR: exporting parameter '%s.version', err: %d\n", _pHCI->Config.zsHalName, _retval);
        return -1;
    }	
	_pHCI->HalPars.dwHalCanInelVer = HAL_CAN_INEL_VER;	
	
	_retval = hal_param_u32_newf(HAL_RO, &(_pHCI->HalPars.dwReadExecCount), _pHCI->Config.iCompId, "%s.dbg.read-exec-count", _pHCI->Config.zsHalName);
    if (_retval < 0) {
        HCI_ERR("ERROR: exporting parameter '%s.dbg.read-exec-count', err: %d\n", _pHCI->Config.zsHalName, _retval);
        return -1;
    }
	_pHCI->HalPars.dwReadExecCount = 0;

	_retval = hal_param_u32_newf(HAL_RO, &(_pHCI->HalPars.dwWriteExecCount), _pHCI->Config.iCompId, "%s.dbg.write-exec-count", _pHCI->Config.zsHalName);
    if (_retval < 0) {
        HCI_ERR("ERROR: exporting parameter '%s.dbg.write-exec-count', err: %d\n", _pHCI->Config.zsHalName, _retval);
        return -1;
    }
	_pHCI->HalPars.dwWriteExecCount = 0;	
	
	_retval = hal_param_u32_newf(HAL_RW, &(_pHCI->HalPars.dwIO_UpdatePrescaler), _pHCI->Config.iCompId, "%s.io-update-prescaler", _pHCI->Config.zsHalName);
    if (_retval < 0) {
        HCI_ERR("ERROR: exporting parameter '%s.io-update-prescaler', err: %d\n", _pHCI->Config.zsHalName, _retval);
        return -1;
    }
	_pHCI->HalPars.dwIO_UpdatePrescaler = 5;
	_pHCI->dwIO_UpdatePrescalerCount = 0;	
	
	_retval = hal_param_u32_newf(HAL_RW, &(_pHCI->HalPars.dwWAGO_ID), _pHCI->Config.iCompId, "%s.io-wago-id", _pHCI->Config.zsHalName);
    if (_retval < 0) {
        HCI_ERR("ERROR: exporting parameter '%s.io-wago-id', err: %d\n", _pHCI->Config.zsHalName, _retval);
        return -1;
    }
	_pHCI->HalPars.dwWAGO_ID = 0x10;	


	//------------------------------------------------------------------------------------

	for (_i = 0; _i < DIG_INPUTS; _i++)
	{
		_retval = hal_pin_bit_newf(HAL_OUT, &(_pHCI->HalPins.pbitDigInputs[_i]), _pHCI->Config.iCompId, "%s.din-%02d", _pHCI->Config.zsHalName, _i);
		if (_retval < 0) 
		{
			HCI_ERR("ERROR: exporting pin '%s.din-%02d', err: %d\n", _pHCI->Config.zsHalName, _i, _retval);
			hal_exit(_pHCI->Config.iCompId);
			return -1;
		}
		*(_pHCI->HalPins.pbitDigInputs[_i]) = 0;
	}	

	//------------------------------------------------------------------------------------

	for (_i = 0; _i < DIG_OUTPUTS; _i++)
	{
		_retval = hal_pin_bit_newf(HAL_IN, &(_pHCI->HalPins.pbitDigOutputs[_i]), _pHCI->Config.iCompId, "%s.dout-%02d", _pHCI->Config.zsHalName, _i);
		if (_retval < 0) 
		{
			HCI_ERR("ERROR: exporting pin '%s.dout-%02d', err: %d\n", _pHCI->Config.zsHalName, _i, _retval);
			hal_exit(_pHCI->Config.iCompId);
			return -1;
		}
		*(_pHCI->HalPins.pbitDigOutputs[_i]) = 0;
	}	

	//------------------------------------------------------------------------------------

	for (_i = 0; _i < COUNTERS; _i++)
	{
		_retval = hal_pin_u32_newf(HAL_OUT, &(_pHCI->HalPins.pdwCounters[_i]), _pHCI->Config.iCompId, "%s.counter-%02d", _pHCI->Config.zsHalName, _i);
		if (_retval < 0) 
		{
			HCI_ERR("ERROR: exporting pin '%s.counter-%02d', err: %d\n", _pHCI->Config.zsHalName, _i, _retval);
			hal_exit(_pHCI->Config.iCompId);
			return -1;
		}
		*(_pHCI->HalPins.pdwCounters[_i]) = 0;
	}	

//------------------------------------------------------------------------------------

	for (_i = 0; _i < AN_OUTPUTS; _i++)
	{
		_retval = hal_pin_float_newf(HAL_IN, &(_pHCI->HalPins.pfAnOutputs[_i]), 
									_pHCI->Config.iCompId, "%s.aout-%02d", 
									_pHCI->Config.zsHalName, _i);
		if (_retval < 0) 
		{
			HCI_ERR("ERROR: exporting pin '%s.aout-%02d', err: %d\n", 
					_pHCI->Config.zsHalName, _i, _retval);
			hal_exit(_pHCI->Config.iCompId);
			return -1;
		}
		*(_pHCI->HalPins.pfAnOutputs[_i]) = 0.0;  // Initialize to zero
	}   
	

//----------------------------------------------------------------------------------------
	// for (int i = 0; i < AN_OUTPUTS; i++) 
	// {
	// 	_retval = hal_param_float_newf(HAL_RW, &(Global.pHCI->HalPars.DacScales[i]),
	// 								Global.comp_id, "%s.dac-scale-%02d", 
	// 								Global.pHCI->Config.zsHalName, i);
	// 	if (_retval < 0) 
	// 	{
	// 		HCI_ERR("ERROR: exporting parameter '%s.dac-scale-%02d', err: %d\n", 
	// 				Global.pHCI->Config.zsHalName, i, _retval);
	// 		return -1;
	// 	}
	// 	Global.pHCI->HalPars.DacScales[i] = 1.0;  // Default scaling

	// 	_retval = hal_param_float_newf(HAL_RW, &(Global.pHCI->HalPars.AdcScales[i]),
	// 								Global.comp_id, "%s.adc-scale-%02d", 
	// 								Global.pHCI->Config.zsHalName, i);
	// 	if (_retval < 0) 
	// 	{
	// 		HCI_ERR("ERROR: exporting parameter '%s.adc-scale-%02d', err: %d\n", 
	// 				Global.pHCI->Config.zsHalName, i, _retval);
	// 		return -1;
	// 	}
	// 	Global.pHCI->HalPars.AdcScales[i] = 1.0;  // Default scaling
	// }

	// for (int i = 0; i < DIG_INPUTS; i++) 
	// {
	//     _retval = hal_param_bit_newf(HAL_RW, &(Global.pHCI->HalPars.IOexpInputFiltEn[i]),
	//                                  Global.comp_id, "%s.ioexp-input-filt-%02d", 
	//                                  Global.pHCI->Config.zsHalName, i);
	//     if (_retval < 0) 
	//     {
	//         HCI_ERR("ERROR: exporting parameter '%s.ioexp-input-filt-%02d', err: %d\n", 
	//                 Global.pHCI->Config.zsHalName, i, _retval);
	//         return -1;
	//     }
	//     Global.pHCI->HalPars.IOexpInputFiltEn[i] = 1;  // Default: filtering enabled

	//     _retval = hal_param_bit_newf(HAL_RW, &(Global.pHCI->HalPars.IOexpInputsInv[i]),
	//                                  Global.comp_id, "%s.ioexp-input-inv-%02d", 
	//                                  Global.pHCI->Config.zsHalName, i);
	//     if (_retval < 0) 
	//     {
	//         HCI_ERR("ERROR: exporting parameter '%s.ioexp-input-inv-%02d', err: %d\n", 
	//                 Global.pHCI->Config.zsHalName, i, _retval);
	//         return -1;
	//     }
	//     Global.pHCI->HalPars.IOexpInputsInv[i] = 0;  // Default: no inversion
	// }

//--------------------------------------------------------------------------------


	// Create position HAL pins for Axis 1 and Axis 2
	_retval = hal_pin_float_newf(HAL_IN, &(_pHCI->HalPins.pfTargetPos[0]), 
								_pHCI->Config.iCompId, "%s.axis-1-pos", 
								_pHCI->Config.zsHalName);
	if (_retval < 0) {
		HCI_ERR("ERROR: exporting pin '%s.axis-1-pos', err: %d\n", 
				_pHCI->Config.zsHalName, _retval);
		return -1;
	}
	*_pHCI->HalPins.pfTargetPos[0] = 0.0;

	_retval = hal_pin_float_newf(HAL_IN, &(_pHCI->HalPins.pfTargetPos[1]), 
								_pHCI->Config.iCompId, "%s.axis-2-pos", 
								_pHCI->Config.zsHalName);
	if (_retval < 0) {
		HCI_ERR("ERROR: exporting pin '%s.axis-2-pos', err: %d\n", 
				_pHCI->Config.zsHalName, _retval);
		return -1;
	}
	*_pHCI->HalPins.pfTargetPos[1] = 0.0;

	// Enable Axes Command Pin
	_retval = hal_pin_u32_newf(HAL_IN, &(_pHCI->HalPins.pEnableFlags), 
							_pHCI->Config.iCompId, "%s.enable-flags", 
							_pHCI->Config.zsHalName);
	if (_retval < 0) {
		HCI_ERR("ERROR: exporting pin '%s.enable-flags', err: %d\n", 
				_pHCI->Config.zsHalName, _retval);
		return -1;
	}
	*_pHCI->HalPins.pEnableFlags = 0x03;  // Default: Enable Axis 1 and 2

	// Movement Trigger Pin
	_retval = hal_pin_bit_newf(HAL_IN, &(_pHCI->HalPins.pbitMoveTrigger), 
							_pHCI->Config.iCompId, "%s.move-trigger", 
							_pHCI->Config.zsHalName);
	if (_retval < 0) {
		HCI_ERR("ERROR: exporting pin '%s.move-trigger', err: %d\n", 
				_pHCI->Config.zsHalName, _retval);
		return -1;
	}
	*_pHCI->HalPins.pbitMoveTrigger = 0;



	//functions export
 
    rtapi_snprintf(_name, sizeof(_name), "%s.write-all", _pHCI->Config.zsHalName);
    _retval = hal_export_funct(_name, hci_write_all, _pHCI, 1, 0, _pHCI->Config.iCompId);
    if (_retval != 0) 
	{
        HCI_ERR("ERROR: function '%s\n' export failed, err: %d\n", _name, _retval);
        hal_exit(_pHCI->Config.iCompId);
        return -1;
    }

    rtapi_snprintf(_name, sizeof(_name), "%s.read-all", _pHCI->Config.zsHalName);
    _retval = hal_export_funct(_name, hci_read_all, _pHCI, 1, 0, _pHCI->Config.iCompId);
    if (_retval != 0) 
	{
        HCI_ERR("ERROR: function '%s\n' export failed, err: %d\n", _name, _retval);
        hal_exit(_pHCI->Config.iCompId);
        return -1;
    }
 
	return 0;
}


/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/
void hci_read_all(void *_void_hci, long _period) 
{
    HCI_t* _pHCI = (HCI_t*)_void_hci;
    struct can_frame _frame;
    int _iRead;
	int bytesRead;
    __u32 _can_id;
    __u32 _Counter;

    _pHCI->HalPars.dwReadExecCount++;

    while ((_iRead = recv(_pHCI->iCanSocket, &_frame, sizeof(_frame), MSG_DONTWAIT)) > 0) 
    {   
        if ((_frame.can_id & CAN_EFF_FLAG) == 0) 
        {   
            _can_id = _frame.can_id & CAN_SFF_MASK;

            // Debugging: Print received CAN frame
            printf("Received CAN ID: 0x%X Data:", _can_id);
            for (int i = 0; i < _frame.len; i++)
                printf(" %02X", _frame.data[i]);
            printf("\n");

            // Process Digital Inputs and Counters (ID: 0x190)
            if ((_can_id == (0x180 + _pHCI->HalPars.dwWAGO_ID)) && (_frame.len == 8)) 
            {   
                // Read Digital Inputs (1 Byte Each)
                *_pHCI->HalPins.pbitDigInputs[0] = (_frame.data[0] & 0x01) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[1] = (_frame.data[0] & 0x02) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[2] = (_frame.data[0] & 0x04) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[3] = (_frame.data[0] & 0x08) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[4] = (_frame.data[0] & 0x10) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[5] = (_frame.data[0] & 0x20) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[6] = (_frame.data[0] & 0x40) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[7] = (_frame.data[0] & 0x80) ? 1 : 0;

                *_pHCI->HalPins.pbitDigInputs[8] = (_frame.data[1] & 0x01) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[9] = (_frame.data[1] & 0x02) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[10] = (_frame.data[1] & 0x04) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[11] = (_frame.data[1] & 0x08) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[12] = (_frame.data[1] & 0x10) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[13] = (_frame.data[1] & 0x20) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[14] = (_frame.data[1] & 0x40) ? 1 : 0;
                *_pHCI->HalPins.pbitDigInputs[15] = (_frame.data[1] & 0x80) ? 1 : 0;

                // Read Counters (3 Bytes Each)
                _Counter = _frame.data[2] | (_frame.data[3] << 8) | (_frame.data[4] << 16);
                *_pHCI->HalPins.pdwCounters[0] = _Counter;
                //printf("Counter 1: %u\n", _Counter);

                _Counter = _frame.data[5] | (_frame.data[6] << 8) | (_frame.data[7] << 16);
                *_pHCI->HalPins.pdwCounters[1] = _Counter;
                //printf("Counter 2: %u\n", _Counter);
            }

            // Process Additional Counters (ID: 0x290)
            else if ((_can_id == (0x280 + _pHCI->HalPars.dwWAGO_ID)) && (_frame.len == 6)) 
            {   
                // Read Counter 3
                _Counter = _frame.data[0] | (_frame.data[1] << 8) | (_frame.data[2] << 16);
                *_pHCI->HalPins.pdwCounters[2] = _Counter;
                //printf("Counter 3: %u\n", _Counter);

                // Read Counter 4
                _Counter = _frame.data[3] | (_frame.data[4] << 8) | (_frame.data[5] << 16);
                *_pHCI->HalPins.pdwCounters[3] = _Counter;
                //printf("Counter 4: %u\n", _Counter);
            }
        }
		
		// TODO: receive txPDO from SIEB&MEYER spindle drive

		// TODO : to receive SDO reply from any device

		// txPDO1 - Feedback for Axis 1 & 2
		if (_frame.can_id == 0x181) 
		{  
            int32_t posAx1, posAx2;
            memcpy(&posAx1, &frame.data[0], 4);
            memcpy(&posAx2, &frame.data[4], 4);
            printf("Feedback: Axis1: %d, Axis2: %d\n", posAx1, posAx2);
        }
    }
}


/**/
void hci_write_all(void *_void_hci, long _period) 
{
	HCI_t* _pHCI = (HCI_t*)_void_hci;
	struct can_frame _frame;
	
	_pHCI->HalPars.dwWriteExecCount++;
	
	_pHCI->dwIO_UpdatePrescalerCount++;
	
	if (*_pHCI->HalPins.pbitMoveTrigger) 
	{  // Only send when trigger is set
        *_pHCI->HalPins.pbitMoveTrigger = 0;  // Reset trigger after sending

        int32_t posAx1 = (int32_t) (*_pHCI->HalPins.pfTargetPos[0]);
        int32_t posAx2 = (int32_t) (*_pHCI->HalPins.pfTargetPos[1]);

        uint8_t enableFlags = (uint8_t) (*_pHCI->HalPins.pEnableFlags);

        send_rxPDO1(_pHCI, posAx1, posAx2);  // Send position command for Axis 1 & 2
        send_rxPDO3(_pHCI, 0, enableFlags);  // Send enable command
    }
	
	if (_pHCI->dwIO_UpdatePrescalerCount >= _pHCI->HalPars.dwIO_UpdatePrescaler)
	{
		_pHCI->dwIO_UpdatePrescalerCount = 0;
	
		//transmit WAGO I/O rxPDO1
		_frame.can_id = 0x200 + _pHCI->HalPars.dwWAGO_ID;
		_frame.len = 8;
		
		_frame.data[0] = 0x00 |
			(*_pHCI->HalPins.pbitDigOutputs[0] ? 0x01 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[1] ? 0x02 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[2] ? 0x04 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[3] ? 0x08 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[4] ? 0x10 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[5] ? 0x20 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[6] ? 0x40 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[7] ? 0x80 : 0x00) ;
		
		_frame.data[1] = 0x00 |
			(*_pHCI->HalPins.pbitDigOutputs[8] ? 0x01 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[9] ? 0x02 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[10] ? 0x04 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[11] ? 0x08 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[12] ? 0x10 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[13] ? 0x20 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[14] ? 0x40 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[15] ? 0x80 : 0x00) ;
			
		_frame.data[2] = 0x00 |
			(*_pHCI->HalPins.pbitDigOutputs[16] ? 0x01 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[17] ? 0x02 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[18] ? 0x04 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[19] ? 0x08 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[20] ? 0x10 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[21] ? 0x20 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[22] ? 0x40 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[23] ? 0x80 : 0x00) ;
			
		_frame.data[3] = 0x00 |
			(*_pHCI->HalPins.pbitDigOutputs[24] ? 0x01 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[25] ? 0x02 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[26] ? 0x04 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[27] ? 0x08 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[28] ? 0x10 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[29] ? 0x20 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[30] ? 0x40 : 0x00) |
			(*_pHCI->HalPins.pbitDigOutputs[31] ? 0x80 : 0x00) ;
		
		// //TODO: ADD DATA FOR ANALOG OUTPUT 1
		// _frame.data[4] = 0x00;
		// _frame.data[5] = 0x00;

			int16_t scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[0]) * (32767.0 / 10.0));
			_frame.data[4] = scaled_value & 0xFF;        // Low Byte
			_frame.data[5] = (scaled_value >> 8) & 0xFF; // High Byte


		// //TODO: ADD DATA FOR ANALOG OUTPUT 2
			 scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[1]) * (32767.0 / 10.0));
			_frame.data[6] = scaled_value & 0xFF;        // Low Byte
			_frame.data[7] = (scaled_value >> 8) & 0xFF; // High Byte
		
		
		write(_pHCI->iCanSocket, &_frame, sizeof(_frame));
		
		//transmit WAGO I/O rxPDO2
		_frame.can_id = 0x300 + _pHCI->HalPars.dwWAGO_ID;
		_frame.len = 8;
		
		//TODO: ADD DATA FOR ANALOG OUTPUT 3
		 scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[2]) * (32767.0 / 10.0));
		_frame.data[0] = scaled_value & 0xFF;        // Low Byte
		_frame.data[1] = (scaled_value >> 8) & 0xFF; // High Byte
		
		//TODO: ADD DATA FOR ANALOG OUTPUT 4
		 scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[3]) * (32767.0 / 10.0));
		_frame.data[2] = scaled_value & 0xFF;        // Low Byte
		_frame.data[3] = (scaled_value >> 8) & 0xFF; // High Byte
		
		//TODO: ADD DATA FOR ANALOG OUTPUT 5
		 scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[4]) * (32767.0 / 10.0));
		_frame.data[4] = scaled_value & 0xFF;        // Low Byte
		_frame.data[5] = (scaled_value >> 8) & 0xFF; // High Byte
		
		//TODO: ADD DATA FOR ANALOG OUTPUT 6
		 scaled_value = (int16_t)((*_pHCI->HalPins.pfAnOutputs[5]) * (32767.0 / 10.0));
		_frame.data[6] = scaled_value & 0xFF;        // Low Byte
		_frame.data[7] = (scaled_value >> 8) & 0xFF; // High Byte
		
		write(_pHCI->iCanSocket, &_frame, sizeof(_frame));	
	}
	
	// TODO: send rxPDO to can axes 1 & 2 (S2509)
	
	// TODO: send rxPDO to SIEB&MEYER spindle drive
	
}

