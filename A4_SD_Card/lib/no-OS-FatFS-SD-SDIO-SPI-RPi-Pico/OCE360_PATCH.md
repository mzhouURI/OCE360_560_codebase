# no-OS-FatFS-SD-SDIO-SPI-RPi-Pico v3.6.2 (patched)

Source: https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico, tag v3.6.2,
`src/` and `LICENSE` only (Apache 2.0).

One change, in `src/sd_driver/SPI/sd_card_spi.c`, marked `OCE360 patch`:
support Standard Capacity (SDSC) cards, 2 GB and under, like the course's 128 MB cards.
Upstream refuses them ("SD Standard Capacity Memory Card unsupported"). SDSC cards take
byte addresses for read/write commands instead of block numbers, so `sd_cmd()` multiplies
the block number by 512 for those cards. Tested 2026-10-04 on a Pico 2 with a 128 MB card
(FAT16): mount, append, read back.
