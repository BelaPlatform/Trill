;@Id: boot.tpl#903 @
;=============================================================================
;  FILENAME:   boot.asm
;  Version:    4.30
;
;  DESCRIPTION:
;  M8C Boot Code for CY8C20x66 microcontroller devices.
;
;  Copyright (c) Cypress Semiconductor 2012. All Rights Reserved.
;
; NOTES:
; PSoC Designer's Device Editor uses a template file, BOOT.TPL, located in
; the project's root directory to create BOOT.ASM. Any changes made to
; BOOT.ASM will be  overwritten every time the project is generated; therfore
; changes should be made to BOOT.TPL not BOOT.ASM. Care must be taken when
; modifying BOOT.TPL so that replacement strings (such as @PROJECT_NAME)
; are not accidentally modified.
;
;=============================================================================

include ".\lib\GlobalParams.inc"
include "m8c.inc"
include "m8ssc.inc"
include "memory.inc"

;--------------------------------------
; Export Declarations
;--------------------------------------

export __Start
IF	(TOOLCHAIN & HITECH)
ELSE
export __bss_start
export __data_start
export __idata_start
export __func_lit_start
export __text_start
ENDIF
export  _bGetPowerSetting
export   bGetPowerSetting


;--------------------------------------
; Optimization flags
;--------------------------------------
;
; To change the value of these flags, modify the file boot.tpl, not
; boot.asm. See the notes in the banner comment at the beginning of
; this file.

; Optimization for Assembly language (only) projects and C-language projects
; that do not depend on the C compiler to initialize the values of RAM variables.
;   Set to 1: Support for C Run-time Environment initialization
;   Set to 0: Support for C not included. Faster start up, smaller code space.
;
IF	(TOOLCHAIN & HITECH)
; The C compiler will customize the startup code - it's not required here

C_LANGUAGE_SUPPORT:              equ 0
ELSE
C_LANGUAGE_SUPPORT:              equ 1
ENDIF


; The following equate is required for proper operation. Reseting its value
; is discouraged.  WAIT_FOR_32K is effective only if the crystal oscillator is
; selected.  If the designer chooses to not wait then stabilization of the ECO
; and PLL_Lock must take place within user code. See the family data sheet for
; the requirements of starting the ECO and PLL lock mode.
;
;   Set to 1: Wait for XTAL (& PLL if selected) to stabilize before
;                invoking main
;   Set to 0: Boot code does not wait; clock may not have stabilized by
;               the time code in main starts executing.
;
WAIT_FOR_32K:                    equ 1


; For historical reasons, by default the boot code uses an lcall instruction
; to invoke the user's _main code. If _main executes a return instruction,
; boot provides an infinite loop. By changing the following equate from zero
; to 1, boot's lcall will be replaced by a ljmp instruction, saving two
; bytes on the stack which are otherwise required for the return address. If
; this option is enabled, _main must not return. (Beginning with the 4.2
; release, the C compiler automatically places an infinite loop at the end
; of main, rather than a return instruction.)
;
ENABLE_LJMP_TO_MAIN:             equ 0


;-----------------------------------------------------------------------------
; Interrupt Vector Table
;-----------------------------------------------------------------------------
;
; Interrupt vector table entries are 4 bytes long.  Each one contains
; a jump instruction to an ISR (Interrupt Service Routine), although
; very short ISRs could be encoded within the table itself. Normally,
; vector jump targets are modified automatically according to the user
; modules selected. This occurs when the 'Generate Application' opera-
; tion is run causing PSoC Designer to create boot.asm and the other
; configuration files. If you need to hard code a vector, update the
; file boot.tpl, not boot.asm. See the banner comment at the beginning
; of this file.
;-----------------------------------------------------------------------------

    AREA TOP (ROM, ABS, CON)

    org   0                        ;Reset Interrupt Vector
IF	(TOOLCHAIN & HITECH)
   ;ljmp   __Start                  ;C compiler fills in this vector
