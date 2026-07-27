         case  on
         longa on
         longi on

****************************************************************
*
* vn_serial_fw_invoke
*
* Call selected Pascal serial firmware from bank zero.  C input and
* output registers are held in file-scope variables in vn_serial_fw.c.
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
         cld                           required by Slot Arbiter contract
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

         lda   >vn_fw_operation
         cmp   #4                      native entry/return probe
         bne   notNativeProbe
         brl   restoreState
notNativeProbe anop
         cmp   #5                      emulation transition probe
         beq   prepareEmulation

         lda   #$8000                  save current slot configuration
         jsl   $E10208                 GS/OS Slot Arbiter duplicate vector
         txa
         sta   >vn_fw_saved_slots

         lda   #0                      arbiter leaves D undefined
         tcd

         lda   >vn_fw_slot_request     request selected internal port
         jsl   $E10208                 GS/OS Slot Arbiter duplicate vector
         bcc   arbiterSucceeded
         brl   arbiterFailed
arbiterSucceeded anop
         lda   #0                      arbiter leaves D undefined
         tcd

         lda   >vn_fw_operation
         cmp   #6                      slot arbiter probe
         bne   prepareEmulation
         brl   restoreSlotState

prepareEmulation anop
         lda   #$01FF                  firmware uses emulation stack
         tcs
         sec
         xce
         longa off
         longi off

         lda   >vn_fw_operation
         cmp   #5
         bne   notEmulationProbe
         brl   leaveEmulation
notEmulationProbe anop
         cmp   #0
         beq   selectInit
         cmp   #1
         beq   selectRead
         cmp   #2
         beq   selectWrite
         lda   >vn_fw_slot_number
         cmp   #2
         beq   selectStatus2
         lda   >$00C110                low byte of PSTATUS entry
         bra   selectDone
selectStatus2 anop
         lda   >$00C210                low byte of PSTATUS entry
         bra   selectDone
selectInit anop
         lda   >vn_fw_slot_number
         cmp   #2
         beq   selectInit2
         lda   >$00C10D                low byte of PINIT entry
         bra   selectDone
selectInit2 anop
         lda   >$00C20D                low byte of PINIT entry
         bra   selectDone
selectRead anop
         lda   >vn_fw_slot_number
         cmp   #2
         beq   selectRead2
         lda   >$00C10E                low byte of PREAD entry
         bra   selectDone
selectRead2 anop
         lda   >$00C20E                low byte of PREAD entry
         bra   selectDone
selectWrite anop
         lda   >vn_fw_slot_number
         cmp   #2
         beq   selectWrite2
         lda   >$00C10F                low byte of PWRITE entry
         bra   selectDone
selectWrite2 anop
         lda   >$00C20F                low byte of PWRITE entry
selectDone anop
         sta   |firmwareCall+1
         lda   >vn_fw_slot_page
         sta   |firmwareCall+2

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

leaveEmulation anop
         sei
         clc
         xce
         rep   #$30
         cld                           firmware may not preserve decimal mode
         longa on
         longi on

         lda   >vn_fw_operation
         cmp   #5
         beq   restoreState

restoreSlotState anop
         lda   >vn_fw_saved_slots
         tax
         lda   #$0300                  restore prior slot configuration
         jsl   $E10208
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

****************************************************************
*
* vn_serial_fw_ext_invoke
*
* Call the slot-1 serial firmware optional-control routine with a
* static command list in bank zero.  Operations 3 and 4 perform the
* documented GetModeBits/modify/SetModeBits transaction while
* interrupts remain disabled inside this single bridge call.
*
****************************************************************
*
vn_serial_fw_ext_invoke entry

         php
         sei
         rep   #$30
         cld
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
         sta   >vn_fw_ext_result_code
         sta   >vn_fw_mode_out_low
         sta   >vn_fw_mode_out_high
         sta   >vn_fw_ext_checkpoint
         sta   >vn_fw_ext_ptr_a
         sta   >vn_fw_ext_ptr_x
         sta   >vn_fw_ext_ptr_y
         sta   >vn_fw_ext_cmd0
         sta   >vn_fw_ext_cmd1
         sta   >vn_fw_ext_cmd2
         sta   >vn_fw_ext_cmd3
         sta   >vn_fw_ext_status
         sta   >vn_fw_ext_dbr
         sta   >vn_fw_ext_dp

         lda   #1
         sta   >vn_fw_ext_checkpoint
         php
         sep   #$20
         pla
         sta   >vn_fw_ext_status
         phb
         pla
         sta   >vn_fw_ext_dbr
         rep   #$30
         tdc
         sta   >vn_fw_ext_dp

         sep   #$20
         lda   >$00C112                optional-control offset
         rep   #$30
         and   #$00FF
         sta   >vn_fw_ext_offset
         lda   #2
         sta   >vn_fw_ext_checkpoint
         lda   >vn_fw_ext_offset
         clc
         adc   #$C100
         sta   >vn_fw_ext_dispatch
         sta   |vn_fw_ext_call+1
         lda   #3
         sta   >vn_fw_ext_checkpoint

         lda   #$8000                  save current slot configuration
         jsl   $E10208
         txa
         sta   >vn_fw_saved_slots

         lda   #0
         tcd

         lda   #$0001                  request internal slot-1 printer port
         jsl   $E10208
         bcc   extArbiterSucceeded
         brl   extArbiterFailed
