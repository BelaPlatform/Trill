;;*****************************************************************************
;;*****************************************************************************
;;
;;       FILENAME: Memory.inc
;;
;;    DESCRIPTION: Memory Model and Stack Parameter Definitions for 
;;                 CY8C20x66/CY8C20x65 PSoC devices.
;;
;;
;;-----------------------------------------------------------------------------
;;  Copyright (c) Cypress Semiconductor 2015. All Rights Reserved.
;;*****************************************************************************
;;*****************************************************************************
;
;
;  ******** Define Memory Model and Stack parameters ********
;
SYSTEM_STACK_PAGE: equ 3   
SYSTEM_STACK_BASE_ADDR: equ 0h   
SYSTEM_LARGE_MEMORY_MODEL: equ 1   
SYSTEM_SMALL_MEMORY_MODEL: equ 0   
IMAGECRAFT: equ 1   
HITECH: equ 2   
TOOLCHAIN: equ IMAGECRAFT   
SYSTEM_TOOLS: equ 1   
SYSTEM_IDXPG_TRACKS_STK_PP: equ 1   
SYSTEM_IDXPG_TRACKS_IDX_PP: equ 0   
SYSTEM_MULTIPAGE_STACK: equ 0 


;  ******* Function Class Definitions *******
;
;  These definitions are used to describe RAM access patterns. They provide
;  documentation and they control prologue and epilogue macros that perform
;  the necessary housekeeping functions for large memory model devices like
;  the CY8C20x66/CY8C20x65.

RAM_USE_CLASS_1:               equ 1   ; PUSH, POP & I/O access
RAM_USE_CLASS_2:               equ 2   ; Indexed address mode on stack page
RAM_USE_CLASS_3:               equ 4   ; Indexed address mode to any page
RAM_USE_CLASS_4:               equ 8   ; Direct/Indirect address mode access