ELSE
   nop
   ljmp   __Start                  ;C compiler fills in this vector
ENDIF
    ;@PSoC_BOOT_ISR_UserCode_START@
    ;---------------------------------------------------
    ; Insert your custom code below this banner
    ;---------------------------------------------------

    org   04h                      ;Low Voltage Detect (LVD) Interrupt Vector
    halt                           ;Stop execution if power falls too low

    org   08h                      ;Analog Interrupt Vector
    `@INTERRUPT_2`
    reti

    org   0Ch                      ;CapSense Interrupt Vector
    `@INTERRUPT_3`
    reti

    org   10h                      ;Timer Interrupt Vector
    `@INTERRUPT_4`
    reti

    org   14h                      ;GPIO Interrupt Vector
    `@INTERRUPT_5`
    reti

    org   18h                      ;PSoC SPI Interrupt Vector
    `@INTERRUPT_6`
    reti

    org   1Ch                      ;PSoC I2C Interrupt Vector
    `@INTERRUPT_7`
    reti

    org   20h                      ;Sleep Timer Interrupt Vector
    `@INTERRUPT_8`
    reti

    org   24h                      ;Timer1 Interrupt Vector
    `@INTERRUPT_9`
    reti

    org   28h                      ;Timer2 Interrupt Vector
    `@INTERRUPT_10`
    reti

    org   2ch                      ;USB_Bus_Reset
    `@INTERRUPT_11`
    reti

    org   30h                      ;USB_SOF
    `@INTERRUPT_12`
    reti

    org   34h                      ;USB_Endpoint0
    `@INTERRUPT_13`
    reti

    org   38h                      ;USB_Endpoint1
    `@INTERRUPT_14`
    reti

    org   3ch                      ;USB_Endpoint2
    `@INTERRUPT_15`
    reti

    org   40h                      ;USB_Endpoint3
    `@INTERRUPT_16`
    reti

    org   44h                      ;USB_Endpoint4
    `@INTERRUPT_17`
    reti

    org   48h                      ;USB_Endpoint5
    `@INTERRUPT_18`
    reti

    org   4ch                      ;USB_Endpoint6
    `@INTERRUPT_19`
    reti

    org   50h                      ;USB_Endpoint7
    `@INTERRUPT_20`
    reti

    org   54h                      ;USB_Endpoint8
    `@INTERRUPT_21`
    reti

    org   58h                      ;USB_Wakeup
    `@INTERRUPT_22`
    reti

    org   5ch                      ;SPC (System Performance Controller) interrupt Vector
    `@INTERRUPT_23`
    reti
    ;---------------------------------------------------
    ; Insert your custom code above this banner
    ;---------------------------------------------------
    ;@PSoC_BOOT_ISR_UserCode_END@

;-----------------------------------------------------------------------------
;  Start of Execution.
;-----------------------------------------------------------------------------
;  The Supervisory ROM SWBootReset function has already completed the
;  calibrate1 process, loading trim values for 5 volt operation.
;

IF	(TOOLCHAIN & HITECH)
 	AREA PD_startup(CODE, REL, CON)
ELSE
    org 68h
ENDIF
__Start:

    nop
    ; Adjust LVD and Trip Voltage supply monitor
    M8C_SetBank1
    mov   reg[VLT_CR], LVD_TBEN_JUST | TRIP_VOLTAGE_JUST
    M8C_SetBank0

M8C_ClearWDTAndSleep 		   ; Clear WDT before enabling it
IF ( WATCHDOG_ENABLE )             ; WDT selected in Global Params
    M8C_EnableWatchDog
ENDIF

    ;---------------------------
    ; Set up the Temporary stack
    ;---------------------------
    ; A temporary stack is set up for the SSC instructions.
    ; The real stack start will be assigned later.
    ;
