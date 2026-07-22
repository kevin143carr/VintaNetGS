         case  on
         longa on
         longi on

****************************************************************
*
* vn_serial_fw_invoke
*
* Call slot-1 Pascal firmware from bank zero.  C input and output
* registers are held in file-scope variables in vn_serial_fw.c.
*
****************************************************************
*
vn_fw_bank0 start VN_FW_BANK0
         kind  $12                     bank-zero DP/stack segment

         ds    256                     reserved for ORCA/C SANE workspace

vn_serial_fw_invoke entry

         php
         sei
         rep   #$30
         pha
         phx
         phy
         phb
         phd
         tsc
         sta   >vn_fw_saved_stack

         lda   #0
         tcd
         pha
         plb
         plb

         lda   #0
         sta   >vn_fw_arbiter_error
         sta   >vn_fw_a_out
         sta   >vn_fw_x_out
         sta   >vn_fw_y_out
         sta   >vn_fw_carry_out

         lda   #1                      request internal slot 1
         jsl   $01FCBC                 GS/OS Slot Arbiter
         txa
         sta   >vn_fw_saved_slots
         bcs   arbiterFailed
         lda   #0                      arbiter leaves D undefined
         tcd

         lda   #$01FF                  firmware uses emulation stack
         tcs
         sec
         xce
         longa off
         longi off

         lda   >vn_fw_operation
         beq   selectInit
         cmp   #1
         beq   selectRead
         cmp   #2
         beq   selectWrite
         lda   >$00C110                low byte of PSTATUS entry
         bra   selectDone
selectInit anop
         lda   >$00C10D                low byte of PINIT entry
         bra   selectDone
selectRead anop
         lda   >$00C10E                low byte of PREAD entry
         bra   selectDone
selectWrite anop
         lda   >$00C10F                low byte of PWRITE entry
selectDone anop
         sta   |firmwareCall+1

         lda   >vn_fw_x_in
         tax
         lda   >vn_fw_y_in
         tay
         lda   >vn_fw_a_in
firmwareCall jsr $C100                 low byte patched from entry table

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out
         php
         pla
         and   #1
         sta   >vn_fw_carry_out

         sei
         clc
         xce
         rep   #$30
         longa on
         longi on

         lda   >vn_fw_saved_slots
         tax
         lda   #$0300                  restore prior slot configuration
         jsl   $01FCBC
         bcc   restoreState
         sta   >vn_fw_arbiter_error
         bra   restoreState

arbiterFailed anop
         sta   >vn_fw_arbiter_error

restoreState anop
         lda   >vn_fw_saved_stack
         tcs
         pld
         plb
         ply
         plx
         pla
         plp
         rtl

         ds    4096                    full ORCA/C stack above shim code
         end