extArbiterSucceeded anop
         lda   #0
         tcd

         lda   >vn_fw_ext_operation
         cmp   #2
         beq   extPrepareSet
         cmp   #3
         beq   extPrepareAtomicGet
         cmp   #4
         beq   extPrepareAtomicGet

extPrepareGet anop
         lda   #$0003
         sta   |vn_fw_get_mode_cmd
         lda   #$0000
         sta   |vn_fw_get_mode_cmd+2
         sta   |vn_fw_get_mode_cmd+4
         sta   |vn_fw_get_mode_cmd+6
         brl   extEnterFirmwareGet

extPrepareSet anop
         lda   #$0103                  count $03, command $01
         sta   |vn_fw_set_mode_cmd
         lda   #0
         sta   |vn_fw_set_mode_cmd+2
         lda   >vn_fw_mode_in_low
         sta   |vn_fw_set_mode_cmd+4
         lda   >vn_fw_mode_in_high
         sta   |vn_fw_set_mode_cmd+6
         brl   extEnterFirmwareSet

extPrepareAtomicGet anop
         lda   #$0003
         sta   |vn_fw_get_mode_cmd
         lda   #$0000
         sta   |vn_fw_get_mode_cmd+2
         sta   |vn_fw_get_mode_cmd+4
         sta   |vn_fw_get_mode_cmd+6
         brl   extEnterFirmwareAtomicGet

extPrepareAtomicSet anop
         lda   |vn_fw_get_mode_cmd+2
         beq   extAtomicGetSucceeded
         brl   extAfterCall
extAtomicGetSucceeded anop
         lda   >vn_fw_ext_operation
         cmp   #4
         beq   extAtomicClearBit
         lda   |vn_fw_get_mode_cmd+6
         ora   #$0080                  bit 23 = bit 7 of ModeBitImage byte 2
         bra   extAtomicStoreBit
extAtomicClearBit anop
         lda   |vn_fw_get_mode_cmd+6
         and   #$FF7F                  clear bit 23 before setup commands
extAtomicStoreBit anop
         sta   |vn_fw_set_mode_cmd+6
         lda   |vn_fw_get_mode_cmd+4
         sta   |vn_fw_set_mode_cmd+4
         lda   #$0103
         sta   |vn_fw_set_mode_cmd
         lda   #0
         sta   |vn_fw_set_mode_cmd+2
         brl   extEnterFirmwareSet

extEnterFirmwareAtomicGet anop
         lda   #1
         sta   >vn_fw_atomic_pending
         lda   #0
         sta   >vn_fw_active_set
         lda   #<vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_a
         lda   #>vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_x
         lda   #^vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_y
         bra   extEnterFirmwareCommon

extEnterFirmwareGet anop
         lda   #0
         sta   >vn_fw_atomic_pending
         sta   >vn_fw_active_set
         lda   #<vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_a
         lda   #>vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_x
         lda   #^vn_fw_get_mode_cmd
         sta   >vn_fw_ext_ptr_y
         bra   extEnterFirmwareCommon

extEnterFirmwareSet anop
         lda   #1
         sta   >vn_fw_active_set
         lda   #<vn_fw_set_mode_cmd
         sta   >vn_fw_ext_ptr_a
         lda   #>vn_fw_set_mode_cmd
         sta   >vn_fw_ext_ptr_x
         lda   #^vn_fw_set_mode_cmd
         sta   >vn_fw_ext_ptr_y

extEnterFirmwareCommon anop
         lda   >vn_fw_active_set
         beq   extSnapshotGet
         lda   |vn_fw_set_mode_cmd
         sta   >vn_fw_ext_cmd0
         lda   |vn_fw_set_mode_cmd+2
         sta   >vn_fw_ext_cmd1
         lda   |vn_fw_set_mode_cmd+4
         sta   >vn_fw_ext_cmd2
         lda   |vn_fw_set_mode_cmd+6
         sta   >vn_fw_ext_cmd3
         bra   extSnapshotDone