_stack_start:          equ 80h
    mov   A, _stack_start          ; Set top of stack to end of used RAM
    swap  SP, A                    ; This is only temporary if going to LMM

    ;------------------------
    ; Set IMO-related Trim 
    ;------------------------
M8C_ClearWDTAndSleep 		   ; Clear WDT before enabling it
IF (IMO_SETTING)
 IF ( IMO_SETTING & IMO_SET_6MHZ )      ; *** 6MHZ Main Oscillator ***
    M8SSC_SetTableIMOTrim 1, SSCTBL1_TRIM_IMO_6MHZ
ENDIF ; 6MHz Operation

;; IF ( IMO_SETTING & IMO_SET_12MHZ )      ; *** 12MHZ Main Oscillator ***
;;    M8SSC_SetTableIMOTrim 1, SSCTBL1_TRIM_IMO_12MHZ
;;ENDIF ; 12MHz Operation

IF ( IMO_SETTING & IMO_SET_24MHZ )      ; *** 2.7 Volts / 12MHZ operation ***
IF (USB_USED & 1)							   ; USB deployed
    M8SSC_SetTableIMOTrim  1, SSCTBL1_TRIM_IMO_24MHZ_USB
ELSE
    M8SSC_SetTableIMOTrim  1, SSCTBL1_TRIM_IMO_24MHZ_NOUSB
ENDIF
ENDIF ; 24MHZ operation ***
ELSE ;(IMO_SETTING == 0 => 12Mhz)  *** 12MHZ Main Oscillator ***
    M8SSC_SetTableIMOTrim 1, SSCTBL1_TRIM_IMO_12MHZ
ENDIF ; 12MHz settings

    mov  [bSSC_KEY1],  0           ; Lock out Flash and Supervisiory operations
    mov  [bSSC_KEYSP], 0

    ;---------------------------------------
    ; Setup CPU Clock for SysClk/2 Frequency
    ;---------------------------------------

    M8C_SetBank1
    mov   reg[OSC_CR0], (SLEEP_TIMER_JUST | OSC_CR0_CPU_IMO24_12MHz); 12Mhz @ IMO = 24Mhz; 6Mhz @ IMO = 12Mhz; 3Mhz @ IMO = 6Mhz
    M8C_SetBank0
    M8C_ClearWDTAndSleep           ; Reset the watch dog
    
    ;-------------------------------------------------------
    ; Initialize External Crystal Oscillator (ECO)
    ;-------------------------------------------------------
	M8C_SetBank1
IF ( SELECT_32K )
    ; If the user has requested the External Crystal Oscillator (ECO) then setup
	; the crystal pin drive modes, then turn the ECO on	and wait for it 
	; to stabilize and the system to switch over to it. 
    ; Set the SleepTimer period to 1 sec to time the wait for
    ; the ECO to stabilize.
	
	; Initialize Proper Drive Mode for ECO pins (P2[3] and P2[5])
	M8C_SetBank0
	or reg[PRT2DR], 0x28      		; Set bits 3 and 5 of ECO pin DR register
    M8C_SetBank1   
    or reg[PRT2DM0],  0x28        	; Set bits 3 and 5 of ECO pin DM0 register
	; ECO pins are now in proper drive mode to interface with the crystal
  
    or    reg[ECO_CFG],  ECO_ECO_EX  ; ECO exists
	mov   reg[OSC_CR0], (OSC_CR0_SLEEP_1HZ | OSC_CR0_CPU_IMO24_12MHz)

	or    reg[ECO_ENBUS], ECO_ENBUS_DIS_MASK		; Set ECO_ENBUS bits to default state (0b111)
	and   reg[ECO_ENBUS], ECO_ENBUS_EN_MASK			; Set ECO_ENBUS bits to value of 0b011
    
	or   reg[OSC_CR0], SELECT_32K_JUST				; Enable the ECO
	
	M8C_SetBank0
    M8C_ClearWDTAndSleep                  ; Reset the sleep timer to get a full second
    or    reg[INT_MSK0], INT_MSK0_SLEEP   ; Enable latching of SleepTimer interrupt
    mov   reg[INT_VC],   0                ; Clear all pending interrupts
   