; ******* Hi Tech Specific *******
; Turn on the expansion of all macros by default
IF (TOOLCHAIN & HITECH)
OPT EXPAND
ENDIF
;  ******* Page Pointer Manipulation Macros *******
;
;  Most of the following macros are conditionally compiled so they only
;  produce code if the large memory model is selected.

   ;-----------------------------------------------
   ;  Set Stack Page Macro
   ;-----------------------------------------------
   ;
   ;  DESC: Modify STK_PP in the large or small memory Models.
   ;
   ; INPUT: Constant (e.g., SYSTEM_STACK_PAGE) that specifies the RAM page on
   ;        which stack operations like PUSH and POP store and retrieve their
   ;        data
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_STK( PG_NUMBER )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      mov reg[STK_PP], @PG_NUMBER
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Set Current Page Macro
   ;-----------------------------------------------
   ;
   ;  DESC: Modify CUR_PP in the large or small memory Models.
   ;
   ; INPUT: Constant value (e.g., >bFoo) for the RAM page number used in
   ;        calculation of effective direct-mode address operands.
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_CUR( PG_NUMBER )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      mov reg[CUR_PP], @PG_NUMBER
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Set Index Page Macro
   ;-----------------------------------------------
   ;
   ;  DESC: Modify IDX_PP in the large or small emory Models.
   ;
   ; INPUT: Constant value (e.g., >caFoo) for the RAM page number used in
   ;         calculation of effective index-mode address operands.
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_IDX( PG_NUMBER )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      mov reg[IDX_PP], @PG_NUMBER
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Set MVI Read Page Macro
   ;-----------------------------------------------
   ;
   ;  DESC: Modify MVR_PP in the large or small memory Models.
   ;
   ; INPUT: Constant value (e.g., >pFoo) for the RAM page number used in
   ;        calculation of indirect address operands used in the
   ;        "mvi A, [pFoo]" instructions.
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_MVR( PG_NUMBER )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      mov reg[MVR_PP], @PG_NUMBER
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Set MVI Write Page Macro
   ;-----------------------------------------------
   ;
   ;  DESC: Modify MVW_PP in the large or small memory Models.
   ;
   ; INPUT: Constant value (e.g., >pFoo) for the RAM page number used in
   ;        calculation of indirect address operands used in the
   ;        "mvi [pFoo], A" instructions.
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_MVW( PG_NUMBER )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      mov reg[MVW_PP], @PG_NUMBER
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Force Index Page Pointer to Stack Page
   ;-----------------------------------------------
   ;
   ;  DESC: Map index-mode operands onto the stack page by modifying IDX_PP.
   ;        See also RAM_LOCK_INDEX_TO_STACKPAGE.
   ;
   ; INPUT: None
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_SETPAGE_IDX2STK
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      IF ( SYSTEM_MULTIPAGE_STACK )
         mov   A, reg[STK_PP]
         mov   reg[IDX_PP], A
      ELSE
         RAM_SETPAGE_IDX SYSTEM_STACK_PAGE
      ENDIF
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Change Memory Mode
   ;-----------------------------------------------
   ;
   ;  DESC: Modify FLAG_PAGEMODE bits in the large and small memory Models.
   ;
   ; INPUT: Constant value for PGMODE bitfield of CPU Flag register, F.
   ;        See FLAG_PGMODE_{x} constants in M8C.INC.
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_CHANGE_PAGE_MODE( MODE )
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      and   F, ~FLAG_PGMODE_MASK        ; NOTE: transition thru 00b state
      or    F,  FLAG_PGMODE_MASK & @MODE
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Set Large Memory Model Native Paging Mode
   ;-----------------------------------------------
   ;
   ;  DESC: Changes the FLAG_PAGEMODE bits to enter the native LMM RAM
   ;        paging mode *IFF* a simple "OR" is guaranteed to work---for
   ;        example, in an ISR, when the PGMODE bits have been cleared
   ;        to zero. If a simple "OR" is not guaranteed to work, use
   ;        the slower RAM_RESTORE_NATIVE_PAGING instead.
   ;
   ; INPUT: none
   ;
   ;  COST: 4 instruction cycles (in LMM only)

   macro RAM_SET_NATIVE_PAGING
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
   IF ( SYSTEM_IDXPG_TRACKS_STK_PP )
      or    F,  FLAG_PGMODE_11b            ; LMM w/ IndexPage<==>StackPage
   ENDIF ;  PGMODE LOCKED
   IF ( SYSTEM_IDXPG_TRACKS_IDX_PP )
      or    F,  FLAG_PGMODE_10b            ; LMM with independent IndexPage
   ENDIF ; PGMODE FREE
   ENDIF ; SYSTEM_LARGE_MEMORY_MODEL
   endm

   ;-----------------------------------------------
   ; Restore Large Memory Model Native Paging Mode
   ;-----------------------------------------------
   ;
   ;  DESC: Changes the FLAG_PAGEMODE bits to enter the native LMM RAM
   ;        paging mode. Always works because it clears the PGMODE bits
   ;        before OR-ing in the new ones. See RAM_RESTORE_NATIVE_PAGING
   ;        for a faster method.
   ;
   ; INPUT: none
   ;
   ;  COST: 8 instruction cycles (in LMM only)

   macro RAM_RESTORE_NATIVE_PAGING
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
   IF ( SYSTEM_IDXPG_TRACKS_STK_PP )
      RAM_CHANGE_PAGE_MODE FLAG_PGMODE_11b ; LMM w/ IndexPage<==>StackPage
   ENDIF ;  PGMODE LOCKED
   IF ( SYSTEM_IDXPG_TRACKS_IDX_PP )
      RAM_CHANGE_PAGE_MODE FLAG_PGMODE_10b ; LMM with independent IndexPage
   ENDIF ; PGMODE FREE
   ENDIF ; SYSTEM_LARGE_MEMORY_MODEL
   endm

   ;-----------------------------------------------
   ; Force indexed addr mode operands to Stack Pg
   ;-----------------------------------------------
   ;
   ;  DESC: Force instructions that use indexed address mode to reference the
   ;        stack page (as defined by STK_PP). This macro sets the "Indexed
   ;        Stack Mode" bit (LSB) of the PGMODE bit field in the CPU Flag
   ;        register, F. (See also RAM_SETPAGE_IDX2STK, above.)
   ;
   ; INPUT: none
   ;
   ;  COST: 4 instruction cycles (in LMM only)

   macro RAM_X_POINTS_TO_STACKPAGE
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      or   F, FLAG_PGMODE_01b
   ENDIF ;  SYSTEM_LARGE_MEMORY_MODEL
   endm

   ;-----------------------------------------------
   ;  Force indexed addr mode operands to Index Pg
   ;-----------------------------------------------
   ;
   ;  DESC: Permit instructions that use indexed address mode to reference page
   ;        zero or the page pointed to by the IDX_PP register, depending on the
   ;        setting of the MSb (or "Direct Page Mode" bit) of the PGMODE bits
   ;        in the CPU Flag register, F. (This macro clears the PGMODE LSb.)
   ;
   ; INPUT: none
   ;
   ;  COST: 4 instruction cycles (in LMM only)

   macro RAM_X_POINTS_TO_INDEXPAGE
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      and  F, ~FLAG_PGMODE_01b
   ENDIF ;  SYSTEM_LARGE_MEMORY_MODEL
   endm

   ;-----------------------------------------------
   ;  Function Prologue
   ;-----------------------------------------------
   ;
   ;  Prologue for functions that run in the LMM and SMM.
   ;

   macro RAM_PROLOGUE( ACTUAL_CLASS )

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_1 )
   ; Nothing to do
   ENDIF ; RAM_USE_CLASS_1

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_2 )
      IF ( SYSTEM_IDXPG_TRACKS_IDX_PP )
         RAM_X_POINTS_TO_STACKPAGE         ; exit native paging mode!
      ENDIF
   ENDIF ; RAM_USE_CLASS_2

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_3 )
      IF ( SYSTEM_IDXPG_TRACKS_STK_PP )
         RAM_X_POINTS_TO_INDEXPAGE         ; exit native paging mode!
      ENDIF
   ENDIF ; RAM_USE_CLASS_3

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_4 )
   ; Nothing to do
   ENDIF ; RAM_USE_CLASS_4

   endm

   ;-----------------------------------------------
   ;  Function Epilogue
   ;-----------------------------------------------
   ;
   ;  Prologue for functions that run in the LMM and SMM.
   ;

   macro RAM_EPILOGUE( ACTUAL_CLASS )

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_1 )
   ; Nothing to do
   ENDIF ; RAM_USE_CLASS_1

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_2 )
      RAM_RESTORE_NATIVE_PAGING
   ENDIF ; RAM_USE_CLASS_2

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_3 )
      RAM_RESTORE_NATIVE_PAGING
   ENDIF ; RAM_USE_CLASS_3

   IF ( @ACTUAL_CLASS & RAM_USE_CLASS_4 )
   ; Nothing to do
   ENDIF ; RAM_USE_CLASS_4

   endm

   ;-----------------------------------------------
   ;  Preserve Register
   ;-----------------------------------------------
   ;
   ;  DESC: Preserve a register value on the stack
   ;
   ; INPUT: Name or address of register in I/O Space
   ;        The I/O bank is an implicit parameter. That is, this function will
   ;        Access the I/O bank currently specified by the CPU Flag register.
   ;
   ;  USES: CPU 'A' register
   ;
   ;  COST: 9 instruction cycles

   macro REG_PRESERVE( IOReg )
   mov   A, reg[ @IOReg ]
   push  A
   endm

   ;-----------------------------------------------
   ;  Restore Register
   ;-----------------------------------------------
   ;
   ;  DESC: Restore a register value from the stack
   ;
   ; INPUT: Name or address of register in I/O Space
   ;        The I/O bank is an implicit parameter. That is, this function will
   ;        Access the I/O bank currently specified by the CPU Flag register.
   ;
   ;  USES: CPU 'A' register
   ;
   ;  COST: 10 instruction cycles

   macro REG_RESTORE( IOReg )
   pop   A
   mov   reg[ @IOReg ], A
   endm

   ;-----------------------------------------------
   ;  Preserve Volatile Page Pointer Registers
   ;-----------------------------------------------
   ;
   ;  DESC: Invoked by ISRs before switching to the LMM mode and calling
   ;        functions that require on it.
   ;
   ; INPUT: none
   ;
   ;  USES: CPU 'A' register
   ;
   ;  COST: 45 instruction cycles (in LMM only)

   macro ISR_PRESERVE_PAGE_POINTERS
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      REG_PRESERVE PRV_PP
      REG_PRESERVE CUR_PP
      REG_PRESERVE IDX_PP
      REG_PRESERVE MVR_PP
      REG_PRESERVE MVW_PP
   ENDIF
   endm

   ;-----------------------------------------------
   ;  Restore Volatile Page Pointer Registers
   ;-----------------------------------------------
   ;
   ;  DESC: Undo for RAM_PRESERVE_PAGE_POINTERS macro. Invoked by ISRs after
   ;        calling functions that run in the LMM mode and before executing
   ;        the RETI instruction.
   ;
   ; INPUT: none
   ;
   ;  USES: CPU 'A' register
   ;
   ;  COST: 50 instruction cycles (in LMM only)

   macro ISR_RESTORE_PAGE_POINTERS
   IF ( SYSTEM_LARGE_MEMORY_MODEL )
      REG_RESTORE MVW_PP
      REG_RESTORE MVR_PP
      REG_RESTORE IDX_PP
      REG_RESTORE CUR_PP
      REG_RESTORE PRV_PP
   ENDIF
   endm