extSnapshotGet anop
         lda   |vn_fw_get_mode_cmd
         sta   >vn_fw_ext_cmd0
         lda   |vn_fw_get_mode_cmd+2
         sta   >vn_fw_ext_cmd1
         lda   |vn_fw_get_mode_cmd+4
         sta   >vn_fw_ext_cmd2
         lda   |vn_fw_get_mode_cmd+6
         sta   >vn_fw_ext_cmd3
extSnapshotDone anop
         lda   #4
         sta   >vn_fw_ext_checkpoint
         lda   #$01FF
         tcs
         sec
         xce
         longa off
         longi off

         lda   #5
         sta   >vn_fw_ext_checkpoint

         lda   >vn_fw_ext_ptr_y
         tay
         lda   >vn_fw_ext_ptr_x
         tax
         lda   >vn_fw_ext_ptr_a
vn_fw_ext_call entry
         jsr   $C100

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out
         php
         pla
         and   #1
         sta   >vn_fw_carry_out

         lda   #6
         sta   >vn_fw_ext_checkpoint
         sei
         clc
         xce
         rep   #$30
         cld
         longa on
         longi on
         lda   >vn_fw_saved_stack
         tcs

         lda   >vn_fw_active_set
         beq   extStoreGet
         lda   |vn_fw_set_mode_cmd+2
         sta   >vn_fw_ext_result_code
         lda   |vn_fw_set_mode_cmd+4
         sta   >vn_fw_mode_out_low
         lda   |vn_fw_set_mode_cmd+6
         sta   >vn_fw_mode_out_high
         lda   |vn_fw_set_mode_cmd
         sta   >vn_fw_ext_cmd0
         lda   |vn_fw_set_mode_cmd+2
         sta   >vn_fw_ext_cmd1
         lda   |vn_fw_set_mode_cmd+4
         sta   >vn_fw_ext_cmd2
         lda   |vn_fw_set_mode_cmd+6
         sta   >vn_fw_ext_cmd3
         bra   extStoreDone
extStoreGet anop
         lda   |vn_fw_get_mode_cmd+2
         sta   >vn_fw_ext_result_code
         lda   |vn_fw_get_mode_cmd+4
         sta   >vn_fw_mode_out_low
         lda   |vn_fw_get_mode_cmd+6
         sta   >vn_fw_mode_out_high
         lda   |vn_fw_get_mode_cmd
         sta   >vn_fw_ext_cmd0
         lda   |vn_fw_get_mode_cmd+2
         sta   >vn_fw_ext_cmd1
         lda   |vn_fw_get_mode_cmd+4
         sta   >vn_fw_ext_cmd2
         lda   |vn_fw_get_mode_cmd+6
         sta   >vn_fw_ext_cmd3
extStoreDone anop
         lda   #7
         sta   >vn_fw_ext_checkpoint
         lda   >vn_fw_atomic_pending
         beq   extAfterCall
         lda   #0
         sta   >vn_fw_atomic_pending
         brl   extPrepareAtomicSet

extAfterCall anop
         lda   >vn_fw_saved_slots
         tax
         lda   #$0300
         jsl   $E10208
         bcc   extRestoreState
         sta   >vn_fw_arbiter_error
         bra   extRestoreState

extArbiterFailed anop
         sta   >vn_fw_arbiter_error

extRestoreState anop
         lda   #8
         sta   >vn_fw_ext_checkpoint
         lda   >vn_fw_saved_stack
         tcs
         pld
         plb
         ply
         plx
         pla
         plp
         rtl

vn_fw_get_mode_cmd entry
         ds    8
vn_fw_set_mode_cmd entry
         ds    8
vn_fw_atomic_pending ds 2
vn_fw_active_set ds 2

         ds    4096                    full ORCA/C stack above shim code
         end

****************************************************************
*
* vn_serial_fw_arbiter_query
*
* Query the GS/OS Slot Arbiter from an ordinary executable code
* segment.  This deliberately avoids the bank-zero firmware shim,
* direct-page changes, stack changes, data-bank changes, emulation
* mode, and slot ownership requests.
*
****************************************************************
*
vn_fw_arbiter_query_seg start VN_FW_ARBITER_QUERY

vn_serial_fw_arbiter_query entry

         php
         rep   #$30
         cld
         pha
         phx
         phy
         phb
         phd

         lda   #0
         sta   >vn_fw_a_out
         sta   >vn_fw_x_out
         sta   >vn_fw_y_out
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

         sep   #$20
         lda   >$E0C068                save language-card state
         pha
         lda   >$E0C08B                map GS/OS language-card bank 1
         lda   >$E0C08B
         rep   #$30

         lda   #$8000                  query current slot state only
         jsl   $01FCBC                 GS/OS Slot Arbiter

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out

         bcc   querySucceeded
         lda   #1
         sta   >vn_fw_carry_out
         lda   >vn_fw_a_out
         sta   >vn_fw_arbiter_error
         bra   queryDone