IF (WAIT_FOR_32K) 	; Only wait for one second if WAIT_FOR_32K is set to 1 above
.WaitFor1s:
    tst   reg[INT_CLR0], INT_MSK0_SLEEP   ; Test the SleepTimer Interrupt Status Interrupt will latch but
    jz   .WaitFor1s                       ;  will not dispatch since interrupts are not globally enabled
ENDIF ; Wait for crystal stabilization
	
ELSE
    and  reg[ECO_CFG], ~ECO_ECO_EX  ; Prevent ECO from being enabled
ENDIF
	M8C_SetBank0
    M8C_ClearWDTAndSleep           ; Reset the watch dog

    ;------------------------------------------------------- 
    ; Initialize Proper Drive Mode for External Clock Pin
    ;-------------------------------------------------------
 
    ; Change EXTCLK pin from Hi-Z Analog (DM:10b, DR:0) drive mode to Hi-Z (DM:11b, DR:1) drive mode
IF (SYSCLK_SOURCE)
    or reg[PRT1DR], 0x10      ; Set bit 4 of EXTCLK pin's DR register
    M8C_SetBank1   
    or reg[PRT1DM0],  0x10        ; Set bit 4 of EXTCLK pin's DM0 register
    M8C_SetBank0
ENDIF
	; EXTCLK pin is now in proper drive mode to input the external clock signal


IF	(TOOLCHAIN & HITECH)
    ;---------------------------------------------
    ; HI-TECH initialization: Enter the Large Memory Model, if applicable
    ;---------------------------------------------
	global		__Lstackps
	mov     a,low __Lstackps
	swap    a,sp

IF ( SYSTEM_LARGE_MEMORY_MODEL )
    RAM_SETPAGE_STK SYSTEM_STACK_PAGE      ; relocate stack page ...
    RAM_SETPAGE_IDX2STK            ; initialize other page pointers
    RAM_SETPAGE_CUR 0
    RAM_SETPAGE_MVW 0
    RAM_SETPAGE_MVR 0
    IF ( SYSTEM_IDXPG_TRACKS_STK_PP ); Now enable paging:
    or    F, FLAG_PGMODE_11b       ; LMM w/ IndexPage<==>StackPage
    ELSE
    or    F, FLAG_PGMODE_10b       ; LMM w/ independent IndexPage
    ENDIF ;  SYSTEM_IDXPG_TRACKS_STK_PP
ENDIF ;  SYSTEM_LARGE_MEMORY_MODEL
ELSE
    ;---------------------------------------------
    ; ImageCraft Enter the Large Memory Model, if applicable
    ;---------------------------------------------
IF ( SYSTEM_LARGE_MEMORY_MODEL )
    RAM_SETPAGE_STK SYSTEM_STACK_PAGE      ; relocate stack page ...
    mov   A, SYSTEM_STACK_BASE_ADDR        ;   and offset, if any
    swap  A, SP
    RAM_SETPAGE_IDX2STK            ; initialize other page pointers
    RAM_SETPAGE_CUR 0
    RAM_SETPAGE_MVW 0
    RAM_SETPAGE_MVR 0

  IF ( SYSTEM_IDXPG_TRACKS_STK_PP ); Now enable paging:
    or    F, FLAG_PGMODE_11b       ; LMM w/ IndexPage<==>StackPage
  ELSE
    or    F, FLAG_PGMODE_10b       ; LMM w/ independent IndexPage
  ENDIF ;  SYSTEM_IDXPG_TRACKS_STK_PP
ELSE
    mov   A, __ramareas_end        ; Set top of stack to end of used RAM
    swap  SP, A