; end of file Memory.inc
;  ******* Extra macros for Memory.inc *******
;
;  Most of the following macros are conditionally compiled so they
;  only produce code if the large memory model is selected.

;-----------------------------------------------
; PRESERVE_CPU_CONTEXT Macro
;-----------------------------------------------
;
; DESC: Preserves CPU context for C function calls from ISRs.
; Also sets the paging mode to the native paging mode.
; All virtual registers and A are preserved. In LMM mode
; all page pointers are preserved. X is not preserved,
; but is preserved within any ImageCraft function call
; automatically by the compiler.
;
; INPUT: N/A
;
; COST: 190 instruction cycles (in LMM+ImageCraft only)
;        20 bytes stack usage  (in LMM+ImageCraft only)
;        59 bytes of flash     (in LMM+ImageCraft only)
;       155 instruction cycles (in SMM+ImageCraft only)
;        16 bytes stack usage  (in SMM+ImageCraft only)  
;        46 bytes of flash     (in SMM+ImageCraft only)

macro PRESERVE_CPU_CONTEXT
IF ( TOOLCHAIN & IMAGECRAFT )
  PUSH A
  IF ( SYSTEM_LARGE_MEMORY_MODEL )
    ISR_PRESERVE_PAGE_POINTERS
    RAM_RESTORE_NATIVE_PAGING
    MOV reg[CUR_PP], >__r0 ;Set CUR_PP to same page as virtual registers
  ENDIF
  MOV A, [__r0]
  PUSH A
  MOV A, [__r1]
  PUSH A
  MOV A, [__r2]
  PUSH A
  MOV A, [__r3]
  PUSH A
  MOV A, [__r4]
  PUSH A
  MOV A, [__r5]
  PUSH A
  MOV A, [__r6]
  PUSH A
  MOV A, [__r7]
  PUSH A
  MOV A, [__r8]
  PUSH A
  MOV A, [__r9]
  PUSH A
  MOV A, [__r10]
  PUSH A
  MOV A, [__r11]
  PUSH A
  MOV A, [__rX]
  PUSH A
  MOV A, [__rY]
  PUSH A
  MOV A, [__rZ]
  PUSH A
  IF ( SYSTEM_LARGE_MEMORY_MODEL )
    MOV REG[CUR_PP], 0
  ENDIF
