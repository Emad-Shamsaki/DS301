#ifndef _S16001_CAN_DATA_H
#define _S16001_CAN_DATA_H

//------------------------------------------------------------------------------
//Il protocollo utilizzato e' "ispirato" al CANopen, nel senso che ne sfrutta i formati
// dei messaggi per lo scambio dati ciclico (RxPDO e TxPDO) e per quello aciclico (SDO)
// senza l'implementazione di tutti quei servizi (ad es. MNT, Emergency, 
// Node Guarding / Heartbeat, PDO mapping) e oggetti del DB CANopen (ad es. quelli
// del Communication Profile Area) che servirebbero per soddisfare completamente la 
// specifica di un nodo slave CANopen.
// In pratica e' una versione ulteriormente alleggerita di un "Predefined Connection Set":
// gestisce solo RxPDO, TxPDO e SDO e all'accensione è già nello stato OPERATIONAL (e 
// non ne esce mai).
//
// Il periodo di ciclo CAN e' di 1ms.
//
// VERSIONE v1
//
// Il master inviera' un singolo RxPDO1 con Cob-ID=0x200 "in broadcast" ai 5 assi (o per
// meglio dire ogni asse sara' configurato per accettare lo stesso Cob-ID).
// Ogni slave usera' la parte di informazioni a lui dedicata.
//
// Non e' previsto al momento l'uso del messaggio di SYNC quindi il RxPDO inviato 
// dal master fungera' anche da "sincronizzazione" per i 5 assi. 
// Alla ricezione del RxPDO ogni asse:
// - campionera' i dati e inviera' il proprio TxPDO con Cob-ID=0x180+n_asse
// - attuera' il nuovo riferimento di posizione (interpolando linearmente le posizioni
//   intermedie da inseguire durante il periodo del ciclo CAN).
//
// A causa del jitter nel ciclo di controllo del master (o anche nel caso che la 
// auto-ritrasmissione di un pacchetto dovesse occupare il bus) il RxPDO potrebbe arrivare 
// dopo che la posizione target del ciclo precedente e' gia' stata raggiunta (in pratica 
// quando un nuovo ciclo di 1 ms e' gia' cominciato sugli assi) in questo caso l'asse
// deve continuare a muoversi mantenendo la stessa velocita' del ciclo precedente 
// fintanto che arrivi il RxPDO col nuovo riferimento di posizione per il ciclo attuale.
// L'asse andra' in allarme di timeout comunicazione se non riceve il RxPDO per 
// 3 cicli consecutivi.
//
// Per non pesare sulla banda a disposizione, lasciando priorita' ai messaggi dello
// scambio stati ciclico (RxPDO e TxPDO), in ogni ciclo ci potra' essere al massimo
// 1 sola richiesta SDO fatta dal master ad uno slave.
// Lo slave rispondera' all'SDO non prima del ciclo successivo: la trasmissione della
// risposta SDO dovra' essere attivata subito dopo quella del TxPDO, in modo da accodarsi
// alla trasmissione e venire verosimilmente trasmessa subito dopo la fine dei TxPDO
// di tutti gli assi (il CAN da priorita' ai messaggi con Cob-ID più basso quindi 
// i TxPDO avranno priorita' sulla risposta SDO).
// In questo modo per ogni ciclo CAN si avranno 6 o 7 messaggi sul bus: 1 RxPDO + 5 TxPDO
// + 1 eventuale SDO (richiesta o risposta). Dovrebbe restare tempo a sufficienza per
// auto-ritrasmettere almeno 1 pacchetto nel caso ci fosse un errore.
//
// Tramite SDO il master potra' accedere in lettura/scrittura ai parametri degli assi.
// Gli unici oggetti del DB CANopen definiti saranno quelli del Manufacturer Specific 
// Profile Area (index 0x2000..0x5FFF): semplicemente i registri Modbus
// normalmente accesssibili via seriale saranno disponibili via CAN come oggetti di 
// tipo VAR a UNSIGNED16 con index pari a 0x2000+indirizzo registro modbus.
// Come caso speciale, nella richiesta SDO di lettura a 32bit i registri coinvolti 
// saranno quello esplicitamente indirizzato nella richiesta ed il successivo.
// Non sono gestite richieste SDO per valori a 8bit.
//
//
// VERSIONE v2
//
// Il master inviera' 3 RxPDO con Cob-ID=0x201 0x202 e 0x203 "in broadcast" ai 5 assi (o per
// meglio dire ogni asse sara' configurato per accettare tutti e 3 i Cob-ID).
// Ogni slave usera' la parte di informazioni a lui dedicata.
//
// Non e' previsto al momento l'uso del messaggio di SYNC quindi terzo RxPDO inviato 
// dal master fungera' anche da "sincronizzazione" per i 5 assi. 
// Alla ricezione dell'insieme dei RxPDO ogni asse:
// - campionera' i dati e inviera' il proprio TxPDO solo se e' stato selezionato 
//   per la risposta (da apposito bit nel terzo RxPDO) con Cob-ID=0x280+n_asse 
//   (N.B. si usa Cob-ID con priorita' inferiore al RxPDO)
// - attuera' il nuovo riferimento di posizione (interpolando linearmente le posizioni
//   intermedie da inseguire durante il periodo del ciclo CAN).
//
// A causa del jitter nel ciclo di controllo del master (o anche nel caso che la 
// auto-ritrasmissione di un pacchetto dovesse occupare il bus) il RxPDO potrebbe arrivare 
// dopo che la posizione target del ciclo precedente e' gia' stata raggiunta (in pratica 
// quando un nuovo ciclo di 1 ms e' gia' cominciato sugli assi) in questo caso l'asse
// deve continuare a muoversi mantenendo la stessa velocita' del ciclo precedente 
// fintanto che arrivi il RxPDO col nuovo riferimento di posizione per il ciclo attuale.
// L'asse andra' in allarme di timeout comunicazione se non riceve il RxPDO per 
// 3 cicli consecutivi.
//
// Per non pesare sulla banda a disposizione, lasciando priorita' ai messaggi dello
// scambio stati ciclico (RxPDO e TxPDO), in ogni ciclo ci potra' essere al massimo
// 1 sola richiesta SDO fatta dal master ad uno slave.
// Lo slave rispondera' all'SDO non prima del ciclo successivo: la trasmissione della
// risposta SDO dovra' essere attivata subito dopo quella del TxPDO, in modo da accodarsi
// alla trasmissione e venire verosimilmente trasmessa subito dopo la fine dei TxPDO
// di tutti gli assi (il CAN da priorita' ai messaggi con Cob-ID più basso quindi 
// i TxPDO avranno priorita' sulla risposta SDO).
// In questo modo per ogni ciclo CAN si avranno 6 o 7 messaggi sul bus: 3 RxPDO + (max) 3 TxPDO
// + 1 eventuale SDO (richiesta o risposta). Dovrebbe restare tempo a sufficienza per
// auto-ritrasmettere almeno 1 pacchetto nel caso ci fosse un errore.
//
// Tramite SDO il master potra' accedere in lettura/scrittura ai parametri degli assi.
// Gli unici oggetti del DB CANopen definiti saranno quelli del Manufacturer Specific 
// Profile Area (index 0x2000..0x5FFF): semplicemente i registri Modbus
// normalmente accesssibili via seriale saranno disponibili via CAN come oggetti di 
// tipo VAR a UNSIGNED16 con index pari a 0x2000+indirizzo registro modbus.
// Come caso speciale, nella richiesta SDO di lettura a 32bit i registri coinvolti 
// saranno quello esplicitamente indirizzato nella richiesta ed il successivo.
// Non sono gestite richieste SDO per valori a 8bit.
//
// VERSIONE v3
//
// E' un'estensione della v2 con aggiunta la gestione dei nodi 6 e 7 utilizzati per il 
// magazzino/cambio cialde (da qui l'indicazione "protocollo CIALDA").
// Viene definito un RxPDO aggiuntivo con Cob-ID=0x206/0x207 inviato in "broadcast" ai 2 nuovi assi (o
// per meglio dire ognuno dei 2 assi sara' configurato per accettare entrambi i Cob-ID).
// Ogni slave usera' la parte di informazioni a lui dedicata.
// Il RxPDO4 viene inviato nel ciclo in cui ci si aspetta la risposta dai soli assi 4 e 5 (quindi a cicli alterni, dove c'e'
// spazio per un pacchetto in piu') e otterra' in risposta un TxPDO solo dall'asse indirizzato tramite il Cob-ID (quando 
// viene inviato 0x206 rispondera' il nodo 6, al 0x207 rispondera' il nodo 7).
// Con questa schedulazione, gli assi 6/7 riceveranno un aggiornamento del comando ogni 2ms mentre lo stato dei singoli assi 6 e 7
// verra' aggiornato ogni 4ms. Questo e' ritenuto accettabile visto che si tratta di assi a bassa priorita' e con gestione "lenta".
//
// MODIFICA 10/06/2020 (v01.01.00-00)
// il primo (meno significativo) dei 4 bit originarimente riservati per funzioni future (bitFunctionsFlagsAx*) viene ora
// usato per comandare il reset della quota dell'asse (bitPositionResetAx*)
//
// MODIFICA 12/01/2021 (v01.02.00-00)
// i 3 bit residui riservati per funzioni future (bitFunctionsFlagsAx*) verranno da ora in poi
// usati come selettori per la funzione da eseguire:
// 000 - 0 nessuna funzione (retrocompatibile)
// 001 - 1 forzatura sblocco freno meccanico
//
// MODIFICA 05/03/2022 (v01.03.00-00 e FW v01.01.00-00)
// gestione "protocollo CIALDA" (protocollo CAN VERSIONE v3)