ENDIF ;  SYSTEM_LARGE_MEMORY_MODEL
ENDIF ;	TOOLCHAIN

    ;@PSoC_BOOT_LOADCFG_UserCode_START@
    ;---------------------------------------------------
    ; Insert your custom code below this banner
    ;---------------------------------------------------

    ;---------------------------------------------------
    ; Insert your custom code above this banner
    ;---------------------------------------------------
    ;@PSoC_BOOT_LOADCFG_UserCode_END@ 

    ;-------------------------
    ; Load Base Configuration
    ;-------------------------
    ; Load global parameter settings and load the user modules in the
    ; base configuration. Exceptions: (1) Leave CPU Speed fast as possible
    ; to minimize start up time; (2) We may still need to play with the
    ; Sleep Timer.
    ;
    lcall LoadConfigInit

    ;-----------------------------------
    ; Initialize C Run-Time Environment
    ;-----------------------------------
IF ( C_LANGUAGE_SUPPORT )
IF ( SYSTEM_SMALL_MEMORY_MODEL )
    mov  A,0                           ; clear the 'bss' segment to zero
    mov  [__r0],<__bss_start
BssLoop:
    cmp  [__r0],<__bss_end
    jz   BssDone
    mvi  [__r0],A
    jmp  BssLoop
BssDone:
    mov  A,>__idata_start              ; copy idata to data segment
    mov  X,<__idata_start
    mov  [__r0],<__data_start
IDataLoop:
    cmp  [__r0],<__data_end
    jz   C_RTE_Done
    push A
    romx
    mvi  [__r0],A
    pop  A
    inc  X
    adc  A,0
    jmp  IDataLoop

ENDIF ; SYSTEM_SMALL_MEMORY_MODEL

IF ( SYSTEM_LARGE_MEMORY_MODEL )
    mov   reg[CUR_PP], >__r0           ; force direct addr mode instructions
                                       ; to use the Virtual Register page.

    ; Dereference the constant (flash) pointer pXIData to access the start
    ; of the extended idata area, "xidata." Xidata follows the end of the
    ; text segment and may have been relocated by the Code Compressor.
    ;
    mov   A, >__pXIData                ; Get the address of the flash
    mov   X, <__pXIData                ;   pointer to the xidata area.
    push  A
    romx                               ; get the MSB of xidata's address
    mov   [__r0], A
    pop   A
    inc   X
    adc   A, 0
    romx                               ; get the LSB of xidata's address
    swap  A, X
    mov   A, [__r0]                    ; pXIData (in [A,X]) points to the
                                       ;   XIData structure list in flash
    jmp   .AccessStruct

    ; Unpack one element in the xidata "structure list" that specifies the
    ; values of C variables. Each structure contains 3 member elements.
    ; The first is a pointer to a contiguous block of RAM to be initial-
    ; ized. Blocks are always 255 bytes or less in length and never cross
    ; RAM page boundaries. The list terminates when the MSB of the pointer
    ; contains 0xFF. There are two formats for the struct depending on the
    ; value in the second member element, an unsigned byte:
    ; (1) If the value of the second element is non-zero, it represents
    ; the 'size' of the block of RAM to be initialized. In this case, the
    ; third member of the struct is an array of bytes of length 'size' and
    ; the bytes are copied to the block of RAM.
    ; (2) If the value of the second element is zero, the block of RAM is
    ; to be cleared to zero. In this case, the third member of the struct
    ; is an unsigned byte containing the number of bytes to clear.

.AccessNextStructLoop:
    inc   X                            ; pXIData++
    adc   A, 0
