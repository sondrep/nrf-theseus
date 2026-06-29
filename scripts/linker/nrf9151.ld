/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

_total_rom_length = 0x100000;
_total_ram_length = 0x40000;

_rom_start_addr = 0x0;
_ram_start_addr = 0x20000000;

/* Offsets for SPM */
_rom_offset = 0x00000;
_ram_offset = 0x10000;

/* Shared memory regions */
shmem_ctrl_offset = 0x0;
shmem_ctrl_addr = _ram_start_addr + _ram_offset + shmem_ctrl_offset; /* 0x20010000 */
shmem_ctrl_size = 0x4e8;
shmem_tx_addr = shmem_ctrl_addr + shmem_ctrl_size; /* 0x200104e8 */
shmem_tx_size = 8192; /* 0x2000 */
shmem_rx_addr = shmem_tx_addr + shmem_tx_size; /* 0x200124e8 */
shmem_rx_size = 8192; /* 0x2000 */
shmem_trace_addr = shmem_rx_addr + shmem_rx_size; /* 0x200144e8 */
shmem_trace_size = 0x4000;
shmem_start_addr = shmem_ctrl_addr; /* 0x20010000 */
shmem_end_addr = shmem_trace_addr + shmem_trace_size; /* 0x200184e8 */
shmem_size = shmem_end_addr - shmem_start_addr; /* 0xe4e8 */


_rom_location = _rom_start_addr + _rom_offset; /* 0x50000 */
_ram_location = _ram_start_addr + _ram_offset + shmem_size; /* 0x2001e4e8 */


MEMORY
{
  FLASH (rx) : ORIGIN = _rom_location, LENGTH = _total_rom_length - _rom_offset
  RAM (rwx) : ORIGIN = _ram_location, LENGTH = _total_ram_length - _ram_offset - shmem_size
}


INCLUDE "nrf_common.ld"