//------------------------------------------------------------------------------
//struttura per RxPDO1 v1
// Dlc = 7 (n byte di dati inviati nel pacchetto)
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		//riferimento incrementale di posizione per ogni asse: step da fare nel tempo 
		// di ciclo (equivalenti ai colpi di clock che avrebbe contato nello stesso 
		// tempo nel caso il riferimento fosse arrivato tramite i segnali di Clock/Dir,
		// con 8 bit si arriva ad un equivalente di 255kHz sul segnale di Clock).
		// Volendo lo si potrebbe anche considerare come riferimento di velocita'
		// dato che a tutti gli effetti esprime quanto spazio percorrere in 1ms.
		u8 bStepsAx1;					
		u8 bStepsAx2;
		u8 bStepsAx3;
		u8 bStepsAx4;
		u8 bStepsAx5;
		
		struct {
			//direzioni relative ai riferimenti di posizione incrementale per ogni asse
			// equivalenti al segnale di Dir
			u8 bitDirAx1 :1;			// 00
			u8 bitDirAx2 :1;			// 01
			u8 bitDirAx3 :1;			// 02
			u8 bitDirAx4 :1;			// 03
			u8 bitDirAx5 :1;			// 04
			
			u8 bitDummy05 :1;			// 05
			u8 bitAlarmReset :1;		// 06 - comando reset allarmi (in OR col comando da morsettiera)
			u8 bitEnable :1;			// 07 - comando di marcia (in AND col comando da morsettiera)

			//comandi attivazione sequenza ricerca zero per ogni asse
			u8 bitGoHomeAx1 :1;			// 08
			u8 bitGoHomeAx2 :1;			// 09
			u8 bitGoHomeAx3 :1;			// 10
			u8 bitGoHomeAx4 :1;			// 11
			u8 bitGoHomeAx5 :1;			// 12

			u8 bitDummy13 :1;			// 13
			u8 bitDummy14 :1;			// 14
			u8 bitDummy15 :1;			// 15
		};
	};
} RxPDO_t;