.AccessStruct:                         ; Entry point for first block
    ;
    ; Assert: pXIData in [A,X] points to the beginning of an XIData struct.
    ;
    M8C_ClearWDT                       ; Clear the watchdog for long inits
    push  A
    romx                               ; MSB of RAM addr (CPU.A <- *pXIData)
    mov   reg[MVW_PP], A               ;   for use with MVI write operations
    inc   A                            ; End of Struct List? (MSB==0xFF?)
    jz    .C_RTE_WrapUp                ;   Yes, C runtime environment complete
    pop   A                            ; restore pXIData to [A,X]
    inc   X                            ; pXIData++
    adc   A, 0
    push  A
    romx                               ; LSB of RAM addr (CPU.A <- *pXIData)
    mov   [__r0], A                    ; RAM Addr now in [reg[MVW_PP],[__r0]]
    pop   A                            ; restore pXIData to [A,X]
    inc   X                            ; pXIData++ (point to size)
    adc   A, 0
    push  A
    romx                               ; Get the size (CPU.A <- *pXIData)
    jz    .ClearRAMBlockToZero         ; If Size==0, then go clear RAM
    mov   [__r1], A                    ;             else downcount in __r1
    pop   A                            ; restore pXIData to [A,X]

.CopyNextByteLoop:
    ; For each byte in the structure's array member, copy from flash to RAM.
    ; Assert: pXIData in [A,X] points to previous byte of flash source;
    ;         [reg[MVW_PP],[__r0]] points to next RAM destination;
    ;         __r1 holds a non-zero count of the number of bytes remaining.
    ;
    inc   X                            ; pXIData++ (point to next data byte)
    adc   A, 0
    push  A
    romx                               ; Get the data value (CPU.A <- *pXIData)
    mvi   [__r0], A                    ; Transfer the data to RAM
    tst   [__r0], 0xff                 ; Check for page crossing
    jnz   .CopyLoopTail                ;   No crossing, keep going
    mov   A, reg[ MVW_PP]              ;   If crossing, bump MVW page reg
    inc   A
    mov   reg[ MVW_PP], A
.CopyLoopTail:
    pop   A                            ; restore pXIData to [A,X]
    dec   [__r1]                       ; End of this array in flash?
    jnz   .CopyNextByteLoop            ;   No,  more bytes to copy
    jmp   .AccessNextStructLoop        ;   Yes, initialize another RAM block

.ClearRAMBlockToZero:
    pop   A                            ; restore pXIData to [A,X]
    inc   X                            ; pXIData++ (point to next data byte)
    adc   A, 0
    push  A
    romx                               ; Get the run length (CPU.A <- *pXIData)
    mov   [__r1], A                    ; Initialize downcounter
    mov   A, 0                         ; Initialize source data

.ClearRAMBlockLoop:
    ; Assert: [reg[MVW_PP],[__r0]] points to next RAM destination and
    ;         __r1 holds a non-zero count of the number of bytes remaining.
    ;
    mvi   [__r0], A                    ; Clear a byte
    tst   [__r0], 0xff                 ; Check for page crossing
    jnz   .ClearLoopTail               ;   No crossing, keep going
    mov   A, reg[ MVW_PP]              ;   If crossing, bump MVW page reg
    inc   A
    mov   reg[ MVW_PP], A
    mov   A, 0                         ; Restore the zero used for clearing
.ClearLoopTail:
    dec   [__r1]                       ; Was this the last byte?
    jnz   .ClearRAMBlockLoop           ;   No,  continue
    pop   A                            ;   Yes, restore pXIData to [A,X] and
    jmp   .AccessNextStructLoop        ;        initialize another RAM block

.C_RTE_WrapUp:
    pop   A                            ; balance stack

ENDIF ; SYSTEM_LARGE_MEMORY_MODEL

C_RTE_Done:

