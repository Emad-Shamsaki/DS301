//----------------------------------------------------------------------//
// Description: hal_pru_inel.c                                          //
// HAL module to communicate with PRU code implementing real-time       //
// managing of Inel Elettronica S16002 + S16001 + S16003 boards         //
//                                                                      //
// Author(s): Gianfranco Rosso                                          //
//																		//
//----------------------------------------------------------------------//
 
// Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H
//#include "config_module.h"



//#include RTAPI_INC_LIST_H
#include "rtapi.h"          /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
#include "rtapi_compat.h"   /* RTAPI support functions */
#include "rtapi_math.h"
#include "hal.h"            /* HAL public API decls */
#include <pthread.h>


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
//#include <linux/bcd.h>
#include <math.h>

#include "hal_can_inel.h"
#include "S16001_CanData.h"

MODULE_AUTHOR("Gianfranco Rosso");
MODULE_DESCRIPTION("AM335x PRU component");
MODULE_LICENSE("GPL");

//visto che non c'e' linux/bcd.h riporto direttamente le define
#define bcd2bin(x)        (((x) & 0x0f) + (((x) >> 4) * 10))
#define bin2bcd(x)        ((((x) / 10) << 4) + ((x) % 10))

//vMM.mm.ff-bb (MM=major, mm=minor, ff=fix, bb=build)
#define HAL_CAN_INEL_VER	0x01000000		//v01.00.00-00

/***********************************************************************
*                    MODULE PARAMETERS AND DEFINES                     *
************************************************************************/



// Start out with default pulse length/width and setup/hold delays of 1 mS (1000000 nS) 
#define DEFAULT_DELAY 1000000

#define f_period_s ((double)(l_period_ns * 1e-9))

static char *hpi_halname = "hci";
RTAPI_MP_STRING(hpi_halname, "Prefix for inel hal names (default: hci)");


/*
static char* hpi_in_inv = "0x00000000";
RTAPI_MP_STRING(hpi_in_inv, "Inel PRU ioexpander in_inv defaults (0x00000000..0xFFFFFFFF, default: 0x00000000)");

static char* hpi_in_filt = "0xFFFFFFFF";
RTAPI_MP_STRING(hpi_in_filt, "Inel PRU ioexpander in_filt enable defaults (0x00000000..0xFFFFFFFF, default: 0xFFFFFFFF)");

static char* hpi_out = "0x00000000";
RTAPI_MP_STRING(hpi_out, "Inel PRU ioexpander out defaults (0x00000000..0xFFFFFFFF, default: 0x00000000)");

static char* hpi_out_inv = "0x00000000";
RTAPI_MP_STRING(hpi_out_inv, "Inel PRU ioexpander out_inv defaults (0x00000000..0xFFFFFFFF, default: 0x00000000)");
*/



/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/


/* other globals */
static int comp_id;     /* component ID */

static const char *modname = "hal_can_inel";



/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
static void hci_read(void* void_hci, long period);
static void hci_write(void* void_hci, long period);



int hci_export(HalCanInel_t *pHCI);
/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    HalCanInel_t *pHCI;
    int retval;

    comp_id = hal_init(modname);
    if (comp_id < 0) 
	{
        HPI_ERR("ERROR: hal_init() failed\n");
        return -1;
    }

    // Allocate HAL shared memory for state data
    pHCI = hal_malloc(sizeof(HalCanInel_t));
    if (pHCI == 0) 
	{
        HPI_ERR("ERROR: hal_malloc() failed\n");
		hal_exit(comp_id);
		return -1;
    }

    // Clear memory
    memset(pHCI, 0, sizeof(HalCanInel_t));


    // Setup global state
    
    pHCI->Config.iCompId = comp_id;
    pHCI->Config.zsModName = modname;
    pHCI->Config.zsHalName = hci_halname;    

    if ((retval = hpi_export(pHCI))) 
	{
        HPI_ERR("ERROR: exporting HAL pins pars and functions failed, err: %d\n", retval);
        hal_exit(comp_id);
        return -1;
    }
 
	pHCI->Config.bForceWrite = 1;
    hci_write(pHCI, 1000000);
	pHCI->Config.bForceWrite = 0;
        
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) 
{
    hal_exit(comp_id);
}

