/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

_total_rom_length = 0x8000;
_total_ram_length = 0x40000;

_rom_start_addr = 0x0; /* Start address of secure flash */
_ram_start_addr = 0x20000000;

/* Offsets for SPM */
_rom_offset = 0x00000;
_ram_offset = 0x38000;

_rom_location = _rom_start_addr + _rom_offset;
_ram_location = _ram_start_addr + _ram_offset;


MEMORY
{
  FLASH (rx) : ORIGIN = _rom_location, LENGTH = _total_rom_length - _rom_offset
  RAM (rwx) : ORIGIN = _ram_location, LENGTH = _total_ram_length - _ram_offset
}


INCLUDE "nrf_common.ld"