ENDIF ; C_LANGUAGE_SUPPORT

    ;-------------------------------
    ; Set Power-On Reset (POR) Level
    ;-------------------------------
    ;  The writes to the VLT_CR register below include setting the POR to VLT_CR_POR_HIGH, 
    ;  VLT_CR_POR_MID or VLT_CR_POR_LOW. Correctly setting this value is critical to the proper 
    ;  operation of the PSoC. The POR protects the M8C from mis-executing when Vdd falls low. 
    ;  These values should not be changed from the settings here. See Section "POR and LVD" of 
    ;  Technical Reference Manual #001-22219 for more information.

	M8C_SetBank1
	or reg[VLT_CR], POR_LEVEL_JUST
    M8C_SetBank0

    ;----------------------------
    ; Wrap up and invoke "main"
    ;----------------------------

    ; Disable the Sleep interrupt that was used for timing above.  In fact,
    ; no interrupts should be enabled now, so may as well clear the register.
    ;
    mov  reg[INT_MSK0],0

    ; Everything has started OK. Now select requested CPU & sleep frequency.
    ;
    and reg[CPU_SCR1], ~CPU_SCR1_SLIMO_MASK			  ; clear slimo bits
	or  reg[CPU_SCR1], 	(IMO_SETTING << 3)
    
    M8C_SetBank1
	IF (ILO_SETTING)
		or reg[ILO_TR], ILO_SETTING_JUST ; appending ILO Setting to ILO_TR
	ENDIF
    
    mov  reg[OSC_CR0],(SELECT_32K_JUST | SLEEP_TIMER_JUST | CPU_CLOCK_JUST)
    M8C_SetBank0

    ; Global Interrupt are NOT enabled, this should be done in main().
    ; LVD is set but will not occur unless Global Interrupts are enabled.
    ; Global Interrupts should be enabled as soon as possible in main().
    ;
    mov  reg[INT_VC],0             ; Clear any pending interrupts which may
                                   ; have been set during the boot process.
IF	(TOOLCHAIN & HITECH)
	ljmp  startup                  ; Jump to C compiler startup code
ELSE
IF ENABLE_LJMP_TO_MAIN
    ljmp  _main                    ; goto main (no return)
ELSE
    lcall _main                    ; call main
.Exit:
    jmp  .Exit                     ; Wait here after return till power-off or reset
ENDIF
ENDIF ; TOOLCHAIN

    ;---------------------------------
    ; Library Access to Global Parms
    ;---------------------------------
    ;
 bGetPowerSetting:
_bGetPowerSetting:
    ; Returns value of POWER_SETTING in the A register.
    ; No inputs. No Side Effects.
    ;
    ret

IF	(TOOLCHAIN & HITECH)
ELSE
    ;---------------------------------
    ; Order Critical RAM & ROM AREAs
    ;---------------------------------
    ;  'TOP' is all that has been defined so far...

    ;  ROM AREAs for C CONST, static & global items
    ;
    AREA lit               (ROM, REL, CON, LIT)   ; 'const' definitions
    AREA idata             (ROM, REL, CON, LIT)   ; Constants for initializing RAM
__idata_start:

    AREA func_lit          (ROM, REL, CON, proclab)   ; Function Pointers
__func_lit_start:

IF ( SYSTEM_LARGE_MEMORY_MODEL )
    ; We use the func_lit area to store a pointer to extended initialized
    ; data (xidata) area that follows the text area. Func_lit isn't
    ; relocated by the code compressor, but the text area may shrink and
    ; that moves xidata around.
    ;
__pXIData:         word __text_end           ; ptr to extended idata
ENDIF

    AREA psoc_config       (ROM, REL, CON)   ; Configuration Load & Unload
    AREA UserModules       (ROM, REL, CON)   ; User Module APIs

    ; CODE segment for general use
    ;
    AREA text (ROM, REL, CON)
__text_start:

    ; RAM area usage
    ;
    AREA data              (RAM, REL, CON)   ; initialized RAM
__data_start:

    AREA virtual_registers (RAM, REL, CON)   ; Temp vars of C compiler
    AREA InterruptRAM      (RAM, REL, CON)   ; Interrupts, on Page 0
    AREA bss               (RAM, REL, CON)   ; general use
__bss_start:

ENDIF ; TOOLCHAIN

; end of file boot.asm
