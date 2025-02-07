//----------------------------------------------------------------------
 /*
 hal_can_inel.h                                                     
                                                                     
 Author(s): Gianfranco Rosso                                         
                                                                     
 Major Changes:                                                      
 2015-Aug    Gianfranco Rosso										
			   Support for Inel Elettronica S16002 + S16001 + S16003 boards
 2016-02-16  Gianfranco Rosso										
			   Protocollo CAN V2, freq. I2C da 400 a 100KHz e tempo 
             ciclo rinfresco I2C da 1 a 5ms							
 2016-02-16  Gianfranco Rosso										
			   I2C bus clear migliorato								
 2016-03-05  Gianfranco Rosso										
			   scrittura su driver stepper SPI con rilettura e retry
 2016-06-18  Gianfranco Rosso										
               filtraggio ingressi digitali (su 3 campioni -> 10ms)	
 2016-06-21  Gianfranco Rosso				
               gestione S16003.01 con compilazione condizionale per	
               mantenere compatibilita' con hw precedente (2 eseguibili)
               GPB2 e GPB3 del secondo I/O expander trasformate da 
               ingressi (IN_32, IN_33) in uscite OUT40 OUT41
 2016-07-12  Gianfranco Rosso				
               inizializzazione registri motori stepper ad ogni abilitazione
			   dell'asse (le sicurezze di macchina prevedono il sezionamento 
			   dell'alimentazione del driver che va quindi riprogrammato)
 2016-07-19  Gianfranco Rosso				
               mascheratura bit read-only del registro STATUS dei driver stepper
			   durante verifica a fine scrittura
 2016-07-30  Gianfranco Rosso				
               watchdog su PRU con reset uscite I/O expander e stop esecuzione PRU
 2016-09-02  Gianfranco Rosso				
               gestione automatica 2 livelli di corrente motori stepper (con passaggio
			   al livello basso dopo il tempo impostato dall'ultimo movimento)
 2016-09-27  Gianfranco Rosso				
               watchdog comunicazione CAN con assi (assenza comunicazione per un numero 
			   di cicli consecutivi parametrizzato) per problema visto su M3/M5 (S16001 con fw stepper)
			   in cui uno o piu' assi interrompono la comunicazione CAN (sia in ricezione che in trasmissione)
 2016-11-15  Gianfranco Rosso				
               correzione "baco" invio SDO: col nuovo protocollo non puo' piu' aspettare di aver ricevuto
			   le risposte da tutti gli assi (perche' non rispondono mai tutti assieme) quindi aspetta solo
			   le risposte degli assi 4 e 5
 2016-11-26  Gianfranco Rosso				
               collecting dei codici errore assi via SDO e polling codice errore e corrente mandrino	
			   (modificato solo codice driver hal su ARM, codice PRU resta il precedente)
			   
 2019-04-09  Gianfranco Rosso
			   Problema: se la macchina viene spenta e riaccesa velocemente le uscite degli I/O expander assumono valori indesiderati
			   durante l'inizializzazione e poi non cambiano piu'. Analisi: si è verificato con l'oscilloscopio che durante l'inizializzazione
			   delle periferiche connesse al bus I2C nel caso in questione fallisce la scrittura sull'ADC MCP3428 (risponde NACK sul byte di address).
			   Questo fa abortire la trasmissione I2C e nella FIFO restano i dati non trasmessi, che usciranno nelle successive trasmissioni (in pratica 
			   i byte di dato risultano tutti traslati e quindi vengono trasmessi indirizzi registri e dati non previsti per i vari dispositivi). 
			   Il problema non si risolve riavviando linuxcnc (quindi riavviando le pru e tutta l'inizializzazione del bus) ma solo spegnendo e riaccendendo.
			   Ipotesi: l'ADC non risponde perche' non riconosce l'indirizzo (sul datasheet e' indicato che i pin di indirizzo vengono campionati solo al
			   power-on o in conseguenza dei comandi di general call, quindi magari all'accensione, dopo un brevissimo spegnimento, vengono acquisiti male).
			   Soluzione (base): all'inizio dell'inizializzazione dei dispositivi viene generata una GENERAL-CALL I2C con comando di reset e acquisizione
			   dell'indirizzo hw.
			   Soluzione (avanzata): in aggiunta alla soluzione base, migliorata sequenza di BUS CLEAR dell' I2C con aggiunta dello STOP alla fine.
			   Check del NACK ricevuto durante trasmissione con conseguente reset della FIFO RX e TX e skip della sequenza I2C interrotta dal NACK (se
			   succede durante inizializzazione riparte dall'inizio). Aggiunto infine un contatore dei nack pubblicato come parametro hal (.dbg.i2c.nack_count).
			   Aggiunto parametro hal con versione driver pru (.dbg.pru_fw_ver)

 2019-04-16  Gianfranco Rosso	
			   v01.00.00-00
			   Aggiunti pin hal (.i2c_cycle_count e i2c_nack_count) con conteggio cicli I2c e nack (per rilevazione/segnalazione problemi ad alto livello). Aggiunto anche parametro con
			   versione driver hal_pru_inel (.dbg.hal_pru_inel_ver)
			   
 2020-06-10  Gianfranco Rosso
			   v01.01.00-00
			   Aggiunti pin hal (.CanAxes.pPositionReset[5]) per invio camando reset posizione agli assi, aggiornato parametro .dbg.hal_pru_inel_ver (HAL_PRU_INEL_VER) 			
   
 2021-01-12  Gianfranco Rosso	
			   v01.02.00-00
			   Aggiunti pin hal (.CanAxes.pAuxFunctions[5]) per invio camandi agli assi (0=NONE, 1=Force Brake Disable), aggiornato parametro .dbg.hal_pru_inel_ver (HAL_PRU_INEL_VER) 	
			   
 2022-03-05  Gianfranco Rosso	
			   v01.03.00-00 e pru v01.01.00-00 
			   Gestione "protocollo CIALDA": gestione 2 nuovi assi via CAN per caricatore/magazzino cialde CN Worldmec. Per ognuno dei 2 nuovi assi aggiunti pin hal per 
			   selezione quota (tra le 32 disponibili) da raggiungere, per comando delle 3 uscite digitali disponibili, per leggere quota attuale, stato dei 4 ingressi 
			   digitali disponibili, flag di posizione raggiunta e flag di fault. Il ciclo di aggiornamento dei nuovi assi dura 4ms (messaggi CAN intervallati tra gli altri
			   gia' presenti per la gestione degli assi del CN).
			   Il nuovo protocollo CAN (v3) e' un'estensione del v2 e si attiva tramite il parametro hal "hpi_can_proto=3"
			   Aggiornato parametro .dbg.hal_pru_inel_ver (HAL_PRU_INEL_VER) e .dbg.pru_fw_ver (FW_VER)
			   
 2023-01-24  Gianfranco Rosso	
			   v02.00.00-00 e pru v02.00.00-00 
			   Gestione S16007 per macchina Worldmec DS1. Motori stepper per i 5assi gestiti da uC STM32G4 (ogni micro gestisce fino a 2 motori). Non ci sono dispositivi su I2c, su SPI 
			   c'e' solo il terzo uC (quello che gestisce "solo" il quinto motore) per l'I/O speciale/aggiuntivo. Comando degli assi via CAN. Eliminata gestione anche del CAN AUX.
			   N.B. una volta compilati gli eseguibili vanno rinominati in _S16007 in modo da distinguerli chiaramente da quelli per le S16001-2-3
			   
 2023-12-23  Gianfranco Rosso	
			   v02.01.00-00 e pru v02.01.00-00 
			   Su S16007 gestione polling via SDO della frequenza flussostato letto dal primo micro (quello degli assi 1-2, l'informazione e' disponibile sui registri modbus dell'asse 1, 
			   il fw degli assi deve essere almeno v1.03).
			   Per tutte le schede inoltre, per generalizzare e permettere l'uso di driver mandrini anche di terze parti, TUTTI gli indirizzi degli oggetti (associati a registri modbus o no) per 
			   la lettura tramite SDO (ad es. per la corrente e dettaglio allarmi) sono specificati per intero (non viene piu' aggiunto automaticamente l'offset 0x2000). La modifica di questa 
			   convenzione e' estesa per uniformita' anche agli indirizzi del dettaglio allarmi degli assi e all'indirizzo della freq. del flussostato nella S16007.
			   Sono stati aggiunti inoltre 2 parametri hal per i k di conversione della corrente mandrino e della freq. flussostato.
			   Il codice della pru non e' stato modificato ma la compilazione per il doppio target (S16001-2-3 e S16007) ha richiesto un ritocco dei file e quindi si e' preferito aggiornare anche
			   la revisione per identificare univocamente i binari
			   
 2024-01-13  Gianfranco Rosso	
			   v02.02.00-00 (versione pru immutata)
			   Aggiunti pin dell'hal per l'invio di un SDO al nodo mandrino (ad esempio per comandare il reset degli allarmi sul driver S&M SD4S). Compilato solo per S16001-2-3
			   
 2024-01-25  Gianfranco Rosso	
			   v02.03.00-00 e pru v02.03.00-00
			   Nuova compilazione condizionale TARGET_S16003_02 per S16001-2-3 rev 02 con gestione dei motori stepper per cambio utensile e cambio cialda tramite driver STM L6482 (SOLO SPI) 
			   invece che TI DRV8711 (step/dir + SPI per configurazione). Aggiunti anche tutti i pin hal e le routine di gestione dell'asse prima presenti in hal_pru_generic per gli stepgen.
			   
 2024-07-18  Gianfranco Rosso
			   v02.04.00-00 (versione pru immutata)
			   compilato solo per per S16001-2-3 con TARGET_S16003_01, aggiunto pin hal per reset allarmi del drive mandrino S&M SD45 (automatizza sequenza invio SDO per reset allarmi)
			   
 2024-07-27  Gianfranco Rosso	
			   v02.05.00-00 e pru v02.05.00-00			   
			   compilato solo per per S16001-2-3 con TARGET_S16003_02, aggiunto parametro (sulla riga di comando caricamento modulo) per bitrare SPI
			   
 2024-08-03  Gianfranco Rosso
			   v02.06.00-00 (versione pru immutata, v02.05.00-00, ricompilata per il target)
			   compilato solo per per S16001-2-3 con TARGET_S16003_01, parametri hal hpi.can_axes.spindle_fault_reg_addr e hpi.can_axes.spindle_current_reg_addr trasformati in pin	

 2024-08-08  Gianfranco Rosso
			   v02.07.00-00 (versione pru immutata, v02.05.00-00, ricompilata per il target)
			   compilato solo per per S16001-2-3 con TARGET_S16003_02, usa comando per stepper L6482 STEPPER_CMD_GO_TO_DIR invece di STEPPER_CMD_GO_TO, banda morta su controllo posizione 
			   stepper (equivalente a 1 micropasso)			  

 2024-08-29  Gianfranco Rosso
			   v02.08.00-00 (versione pru immutata, v02.05.00-00)
			   compilato solo per per S16001-2-3 con TARGET_S16003_02, usa nuovi pin "dir" e "dirack" per gli stepper per impostare la direzione sui driver stepper L6482: il clock viene generato dagli
			   stepgen in hal_pru_generic come sulle versioni _01 (e _00). Questa modalita' viene abilitata quando il pin "L6482-control-type" e' posto a 2 (trasformato da bit a s32)
			   
 2024-10-03  Gianfranco Rosso
			   v02.09.00-00 (versione pru immutata, v02.05.00-00)	
			   compilato solo per per S16001-2-3 con TARGET_S16003_02, gestisce 2 livelli di corrente (in movimento e da fermo) quando L6482-control-type == 2
			   ossia nel controllo clock esterno + dir spi (in questa modalita' il driver L6482 considera il motore sempre fermo quindi non sono
			   sfruttabili i diversi livelli di corrente programmabili nelle altre modalita')
			   
 2024-10-24  Gianfranco Rosso
			   v02.0A.00-00 (versione pru immutata, v02.05.00-00, ricompilata per il target)
			   compilato solo per per S16001-2-3 con TARGET_S16003_01, fix baco gestione SDO generico e reset allarmi verso mandrino
   
 2024-10-31  Gianfranco Rosso
			   v02.0B.00-00 (versione pru immutata, v02.05.00-00, ricompilata per i target)
			   compilato sia per per S16001-2-3 con TARGET_S16003_01 che per S16007 con TARGET_S16007, lettura versioni fw assi tramite SDO una-tantum (all'avvio)
			   ma volendo c'e' apposito pin di trigger
			   parametro hal spindle_poll_time convertito in pin can_axes.fw_ver_read_trigger
			   aggiunto pin hal di input can_axes.spindle_fault per triggerare lettura codice allarme mandrino
			   aggiunto parametro hal can_axes.spindle_sdo_timeout dedicato al mandrino (can_axes.sdo_timeout resta per gli SDO verso gli assi e SDO generico)
			   
 2024-10-31  Gianfranco Rosso
			   v02.0C.00-00 (versione pru immutata, v02.05.00-00)
			   parametri hal hpi.dbg.can_axes.fw_ver-xx trasformati in pin hpi.can_axes.fw_ver-xx
			   
*/
//----------------------------------------------------------------------//
 