ENDIF
endm

;-----------------------------------------------
; RESTORE_CPU_CONTEXT Macro
;-----------------------------------------------
;
; DESC: Restores CPU context for C function calls from ISRs.
; Intended to be used in a pair with the PRESERVE_CPU_CONTEXT
; macro.
;
; INPUT: N/A
;
; COST: 185 instruction cycles (in LMM+ImageCraft only)
;        58 bytes of flash     (in LMM+ImageCraft only)
;       155 instruction cycles (in SMM+ImageCraft only)
;        46 bytes of flash     (in SMM+ImageCraft only)

macro RESTORE_CPU_CONTEXT
IF ( TOOLCHAIN & IMAGECRAFT )
  IF ( SYSTEM_LARGE_MEMORY_MODEL )
    RAM_RESTORE_NATIVE_PAGING
    MOV reg[CUR_PP], >__r0 ;Set CUR_PP to same page as virtual registers
  ENDIF
  POP A
  MOV [__rZ], A
  POP A
  MOV [__rY], A
  POP A
  MOV [__rX], A
  POP A
  MOV [__r11], A
  POP A
  MOV [__r10], A
  POP A
  MOV [__r9], A
  POP A
  MOV [__r8], A
  POP A
  MOV [__r7], A
  POP A
  MOV [__r6], A
  POP A
  MOV [__r5], A
  POP A
  MOV [__r4], A
  POP A
  MOV [__r3], A
  POP A
  MOV [__r2], A
  POP A
  MOV [__r1], A
  POP A
  MOV [__r0], A
  IF ( SYSTEM_LARGE_MEMORY_MODEL )
    ISR_RESTORE_PAGE_POINTERS
    AND F, FLAG_PGMODE_0
  ENDIF
  POP A
ENDIF
endm

; end of extra macros to file Memory.inc