/***********************************************************************
*                       REALTIME FUNCTIONS                             *
************************************************************************/

static void hci_read(void* void_hci, long period)
{
	HalCanInel_t* pHCI = void_hci;



}

static void hci_write(void* void_hci, long period) 
{
	HalCanInel_t* pHCI = void_hci;


}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

int assure_module_loaded(const char *module)
{
    FILE *fd;
    char line[100];
    int len = strlen(module);
    int retval;

    fd = fopen("/proc/modules", "r");
    if (fd == NULL) 
	{
        HPI_ERR("ERROR: cannot read /proc/modules\n");
        return -1;
    }
    while (fgets(line, sizeof(line), fd)) 
	{
        if (!strncmp(line, module, len)) 
		{
            HPI_DBG("module '%s' already loaded\n", module);
            fclose(fd);
            return 0;
        }
    }
    fclose(fd);
    HPI_DBG("loading module '%s'\n", module);
    sprintf(line, "/sbin/modprobe %s", module);
    if ((retval = system(line))) 
	{
        HPI_ERR("ERROR: executing '%s'  %d - %s\n", line, errno, strerror(errno));
        return -1;
    }
    return 0;
}

int hci_export(HalCanInel_t *pHCI)
{
	int i;
	int retval;
	char* pError;
	u32 dwIOexpInInvDef;
	u32 dwIOexpInFiltDef;
	u32 dwIOexpOutDef;
	u32 dwIOexpOutInvDef;
#ifdef TARGET_S16003_01
	u32 dwIOexpOutDef1;
	u32 dwIOexpOutInvDef1;
#endif	
    char name[HAL_NAME_LEN + 1];
	
	//export parameters and pins

/*	
	//------------------------------------------------------------------
	//ioexp inputs
	
	dwIOexpInInvDef = strtoul(hpi_in_inv, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_in_inv=%s at %s\n", hpi_in_inv, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}	
	
	for (i = 0; i < IOEXP_IN_PINS; i++)
	{
//N.B. si lasciano definiti i 2 pin di ingresso per uniformita' con la versione precedente
//#ifdef TARGET_S16003_01
//		if ((i == 27) || (i == 28))
//			continue;		//i 2 ingressi convertiti in uscite...
//#endif		
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.pIOexpInputs[i]), pHPI->Config.iCompId, "%s.ioexp.in-%02o", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.ioexp.in-%02o', err: %d\n", pHPI->Config.zsHalName,i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.pIOexpInputs[i]) = 0;

		retval = hal_param_bit_newf(HAL_RW, &(pHPI->HalPars.IOexpInputsInv[i]), pHPI->Config.iCompId, "%s.ioexp.in-%02o.invert", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.ioexp.in-%02o.invert', err: %d\n", pHPI->Config.zsHalName,i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		pHPI->HalPars.IOexpInputsInv[i] = ((dwIOexpInInvDef >> i) & 0x01);
	}	
	
	dwIOexpInFiltDef = strtoul(hpi_in_filt, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_in_filt=%s at %s\n", hpi_in_filt, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}	
	retval = hal_param_u32_newf(HAL_RW, &(pHPI->HalPars.IOexpInputFiltEn), pHPI->Config.iCompId, "%s.ioexp.in-filt", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.ioexp.in-filt', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}
	pHPI->HalPars.IOexpInputFiltEn = dwIOexpInFiltDef;	
	
	//------------------------------------------------------------------
	//ioexp outputs
	dwIOexpOutDef = strtoul(hpi_out, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_out=%s at %s\n", hpi_out, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}
	
	dwIOexpOutInvDef = strtoul(hpi_out_inv, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_out_inv=%s at %s\n", hpi_out_inv, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}	

#ifdef TARGET_S16003_01
	dwIOexpOutDef1 = strtoul(hpi_out1, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_out1=%s at %s\n", hpi_out1, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}
	
	dwIOexpOutInvDef1 = strtoul(hpi_out_inv1, &pError, 16);
	if ((pError != NULL) && (*pError != 0))
	{
		HPI_ERR("ERROR: error converting command line parameter hpi_out_inv1=%s at %s\n", hpi_out_inv1, pError);
		hal_exit(pHPI->Config.iCompId);
		return -1;	
	}
#endif
	
	for (i = 0; i < IOEXP_OUT_PINS; i++)
	{
		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.pIOexpOutputs[i]), pHPI->Config.iCompId, "%s.ioexp.out-%02o", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.ioexp.out-%02o', err: %d\n", pHPI->Config.zsHalName,i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
#ifdef TARGET_S16003_01
		if (i < 32)
			*(pHPI->HalPins.pIOexpOutputs[i]) = ((dwIOexpOutDef >> i) & 0x01);
		else
			*(pHPI->HalPins.pIOexpOutputs[i]) = ((dwIOexpOutDef1 >> (i - 32)) & 0x01);			
#else		
		*(pHPI->HalPins.pIOexpOutputs[i]) = ((dwIOexpOutDef >> i) & 0x01);		
#endif

		retval = hal_param_bit_newf(HAL_RW, &(pHPI->HalPars.IOexpOutputsInv[i]), pHPI->Config.iCompId, "%s.ioexp.out-%02o.invert", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.ioexp.out-%02o.invert', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
#ifdef TARGET_S16003_01
		if (i < 32)
			pHPI->HalPars.IOexpOutputsInv[i] = ((dwIOexpOutInvDef >> i) & 0x01);
		else
			pHPI->HalPars.IOexpOutputsInv[i] = ((dwIOexpOutInvDef1 >> (i - 32)) & 0x01);			
#else		
		pHPI->HalPars.IOexpOutputsInv[i] = ((dwIOexpOutInvDef >> i) & 0x01);
#endif
	}	
	
#ifndef TARGET_S16007	
	//inizializza lo stato di safe delle uscite 
	pHPI->pPruTaskDataI2c->dwIOexpOutputsSafe = dwIOexpOutDef ^ dwIOexpOutInvDef;
#ifdef TARGET_S16003_01
	pHPI->pPruTaskDataI2c->dwIOexpOutputs1Safe = dwIOexpOutDef1 ^ dwIOexpOutInvDef1;
#endif
#endif	
	
	//------------------------------------------------------------------
	//dac output 
	
	for (i = 0; i < DAC_OUTPUTS; i++)
	{
		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.pDacEnables[i]), pHPI->Config.iCompId, "%s.dac.out-%02d.enable", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.dac.out-%02d.enable', err: %d\n", pHPI->Config.zsHalName,i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.pDacEnables[i]) = 0;

		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.pDacValues[i]), pHPI->Config.iCompId, "%s.dac.out-%02d.value", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.dac.out-%02d.value', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.pDacValues[i]) = 0.0;

		retval = hal_param_float_newf(HAL_RW, &(pHPI->HalPars.DacScales[i]), pHPI->Config.iCompId, "%s.dac.out-%02d.scale", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dac.out-%02d.scale', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		pHPI->HalPars.DacScales[i] =  1.0;
		
#ifndef TARGET_S16007		
		retval = hal_param_bit_newf(HAL_RW, &(pHPI->HalPars.DacOut2Xs[i]), pHPI->Config.iCompId, "%s.dac.out-%02d.2x", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dac.out-%02d.2x', err: %d\n", pHPI->Config.zsHalName,i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		pHPI->HalPars.DacOut2Xs[i] = 1;		
#endif		
	}		

#ifndef TARGET_S16007
	//------------------------------------------------------------------
	//adc input

	for (i = 0; i < ADC_INPUTS; i++)
	{
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.pAdcValues[i]), pHPI->Config.iCompId, "%s.adc.in-%02d.value", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.adc.in-%02d.value', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.pAdcValues[i]) = 0.0;
		
		retval = hal_param_float_newf(HAL_RW, &(pHPI->HalPars.AdcScales[i]), pHPI->Config.iCompId, "%s.adc.in-%02d.scale", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.adc.in-%02d.scale', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		pHPI->HalPars.AdcScales[i] =  2.048*123.2/23.2;		//come scala di default usa la tensione di FS [V] 
	}
*/
	

	
	//------------------------------------------------------------------
	//varie
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.HalCanInelVer), pHPI->Config.iCompId, "%s.dbg.hal_can_inel_ver", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.hal_can_inel_ver', err: %d\n",pHPI->Config.zsHalName, retval);
        return -1;
    }	
	pHCI->HalPars.Dbg.HalCanInelVer = HAL_CAN_INEL_VER;	
	

	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.HalUpdateCount), pHPI->Config.iCompId, "%s.dbg.hal_update_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.hal_update_count', err: %d\n",pHPI->Config.zsHalName, retval);
        return -1;
    }		


	//------------------------------------------------------------------
	//CAN Axes

	retval = hal_param_bit_newf(HAL_RW, &(pHPI->HalPars.CanAxesSyncEn), pHPI->Config.iCompId, "%s.can_axes.sync_en", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.sync_en', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	pHPI->HalPars.CanAxesSyncEn = 0;

	retval = hal_param_u32_newf(HAL_RW, &(pHPI->HalPars.CanAxesRxTimeout), pHPI->Config.iCompId, "%s.can_axes.rx_timeout_time", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.rx_timeout_time', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	pHPI->HalPars.CanAxesRxTimeout = 10;	//default timeout comunicazione assi [ms]
			
	retval = hal_param_u32_newf(HAL_RW, &(pHPI->HalPars.AxesFaultCodeRegAddress), pHPI->Config.iCompId, "%s.can_axes.fault_reg_addr", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.fault_reg_addr', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	pHPI->HalPars.AxesFaultCodeRegAddress = 0x2000 + 291;	//indirizzo modbus registro allarmi S16001 sia con fw brushless che stepper	
			
	retval = hal_param_u32_newf(HAL_RW, &(pHPI->HalPars.AxesFwVersionRegAddress), pHPI->Config.iCompId, "%s.can_axes.fw_ver_reg_addr", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.fw_ver_reg_addr', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	pHPI->HalPars.AxesFwVersionRegAddress = 0x2000 + 293;	//indirizzo modbus registro versione fw S16001 sia con fw brushless che stepper				
			
	retval = hal_param_u32_newf(HAL_RW, &(pHPI->HalPars.SpindleCAN_Node), pHPI->Config.iCompId, "%s.can_axes.spindle_node_addr", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.spindle_node_addr', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	pHPI->HalPars.SpindleCAN_Node = 9;				//per default il mandrino ha indirizzo 9 (da M9...)
	
	retval = hal_param_float_newf(HAL_RW, &(pHPI->HalPars.SpindleCurrentK), pHPI->Config.iCompId, "%s.can_axes.spindle_current_K", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.spindle_current_K', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}
	pHPI->HalPars.SpindleCurrentK =  0.1;			//fattore per conversione valore corrente uscita S16005 
	
	
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesCycleCount), pHPI->Config.iCompId, "%s.dbg.can_axes.cycle_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: adding pin '%s.dbg.can_axes.cycle_count', err: %d\n",pHPI->Config.zsHalName, retval);
        return -1;
    }

	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesBusOffCount), pHPI->Config.iCompId, "%s.dbg.can_axes.bus_off_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.bus_off_count', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }
  
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesReInitCount), pHPI->Config.iCompId, "%s.dbg.can_axes.re_init_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.re_init_count', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }  

	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesTxParityErrorCount), pHPI->Config.iCompId, "%s.dbg.can_axes.tx_parity_error_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.tx_parity_error_count', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }
    
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesRxParityErrorCount), pHPI->Config.iCompId, "%s.dbg.can_axes.rx_parity_error_count", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rx_parity_error_count', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }
 
 	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesParityErrorAccazzus), pHPI->Config.iCompId, "%s.dbg.can_axes.parity_error_accazzus", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.parity_error_accazzus', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }
   
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesTxErrorCounter), pHPI->Config.iCompId, "%s.dbg.can_axes.tx_error_counter", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.tx_error_counter', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    }
    
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesRxErrorCounter), pHPI->Config.iCompId, "%s.dbg.can_axes.rx_error_counter", pHPI->Config.zsHalName);
    if (retval < 0) {
        HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rx_error_counter', err: %d\n", pHPI->Config.zsHalName, retval);
        return -1;
    } 
  
 	for (i = 0; i < 1; i++)
	{
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx1[i*2]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo1-%02d.data-0", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo1-%02d.data-0', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx1[i*2+1]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo1-%02d.data-1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo1-%02d.data-1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		} 
		
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx2[i*2]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo2-%02d.data-0", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo2-%02d.data-0', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx2[i*2+1]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo2-%02d.data-1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo2-%02d.data-1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		} 
		
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx3[i*2]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo3-%02d.data-0", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo3-%02d.data-0', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx3[i*2+1]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo3-%02d.data-1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo3-%02d.data-1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		} 	

		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx4[i*2]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo4-%02d.data-0", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo4-%02d.data-0', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataTx4[i*2+1]), pHPI->Config.iCompId, "%s.dbg.can_axes.rxpdo4-%02d.data-1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.rxpdo4-%02d.data-1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		} 		
    }
     
 	for (i = 0; i < 7; i++)
	{
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataRx[i*2]), pHPI->Config.iCompId, "%s.dbg.can_axes.txpdo-%02d.data-0", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.txpdo-%02d.data-0', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesDataRx[i*2+1]), pHPI->Config.iCompId, "%s.dbg.can_axes.txpdo-%02d.data-1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.txpdo-%02d.data-1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}     
		
		retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesRxMissing[i]), pHPI->Config.iCompId, "%s.dbg.can_axes.txpdo-%02d.miss", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.txpdo-%02d.miss', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}    
		pHPI->HalPars.Dbg.CanAxesRxMissing[i] = 0;
    } 

	retval = hal_pin_bit_newf(HAL_IO, &(pHPI->HalPins.CanAxes.RxOverrun), pHPI->Config.iCompId, "%s.can_axes.rx_overrun", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.rx_overrun', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RxOverrun) = 0; 

	retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.RxTimeout), pHPI->Config.iCompId, "%s.can_axes.rx_timeout", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.rx_timeout', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RxTimeout) = 0; 
	
	
	for (i = 0; i < 5; i++)
	{
		retval = hal_pin_u32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pCanAxesFwVer[i]), pHPI->Config.iCompId, "%s.can_axes.fw_ver-%02d", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.fw_ver-%02d', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}	
		*(pHPI->HalPins.CanAxes.pCanAxesFwVer[i]) = 0;

		//pin relativi al controllo di posizione

		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_cmd[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.position-cmd", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.position-cmd', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_cmd[i]) = 0.0;		
		
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_fb[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.position-fb", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.position-fb', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_fb[i]) = 0.0;		
		
		
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_fb_by_can[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-fb-by-can", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-fb-by-can', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_fb_by_can[i]) = 0.0;			

		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_fb_window[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-fb-window", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-fb-window', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_fb_window[i]) = 0.005;		
		
		retval = hal_pin_s32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_fb_by_can_counts[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-fb-by-can-counts", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-fb-by-can-counts', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_fb_by_can_counts[i]) = 0;	

		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.enable[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.enable", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.enable', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.enable[i]) = 0;	

		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_reg_e[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-e", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-e', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_e[i]) = 0.0;				

		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_reg_p[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-p", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-p', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_p[i]) = 0.0;
	
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_reg_i[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-i", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-i', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_i[i]) = 0.0;
		
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_reg_pi[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-pi", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-pi', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_pi[i]) = 0.0;		
	
		//"param" pins

		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_scale[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.position-scale", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.position-scale', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_scale[i]) = 1.0;
		pHPI->position_scale_rev[i] = 1.0;

		retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_reg_time[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-time", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-time', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_time[i]) = 3;	

		retval = hal_pin_u32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.position_reg_timer[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-timer", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-timer', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_timer[i]) = 0;			
		
		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_reg_kp[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-kp", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-kp', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_kp[i]) = 0.3;		

		retval = hal_pin_float_newf(HAL_IN, &(pHPI->HalPins.CanAxes.position_reg_ki[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.pos-reg-ki", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.pos-reg-ki', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.position_reg_ki[i]) = 0.3;

		
		// debug pins
		
		retval = hal_pin_s32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.test1[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.test1", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.test1', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.test1[i]) = 0;

		retval = hal_pin_s32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.test2[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.test2", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.test2', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.test2[i]) = 0;

		retval = hal_pin_s32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.test3[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.test3", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.test3', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.test3[i]) = 0;
		
		retval = hal_pin_s32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.test4[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.test4", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.test4', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.test4[i]) = 0;	

		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.test5[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.test5", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.test5', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.test5[i]) = 0.0;			
		
		//altri pin relativi al can
		
		retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pCurrent[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.current", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.current', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.pCurrent[i]) = 0.0;
		
		
		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pAlarmReset[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.alarm_reset", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.alarm_reset', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pAlarmReset[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pGoHome[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.go_home", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.go_home', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pGoHome[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pPositionReset[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.position_reset", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.position_reset', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pPositionReset[i]) = 0; 		
		
		retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pAuxFunctions[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.aux_functions", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.aux_functions', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.pAuxFunctions[i]) = 0;			
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pEnabled[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.enabled", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.enabled', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pEnabled[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pFault[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.fault", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.fault', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pFault[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pEmergency[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.emergency", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.emergency', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pEmergency[i]) = 0; 

		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pHome[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.home", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.home', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pHome[i]) = 0; 

		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pLimCW[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.lim_cw", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.lim_cw', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pLimCW[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pLimCCW[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.lim_ccw", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.lim_ccw', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pLimCCW[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pHoming[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.homing", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.homing', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pHoming[i]) = 0; 
		
		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pHomed[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.homed", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.homed', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pHomed[i]) = 0; 

		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pBrake[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.brake", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.brake', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pBrake[i]) = 0; 

		retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pOutDig[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.out_dig", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.out_dig', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}		
		*(pHPI->HalPins.CanAxes.pOutDig[i]) = 0;

		retval = hal_pin_u32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pFaultCode[i]), pHPI->Config.iCompId, "%s.can_axes.%02d.fault_code", pHPI->Config.zsHalName, i);
		if (retval < 0) 
		{
			HPI_ERR("ERROR: exporting pin '%s.can_axes.%02d.fault_code', err: %d\n", pHPI->Config.zsHalName, i, retval);
			hal_exit(pHPI->Config.iCompId);
			return -1;
		}
		*(pHPI->HalPins.CanAxes.pFaultCode[i]) = 0;
		
	}


	//CAN Axes SDO 
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSDO_TimeoutTime), pHPI->Config.iCompId, "%s.can_axes.sdo_timeout", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.sdo_timeout', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	*(pHPI->HalPins.CanAxes.pSDO_TimeoutTime) = 10;	//default timeout richieste SDO [ms]	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleSDO_TimeoutTime), pHPI->Config.iCompId, "%s.can_axes.spindle_sdo_timeout", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_sdo_timeout', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	*(pHPI->HalPins.CanAxes.pSpindleSDO_TimeoutTime) = 50;	//default timeout richieste SDO verso mandrino [ms]		
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindlePollingTime), pHPI->Config.iCompId, "%s.can_axes.spindle_poll_time", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.can_axes.spindle_poll_time', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	*(pHPI->HalPins.CanAxes.pSpindlePollingTime) = 0;			//per default il polling del mandrino e' disabilitato
	
	retval = hal_pin_u32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pSpindleFaultCode), pHPI->Config.iCompId, "%s.can_axes.spindle_fault_code", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_fault_code', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleFaultCode) = 0;
	
	retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pSpindleCurrent), pHPI->Config.iCompId, "%s.can_axes.spindle_current", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_current', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleCurrent) = 0.0;	
	
	retval = hal_pin_u32_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pSpindleCanErrorCount), pHPI->Config.iCompId, "%s.can_axes.spindle_error_count", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_error_count', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleCanErrorCount) = 0;
	
	
	
	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleFault), pHPI->Config.iCompId, "%s.can_axes.spindle_fault", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_fault', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleFault) = 0; 		
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleWriteAddress), pHPI->Config.iCompId, "%s.can_axes.spindle_write_address", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_write_address', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleWriteAddress) = 0x004F;			//indirizzo oggetto "Error reset" del drive S&M SD4S 		
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleWriteData), pHPI->Config.iCompId, "%s.can_axes.spindle_write_data", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_write_data', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleWriteData) = 1; 				//valore per reset allarmi su drive S&M SD4S	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleWriteU16_Trigger), pHPI->Config.iCompId, "%s.can_axes.spindle_write_u16_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_write_u16_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleWriteU16_Trigger) = pHPI->HalPins.CanAxes.SpindleWriteU16_TriggerOld = 0; 	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleWriteU32_Trigger), pHPI->Config.iCompId, "%s.can_axes.spindle_write_u32_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_write_u32_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleWriteU32_Trigger) = pHPI->HalPins.CanAxes.SpindleWriteU32_TriggerOld = 0;	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleSM_SD4S_FaultResetTrigger), pHPI->Config.iCompId, "%s.can_axes.spindle_sm_sd4s_fault_reset", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_sm_sd4s_fault_reset', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pSpindleSM_SD4S_FaultResetTrigger) = pHPI->HalPins.CanAxes.SpindleSM_SD4S_FaultResetTriggerOld = 0;	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pCanAxesFwVerQueryTrigger), pHPI->Config.iCompId, "%s.can_axes.fw_ver_read_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.fw_ver_read_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pCanAxesFwVerQueryTrigger) = 1;		//per eseguire automaticamente la lettura delleversioni all'avvio
	pHPI->HalPins.CanAxes.CanAxesFwVerQueryTriggerOld = 0;		
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleFaultCodeRegAddress), pHPI->Config.iCompId, "%s.can_axes.spindle_fault_reg_addr", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_fault_reg_addr', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	*(pHPI->HalPins.CanAxes.pSpindleFaultCodeRegAddress) = 0x2000 + 291;	//indirizzo modbus registro allarmi S16005	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.pSpindleCurrentRegAddress), pHPI->Config.iCompId, "%s.can_axes.spindle_current_reg_addr", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.spindle_current_reg_addr', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}	
	*(pHPI->HalPins.CanAxes.pSpindleCurrentRegAddress) = 0x2000 + 257;	//indirizzo modbus registro corrente uscita S16005		
	
#ifdef	TARGET_S16007		
	retval = hal_pin_float_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.pFlowFreq), pHPI->Config.iCompId, "%s.can_axes.flow_freq", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.flow_freq', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.pFlowFreq) = 0.0;		
#endif	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterAccessNode), pHPI->Config.iCompId, "%s.can_axes.reg_access.node", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.node', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterAccessNode) = 1; 	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterAccessAddress), pHPI->Config.iCompId, "%s.can_axes.reg_access.address", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.address', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterAccessAddress) = 0; 	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterRead_Trigger), pHPI->Config.iCompId, "%s.can_axes.reg_access.read_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.read_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterRead_Trigger) = pHPI->HalPins.CanAxes.RegisterRead_TriggerOld = 0; 	
		
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterWriteU16_Trigger), pHPI->Config.iCompId, "%s.can_axes.reg_access.write_u16_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.write_u16_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterWriteU16_Trigger) = pHPI->HalPins.CanAxes.RegisterWriteU16_TriggerOld = 0; 	
	
	retval = hal_pin_bit_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterWriteU32_Trigger), pHPI->Config.iCompId, "%s.can_axes.reg_access.write_u32_trigger", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.write_u32_trigger', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterWriteU32_Trigger) = pHPI->HalPins.CanAxes.RegisterWriteU32_TriggerOld = 0; 	
	
	retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.RegisterAccessDone), pHPI->Config.iCompId, "%s.can_axes.reg_access.done", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.done', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterAccessDone) = 0; 	
	
	retval = hal_pin_bit_newf(HAL_OUT, &(pHPI->HalPins.CanAxes.RegisterAccessFail), pHPI->Config.iCompId, "%s.can_axes.reg_access.fail", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.fail', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterAccessFail) = 0; 	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterDataIn), pHPI->Config.iCompId, "%s.can_axes.reg_access.data_in", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.data_in', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterDataIn) = 0; 	
	
	retval = hal_pin_u32_newf(HAL_IN, &(pHPI->HalPins.CanAxes.RegisterDataOut), pHPI->Config.iCompId, "%s.can_axes.reg_access.data_out", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting pin '%s.can_axes.reg_access.data_out', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	*(pHPI->HalPins.CanAxes.RegisterDataOut) = 0; 	
	
	retval = hal_param_bit_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesSdoRequest), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.request", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.request', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.CanAxesSdoRequest = 0; 
	
	retval = hal_param_bit_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesSdoReply), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.reply", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.reply', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.CanAxesSdoReply = 0; 
	
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.CanAxesSdoTargetNode), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.target_node", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.target_node', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.CanAxesSdoTargetNode = 1; 
		
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.pCanAxesSdoRequestData[0]), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.request.data-0", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.request.data-0', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.pCanAxesSdoRequestData[0] = 0; 
	
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.pCanAxesSdoRequestData[1]), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.request.data-1", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.request.data-1', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.pCanAxesSdoRequestData[1] = 0; 
	
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.pCanAxesSdoReplyData[0]), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.reply.data-0", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.reply.data-0', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.pCanAxesSdoReplyData[0] = 0; 
	
	retval = hal_param_u32_newf(HAL_RO, &(pHPI->HalPars.Dbg.pCanAxesSdoReplyData[1]), pHPI->Config.iCompId, "%s.dbg.can_axes.sdo.reply.data-1", pHPI->Config.zsHalName);
	if (retval < 0) 
	{
		HPI_ERR("ERROR: exporting parameter '%s.dbg.can_axes.sdo.reply.data-1', err: %d\n", pHPI->Config.zsHalName, retval);
		hal_exit(pHPI->Config.iCompId);
		return -1;
	}		
	pHPI->HalPars.Dbg.pCanAxesSdoReplyData[1] = 0; 	



	
	//altre inizializzazioni
 
 
 	//------------------------------------------------------------------
   //Export functions
    rtapi_snprintf(name, sizeof(name), "%s.write", hci_halname);
    retval = hal_export_funct(name, hci_write, pHCI, 1, 0, comp_id);
    if (retval != 0) 
	{
        HPI_ERR("ERROR: function '%s\n' export failed, err: %d\n", name, retval);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_snprintf(name, sizeof(name), "%s.read", hci_halname);
    retval = hal_export_funct(name, hci_read, pHPI, 1, 0, comp_id);
    if (retval != 0) 
	{
        HPI_ERR("ERROR: function '%s\n' export failed, err: %d\n", name, retval);
        hal_exit(comp_id);
        return -1;
    }
 
	return 0;
}