//------------------------------------------------------------------------------
//struttura per RxPDO1 v2
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		s32 sdwPosAx1;			//posizione assoluta asse 1
		s32 sdwPosAx2;			//posizione assoluta asse 2
	};
} RxPDO1_t;

//struttura per RxPDO2 v2
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		s32 sdwPosAx3;			//posizione assoluta asse 3
		s32 sdwPosAx4;			//posizione assoluta asse 4
	};
} RxPDO2_t;

//struttura per RxPDO3 v2
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		s32 sdwPosAx5;			//posizione assoluta asse 5
		struct {
			//abilitazioni assi
			u8 bitEnableAx1 :1;			// 00
			u8 bitEnableAx2 :1;			// 01
			u8 bitEnableAx3 :1;			// 02
			u8 bitEnableAx4 :1;			// 03
			u8 bitEnableAx5 :1;			// 04
			
			u8 bitAlarmReset :1;		// 05 - comando reset allarmi (in OR col comando da morsettiera)

			//comandi attivazione sequenza ricerca zero per ogni asse
			u8 bitGoHomeAx1 :1;			// 06
			u8 bitGoHomeAx2 :1;			// 07
			u8 bitGoHomeAx3 :1;			// 08
			u8 bitGoHomeAx4 :1;			// 09
			u8 bitGoHomeAx5 :1;			// 10

			u8 bitReplySelector :1;		// 11 - selettore gruppo assi che risponderanno con TxPDO: 0 assi 1,2 e 3 / 1 assi 4 e 5

			//flag per funzioni aggiuntive (4 per ogni asse)
			u8 bitPositionResetAx1 :1;
			u8 bitFunctionsFlagsAx1 :3;

			u8 bitPositionResetAx2 :1;
			u8 bitFunctionsFlagsAx2 :3;
			
			u8 bitPositionResetAx3 :1;			
			u8 bitFunctionsFlagsAx3 :3;
			
			u8 bitPositionResetAx4 :1;
			u8 bitFunctionsFlagsAx4 :3;
			
			u8 bitPositionResetAx5 :1;
			u8 bitFunctionsFlagsAx5 :3;
		};
	};
} RxPDO3_t;

