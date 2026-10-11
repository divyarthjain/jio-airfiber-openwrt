# Hardware and UART

## Opening the unit
Remove the outer white plastic cover. The internal assembly (board plus shields) comes out as one piece.

## Where the serial header is
Two 4-pin spots sit **at the board edge, beside the four Ethernet ports**. Neither is near the SoC shield, unlike what some guides say.

| Spot | Type | Used? |
|---|---|---|
| **J1** | 4 through-hole pins in a row, pin 1 has a **square pad** | **Yes, this is the UART** |
| J15 | 4-pin footprint for a white female plastic connector (SMD) | No. Purpose unknown, not tested |

## J1 pins (counting from the square pad)
| Pin | Finding |
|---|---|
| 1 (square) | Not measured. Assumed VCC, **left unconnected** |
| 2 (next to VCC) | **Router TX** (wired to ESP32 GPIO18) |
| 3 (next to GND) | **Router RX** (wired to ESP32 GPIO17) |
| 4 (last, round) | **GND**, confirmed by continuity to the shield can |

**Still open:** pin 1 was never measured with a meter, so "VCC" is an assumption. Add a photo of J1 here.

Rule that held: wire GND, TX, RX only. Never wire VCC.

## Serial settings
115200 baud, 8N1, 3.3 V logic. The U-Boot boot log and Linux console both appear on it.

## ESP32-S3 as the USB-serial adapter
No USB-UART adapter was on hand, so an ESP32-S3 dev board (EdgeHax S3-PRO) was flashed as a pass-through bridge ([`tools/uartbridge/uartbridge.ino`](../tools/uartbridge/uartbridge.ino)).

| Router (J1) | ESP32-S3 |
|---|---|
| TX | GPIO18 (RX_U1) |
| RX | GPIO17 (TX_U1) |
| GND | GND |
| VCC (pin 1) | not connected |

Notes:
- Both boards stay powered separately. Only GND is shared.
- On this board the "UART" USB-C port did not enumerate. The native "USB" port did, so the sketch is built with `USB CDC On Boot: Enabled` and shows up as `/dev/cu.usbmodemXXXX`.
- Built and uploaded with `arduino-cli` (FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc`).
- Reading the console is read-only and cannot brick anything.

## What the stock boot log shows
- BL2 v2.8, built 22 Aug 2023, with anti-rollback checks (`bl_ar_ver:0>=0 OK`)
- U-Boot 2023.04 with a boot menu (see [procedure](02-procedure.md))
- Kernel FIT verified with RSA-2048 (`fit_key`), rootfs on dm-verity. This is the secure-boot lock.
- NAND: Winbond 256 MiB, UBI volumes kernel/rootfs/rootfs_data and an env volume, plus dual-boot slots

## MTD partitions
| mtd | Name | Size | Note |
|---|---|---|---|
| 0 | BL2 | 1 MiB | |
| 1 | u-boot-env | 512 KiB | |
| 2 | Factory | 2 MiB | RF calibration and MACs. **Do not write** |
| 3 | FIP | 2 MiB | Signed BL31 + U-Boot. **Never flash unsigned** |
| 4 | ubi | 140 MiB | Where OpenWrt goes |
| 5 | ubi2 | 90 MiB | Second dual-boot slot |
| 6 | MFG | 2 MiB | |
| 7 | Reserved | 2 MiB | |