querySucceeded anop
         lda   #0
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

queryDone anop
         sep   #$20
         lda   >$E0C083                restore prior language-card mapping
         lda   >$E0C083
         pla
         sta   >$E0C068
         rep   #$30

         pld
         plb
         ply
         plx
         pla
         plp
         rtl

         end

****************************************************************
*
* vn_serial_fw_arbiter_probe
*
* Query the GS/OS Slot Arbiter through $E10208 with the A-register
* request value supplied in vn_fw_a_in.  This routine does not
* restore slot state and must only be used for no-dependency queries.
*
****************************************************************
*
vn_fw_arbiter_probe_seg start VN_FW_ARBITER_PROBE

vn_serial_fw_arbiter_probe entry

         php
         rep   #$30
         cld
         pha
         phx
         phy
         phb
         phd

         lda   #0
         sta   >vn_fw_a_out
         sta   >vn_fw_x_out
         sta   >vn_fw_y_out
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

         lda   >vn_fw_a_in
         jsl   $E10208                 GS/OS Slot Arbiter duplicate vector

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out

         bcc   probeSucceeded
         lda   #1
         sta   >vn_fw_carry_out
         lda   >vn_fw_a_out
         sta   >vn_fw_arbiter_error
         bra   probeDone

probeSucceeded anop
         lda   #0
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

probeDone anop
         pld
         plb
         ply
         plx
         pla
         plp
         rtl

         end

****************************************************************
*
* vn_serial_fw_arbiter_request_restore
*
* Request a slot through $E10208 using vn_fw_a_in, but first save
* the current bit-encoded slot configuration and restore it before
* returning to C.  The request result is returned unless restore
* itself fails.
*
****************************************************************
*
vn_fw_arbiter_req_seg start VN_FW_ARBITER_REQ

vn_serial_fw_arbiter_request_restore entry

         php
         rep   #$30
         cld
         pha
         phx
         phy
         phb
         phd

         lda   #0
         sta   >vn_fw_a_out
         sta   >vn_fw_x_out
         sta   >vn_fw_y_out
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error
         sta   >vn_fw_saved_slots

         lda   #$8000                  save current slot configuration
         jsl   $E10208
         txa
         sta   >vn_fw_saved_slots

         lda   >vn_fw_a_in             request selected slot or internal port
         jsl   $E10208

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out

         bcc   requestSucceeded
         lda   #1
         sta   >vn_fw_carry_out
         lda   >vn_fw_a_out
         sta   >vn_fw_arbiter_error
         bra   requestDone

requestSucceeded anop
         lda   #0
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

requestDone anop
         lda   >vn_fw_saved_slots
         tax
         lda   #$0300                  restore prior slot configuration
         jsl   $E10208
         bcc   requestRestoreDone

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out
         lda   #1
         sta   >vn_fw_carry_out
         lda   >vn_fw_a_out
         sta   >vn_fw_arbiter_error

requestRestoreDone anop
         pld
         plb
         ply
         plx
         pla
         plp
         rtl

         end

****************************************************************
*
* vn_serial_fw_arbiter_query_e1
*
* Query the GS/OS Slot Arbiter through the application-safe
* duplicate vector at $E10208.  This uses the normal C stack and
* deliberately performs no manual language-card mapping.
*
****************************************************************
*
vn_fw_arbiter_e1_seg start VN_FW_ARBITER_E1

vn_serial_fw_arbiter_query_e1 entry

         php
         rep   #$30
         cld
         pha
         phx
         phy
         phb
         phd

         lda   #0
         sta   >vn_fw_a_out
         sta   >vn_fw_x_out
         sta   >vn_fw_y_out
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

         lda   #$8000                  query current slot state only
         jsl   $E10208                 GS/OS Slot Arbiter duplicate vector

         sta   >vn_fw_a_out
         txa
         sta   >vn_fw_x_out
         tya
         sta   >vn_fw_y_out

         bcc   queryE1Succeeded
         lda   #1
         sta   >vn_fw_carry_out
         lda   >vn_fw_a_out
         sta   >vn_fw_arbiter_error
         bra   queryE1Done

queryE1Succeeded anop
         lda   #0
         sta   >vn_fw_carry_out
         sta   >vn_fw_arbiter_error

queryE1Done anop
         pld
         plb
         ply
         plx
         pla
         plp
         rtl

         end