#ifndef _hal_can_inel_H_
#define _hal_can_inel_H_

#define INT32_MIN (-2147483647-1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295U)
//#include <stdint.h>

#include "config_module.h"
#include RTAPI_INC_LIST_H
#include "rtapi.h"
#include "hal.h"

//#include "pru_inel.h"

#define HCI_NAME    "HCI"

#define HCI_PRINT(fmt, args...)  rtapi_print(HCI_NAME ": " fmt, ## args)

#define HCI_ERR(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  HCI_NAME ": " fmt, ## args)
#define HCI_WARN(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, HCI_NAME ": " fmt, ## args)
#define HCI_INFO(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, HCI_NAME ": " fmt, ## args)
#define HCI_DBG(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  HCI_NAME ": " fmt, ## args)


/***********************************************************************
*                   STRUCTURES AND GLOBAL VARIABLES                    *
************************************************************************/

/*
enum SDO_StatusEnum
{
	SDO_STATUS_IDLE							= 0,
	
	SDO_STATUS_SPINDLE_CURRENT_SEND			,
	SDO_STATUS_SPINDLE_CURRENT_WAIT			,
	SDO_STATUS_SPINDLE_FAULT_SEND			,
	SDO_STATUS_SPINDLE_FAULT_WAIT			,
	
	SDO_STATUS_AXES1_FAULT_SEND				,
	SDO_STATUS_AXES2_FAULT_SEND				,
	SDO_STATUS_AXES3_FAULT_SEND				,
	SDO_STATUS_AXES4_FAULT_SEND				,
	SDO_STATUS_AXES5_FAULT_SEND				,
	
	SDO_STATUS_AXES1_FAULT_WAIT				,
	SDO_STATUS_AXES2_FAULT_WAIT				,
	SDO_STATUS_AXES3_FAULT_WAIT				,
	SDO_STATUS_AXES4_FAULT_WAIT				,
	SDO_STATUS_AXES5_FAULT_WAIT				,
	
	SDO_STATUS_AXES1_FW_VER_SEND			,
	SDO_STATUS_AXES2_FW_VER_SEND			,
	SDO_STATUS_AXES3_FW_VER_SEND			,
	SDO_STATUS_AXES4_FW_VER_SEND			,
	SDO_STATUS_AXES5_FW_VER_SEND			,
	
	SDO_STATUS_AXES1_FW_VER_WAIT			,
	SDO_STATUS_AXES2_FW_VER_WAIT			,
	SDO_STATUS_AXES3_FW_VER_WAIT			,
	SDO_STATUS_AXES4_FW_VER_WAIT			,
	SDO_STATUS_AXES5_FW_VER_WAIT			,	
	
	SDO_STATUS_REG_READ_SEND				,
	SDO_STATUS_REG_READ_WAIT				,

	SDO_STATUS_REG16_WRITE_SEND				,
	SDO_STATUS_REG16_WRITE_WAIT				,

	SDO_STATUS_REG32_WRITE_SEND				,
	SDO_STATUS_REG32_WRITE_WAIT				,
	
	SDO_STATUS_SPINDLE_REG16_WRITE_SEND		,
	SDO_STATUS_SPINDLE_REG16_WRITE_WAIT		,

	SDO_STATUS_SPINDLE_REG32_WRITE_SEND		,
	SDO_STATUS_SPINDLE_REG32_WRITE_WAIT		,

	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_SEND		,
	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_WAIT		,
	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_SEND1	,
	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_WAIT1	,
	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_SEND2	,
	SDO_STATUS_SPINDLE_SM_SD4S_FAULT_RESET_WAIT2	,
	
};
*/