//struttura per RxPDO4 (v3) per assi 6/7 magazzino/cambio cialda
// Dlc = 2 (n byte di dati inviati nel pacchetto)
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		struct {
			//abilitazioni assi
			u8 bPosSel :5;				// 00..04 selettore posizione da raggiungere (e mantenere in coppia fintanto che resta comandata), la posizione 0 indica motore non in coppia
			u8 bitOutFault :1;			// 05 comando uscita digitale Fault
			u8 bitOutMotion :1;			// 06 comando uscita digitale Motion
			u8 bitOutFree :1;			// 07 comando uscita digitale Free
		} Ax6;
			
		struct {
			//abilitazioni assi
			u8 bPosSel :5;				// 00..04 selettore posizione da raggiungere (e mantenere in coppia fintanto che resta comandata), la posizione 0 indica motore non in coppia
			u8 bitOutFault :1;			// 05 comando uscita digitale Fault
			u8 bitOutMotion :1;			// 06 comando uscita digitale Motion
			u8 bitOutFree :1;			// 07 comando uscita digitale Free
		} Ax7;
	};
} RxPDO4_t;


//------------------------------------------------------------------------------
//struttura per TxPDO1 v1 e v2
// Dlc = 7 (n byte di dati inviati nel pacchetto)
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		s32 sdwActualPosition;			//posizione assoluta dell'asse nella stessa unita'
										// di misura del riferimento incrementale (1 count
										// corrisponde ad uno step/clock)
		
		u8 bOutCurrent;					//modulo della corrente motore [0.1A]
		
		//flag vari
		struct {
			u8 bitEnabled :1;			// 00 - stato asse abilitato (motore in coppia)
			u8 bitFault :1;				// 01 - stato presenza allarme
			u8 bitEmergency :1;			// 02 - stato emergenza (0 alimentazione potenza sezionata / 1 alimentazione potenza presente)
			
			u8 bitHome :1;				// 03 - stato ingresso di Home
			u8 bitLimCW :1;				// 04 - stato ingresso finecorsa CW
			u8 bitLimCCW :1;			// 05 - stato ingresso finecorsa CCW
			
			u8 bitHoming :1;			// 06 - stato ricerca di zero in corso
			u8 bitHomed :1;				// 07 - stato posizione di Home raggiunta (set su Home, reset su tacca zero encoder)
			
			u8 bitBrake :1;				// 08 - stato uscita freno motore
			u8 bitOutDig :1;			// 09 - stato uscita digitale configurabile
			
			u8 bitDummy10 :1;			// 10
			u8 bitDummy11 :1;			// 11
			u8 bitDummy12 :1;			// 12
			u8 bitDummy13 :1;			// 13
			u8 bitDummy14 :1;			// 14
			u8 bitDummy15 :1;			// 15			
		};
	};
} TxPDO_t;

//------------------------------------------------------------------------------
//struttura per TxPDO4 (v3) per assi 6/7 magazzino/cambio cialda
// Dlc = 5 (n byte di dati inviati nel pacchetto)
typedef union {
	u32 dwords[2];
	u16 words[4];
	u8 bytes[8];
	
	struct {
		s32 sdwActualPosition;			//posizione assoluta dell'asse nella stessa unita'
										// di misura del riferimento incrementale (1 count
										// corrisponde ad uno step/clock)
		
		//flag vari
		struct {
			u8 bitHome :1;				// 00 - stato ingresso di Home
			u8 bitLimCW :1;				// 01 - stato ingresso finecorsa CW
			u8 bitLimCCW :1;			// 02 - stato ingresso finecorsa CCW
			u8 bitHoming :1;			// 03 - stato ingresso ricerca di zero
			
			u8 bitPosReached :1;		// 04 - stato posizione raggiunta (valevole anche come segnalazione di homed durante homing)
			u8 bitFault :1;				// 05 - stato presenza allarme
			u8 bitDummy6 :1;			// 06
			u8 bitDummy7 :1;			// 07			
		};
	};
} TxPDO4_t;

#endif