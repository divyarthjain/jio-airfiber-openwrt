# Airtel AirFiber indoor unit (AAP4221ZY): research only

**Not flashed.** These notes come from web research and the unit's sticker. Nothing here was verified on the device.

## The unit
- Sticker: "Commodity dual band Wi-Fi six AP", model **AAP4221ZY**, 12 V / 3 A barrel jack, 2 LAN + 1 WAN, made in India.
- Sticker credentials follow the pattern admin / serial number. Do not publish yours.
- The matching outdoor unit is reported as **AOD4101ZY**. The indoor unit powers it over PoE on the WAN port.
- Earlier Airtel units used Nokia FastMile and Arcadyan (AID-4461AR) hardware. Forum reports only.

## Hardware (per OpenWrt PR 22809 and TechInfoDepot)
| | |
|---|---|
| OEM | Probably Zyxel EX3310-T0 (from firmware DTS, not printed on the label) |
| FCC ID | I88-03911 |
| SoC | MediaTek MT7981B, 2x Cortex-A53 @ 1.3 GHz |
| RAM / flash | 512 MB DDR3, 512 MB Micron SPI-NAND |
| Wi-Fi | MT7976 |
| Switch | Airoha AN8855 |
| PoE enable | GPIO 10 |

## Software
- OpenWrt support was merged in 2026 (PR 22809, backport PR 24826 to 25.12). Images: `openwrt-mediatek-filogic-airtel_aap4221zy-{initramfs-kernel,squashfs-sysupgrade}.bin`.
- Boot chain is BL2, U-Boot, then Zyxel zloader. zloader needs a UBI volume `zyfwinfo`, and OpenWrt must go in the first bank.
- Stock root route in the PR: decrypt the config backup, enable telnet/SSH, re-upload. UART console is gated by `EngDebugFlag`.
- Flashing it would turn off PoE to the outdoor unit unless GPIO 10 is handled.

## Sources
- https://github.com/openwrt/openwrt/pull/22809
- https://techinfodepot.shoutwiki.com/wiki/Airtel_AirFiber_AAP4221ZY
- https://broadband.forum/threads/how-to-unlock-the-airtel-airfiber-5g-router-and-access-hidden-tr-369-settings.236314/