typedef struct {

    struct {
        int iCompId;
        const char* zsModName;
        const char* zsHalName;
		u8 bForceWrite;
    } Config;

	//puntatori alla PRU RAM
	
	
	u32 pdwIOexpInputs[3];					//memorie stato ingressi IOexp per filtro
	u32 dwIOexpInputs;						//valore filtrato dello stato ingressi IOexp
	
	double position_scale_rev[5];			//inverso di position_scale (1/position_scale)
		
	hal_s32_t position_cmd_counts[5];
	hal_s32_t commanded_pos_counts[5];
	double position_cmd_delta[5];
	s32 sdwStepsPerPeriod[5];					//step x ms
	s32 sdwStepsPerPeriodLatch[5];
	
	TxPDO_t pTxPDO[5+2];
	
	u8 bSDO_Status;							//stato SM degli SDO (per polling mandrino, codici allarmi assi e richieste SDO esterne)
	u32 dwSDO_TimeoutTimer;					//timer/contatore per timeout richieste SDO
	u32 dwSpindleSDO_TimeoutTimer;			//timer/contatore per timeout richieste SDO verso mandrino
	u32 dwSpindlePollingTimer;				//timer/contatore per periodo polling mandrino
	u32 dwAxesFaultCodeHoldOffTimer;		//timer/contatore per rinfresco codici allarmi
	u32 dwSpindleFaultCodeHoldOffTimer;		//timer/contatore per rinfresco codice allarme mandrino
		
	//HAL parameters
	struct {
		hal_bit_t CanAxesSyncEn;
		hal_u32_t CanAxesRxTimeout;			// timeout ricezione assi (watchdog assi) [ms]

		hal_u32_t AxesFaultCodeRegAddress;	//indirizzo registro codice allarmi degli assi
		hal_u32_t AxesFwVersionRegAddress;	//indirizzo registro versione fw degli assi
		
		hal_u32_t SpindleCAN_Node;			//indirizzo CAN del mandrino
		hal_float_t SpindleCurrentK;		//fattore per conversione valore corrente uscita mandrino 
			
		hal_bit_t IOexpInputsInv[IOEXP_IN_PINS];
		hal_bit_t IOexpOutputsInv[IOEXP_OUT_PINS];
		hal_u32_t IOexpInputFiltEn;

		hal_float_t DacScales[DAC_OUTPUTS];
		
		//dbg
		struct {
			hal_u32_t HalUpdateCount;
			
			hal_u32_t HalCanInelVer;
/*			
			hal_u32_t CanAxesBusOffCount;
			hal_u32_t CanAxesReInitCount;
			hal_u32_t CanAxesTxParityErrorCount;
			hal_u32_t CanAxesRxParityErrorCount;
			hal_u32_t CanAxesParityErrorAccazzus;
			hal_u32_t CanAxesTxErrorCounter;
			hal_u32_t CanAxesRxErrorCounter;
*/
			hal_u32_t CanAxesDataTx1[2];
			hal_u32_t CanAxesDataTx2[2];
			hal_u32_t CanAxesDataTx3[2];
			hal_u32_t CanAxesDataTx4[2];
			hal_u32_t CanAxesDataRx[2*(5+2)];

			hal_u32_t CanAxesRxMissing[5+2];
			
			hal_bit_t CanAxesSdoRequest;
			hal_u32_t CanAxesSdoTargetNode;
			hal_u32_t pCanAxesSdoRequestData[2];
			
			hal_bit_t CanAxesSdoReply;
			hal_u32_t pCanAxesSdoReplyData[2];			
			
		} Dbg;
	} HalPars;
 
	//HAL pins
	struct {
#ifndef TARGET_S16007		
		hal_u32_t* pI2cCycleCount;
		hal_u32_t* pI2cNackCount;
#endif		
		
		//ioexp data
		hal_bit_t* pIOexpInputs[IOEXP_IN_PINS]; 	// array of pointers to bits
		hal_bit_t* pIOexpOutputs[IOEXP_OUT_PINS]; 	// array of pointers to bits

		hal_bit_t* pDacEnables[DAC_OUTPUTS];
		hal_float_t* pDacValues[DAC_OUTPUTS];
#ifndef TARGET_S16007		
		hal_float_t* pAdcValues[ADC_INPUTS];
#endif		
		
		struct {
			hal_bit_t* 		RxTimeout;
			
			//controllo di posizione assi ("come" stepgen software)
            hal_bit_t*		enable[5];
			
            hal_float_t*	position_cmd[5];
            hal_float_t*	position_fb[5];
			hal_float_t*	position_fb_by_can[5];
			hal_float_t*	position_fb_window[5];
 
            hal_float_t*	position_scale[5];
 			
			hal_u32_t*		position_reg_time[5];
			hal_u32_t*		position_reg_timer[5];
			hal_float_t* 	position_reg_kp[5];
			hal_float_t* 	position_reg_ki[5];
			hal_float_t*	position_reg_e[5];
			hal_float_t* 	position_reg_p[5];
			hal_float_t* 	position_reg_i[5];
 			hal_float_t* 	position_reg_pi[5];

            // debug pins
            hal_s32_t*		test1[5];
            hal_s32_t*		test2[5];
            hal_s32_t*		test3[5];
			hal_s32_t*		test4[5];
			hal_float_t*	test5[5];
			
			//info trasmesse e ricevute dagli assi via PDO
			hal_bit_t* RxOverrun;
 
			hal_s32_t* position_fb_by_can_counts[5+2];

			hal_float_t* pCurrent[5];		// corrente motore [A]

			hal_bit_t* pAlarmReset[5];		// comando reset allarmi (in OR col comando da morsettiera)
			hal_bit_t* pGoHome[5];			// comando home
			hal_bit_t* pPositionReset[5];	// comando reset posizione		
			hal_u32_t* pAuxFunctions[5];	// comandi ausiliari (solo 3 bit, quindi range 0..7)

			hal_bit_t* pEnabled[5];			// stato asse abilitato (motore in coppia)
			hal_bit_t* pFault[5+2];			// stato presenza allarme
			hal_bit_t* pEmergency[5];		// stato emergenza (0 alimentazione potenza sezionata / 1 alimentazione potenza presente)
			
			hal_bit_t* pHome[5+2];			// stato ingresso di Home
			hal_bit_t* pLimCW[5+2];			// stato ingresso finecorsa CW
			hal_bit_t* pLimCCW[5+2];		// stato ingresso finecorsa CCW
			
			hal_bit_t* pHoming[5+2];		// stato ricerca di zero in corso
			hal_bit_t* pHomed[5+2];			// stato posizione di Home raggiunta (set su Home, reset su tacca zero encoder dopo uscita da Home)
			
			hal_bit_t* pBrake[5];			// stato uscita freno motore
			hal_bit_t* pOutDig[5];			// stato uscita digitale configurabile	

			hal_bit_t* pOutFault[2];		// comando uscita digitale Fault su assi 6/7
			hal_bit_t* pOutMotion[2];		// comando uscita digitale Motion su assi 6/7	
			hal_bit_t* pOutFree[2];			// comando uscita digitale Free su assi 6/7	
			
			hal_u32_t* pPosSel[2];			// comando assi 6/7 selezione posizione da raggiungere (e mantenere in coppia fintanto che resta comandata), 
											// la posizione 0 indica motore non in coppia (solo 5 bit, quindi range 0..31)
											

			//info trasmesse e ricevute dagli assi via SDO
			hal_u32_t* pFaultCode[5];		// codice allarme

			hal_u32_t* pSpindlePollingTime;	//periodo di polling mandrino [ms] (0 = polling disabilitato)
			
			hal_u32_t* pSpindleFaultCode;	// codice allarme mandrino
			hal_float_t* pSpindleCurrent;	// corrente mandrino [A]
			hal_u32_t* pSpindleCanErrorCount;	// contatore errori comunicazione CAN col mandrino
			
			hal_u32_t* pSpindleWriteAddress;		//indirizzo registro (object) del mandrino da scrivere tramite SDO
			hal_u32_t* pSpindleWriteData;			//dato da scrivere nel registro del mandrino tramite SDO
			hal_bit_t* pSpindleWriteU16_Trigger;	//trigger per scrittura di un registro a 16bit del mandrino tramite SDO
			hal_bit_t* pSpindleWriteU32_Trigger;	//trigger per scrittura di un registro a 32bit del mandrino tramite SDO

			hal_u32_t* pSpindleSDO_TimeoutTime;		//tempo timeout richieste SDO verso mandrino[ms]
			
			hal_bit_t* pSpindleSM_SD4S_FaultResetTrigger;	//trigger per invio sequenza SDO per reset allarmi drive mandrino S&M SD4S
			
			hal_u32_t* pSpindleFaultCodeRegAddress;	//indirizzo registro codice allarmi del mandrino
			hal_u32_t* pSpindleCurrentRegAddress;	//indirizzo registro corrente uscita mandrino			
			
			hal_bit_t* pSpindleFault;		//trigger per lettura codice allarme 
			hal_bit_t* pCanAxesFwVerQueryTrigger;	//trigger lettura versione fw assi
			
			hal_u32_t* pCanAxesFwVer[5];

			hal_bit_t SpindleWriteU16_TriggerOld;
			hal_bit_t SpindleWriteU32_TriggerOld;	
			
			hal_bit_t SpindleSM_SD4S_FaultResetTriggerOld;
			
			hal_bit_t  CanAxesFwVerQueryTriggerOld;
			
#
			//lettura/scrittura registri assi tramite SDO
			hal_u32_t* RegisterAccessNode;
			hal_u32_t* RegisterAccessAddress;
			
			hal_bit_t* RegisterRead_Trigger;
			hal_bit_t* RegisterWriteU16_Trigger;
			hal_bit_t* RegisterWriteU32_Trigger;
			hal_bit_t* RegisterAccessDone;
			hal_bit_t* RegisterAccessFail;
		
			hal_u32_t* RegisterDataIn;
			hal_u32_t* RegisterDataOut;			

			hal_u32_t* pSDO_TimeoutTime;			//tempo timeout richieste SDO [ms]
	 		
			hal_bit_t RegisterRead_TriggerOld;
			hal_bit_t RegisterWriteU16_TriggerOld;
			hal_bit_t RegisterWriteU32_TriggerOld;
			
		} CanAxes;
		
	} HalPins;

} HalCanInel_t;

#endif
