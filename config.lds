PHDRS {
  load_r PT_LOAD;
  load_w PT_LOAD;
  load_ws PT_LOAD;
}
SECTIONS {
  . = CONFIG_START;
  __config_start_ro = .;
  .rodata_head : { *(.rodata_head) } :load_r
  .rodata : { *(.rodata) *(.rodata.str1.1) *(.rodata.str1.4) *(.rodata.str.1.8)
		. = ALIGN(0x1000);
	} :load_r
  __config_start_rw = .;
  .data : { *(.data)
		. = ALIGN(0x1000);
	} :load_w
  __config_start_rws = .;
  .data_shared : { *(.data_shared)
		. = ALIGN(0x1000);
	} :load_ws
  __config_end = .;
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.eh_frame_hdr) } :NONE
}
entry_sym = 0;
ENTRY(entry_sym)
PROVIDE(CONFIG_START = 0xeeeee000);
