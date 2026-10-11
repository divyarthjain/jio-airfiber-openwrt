# Procedure (in the order it was done)

Mac side: Ethernet adapter to the router's **WAN (blue) port**, adapter set to `192.168.9.2/24`. A different subnet from the home network so Wi-Fi can stay on.

## 1. Read the boot log
Wire GND, TX, RX as in [01](01-hardware-and-uart.md). Power the router. Confirm U-Boot and Linux output scrolls.

## 2. Stop autoboot at the U-Boot menu
The countdown is only about 2 seconds. See [gotchas](03-gotchas.md) for what worked. At the menu:
- Option 8 "Start Web failsafe" exits at once (`ipaddr not set`), then a 5 second autoboot countdown starts. Send a key in that window.
- This lands on the `MT7986>` console with **no password**.
- Menu options 2 to 7 and 0 ask for a login. `admin`/`admin` was **rejected** on this unit.

## 3. Boot the OpenWrt initramfs from RAM (writes nothing)
Serve the images from the Mac (`python3 tools/tftp_server.py 192.168.9.2 <folder>`, needs sudo for port 69). Image files from `downloads.openwrt.org/releases/25.12.5/targets/mediatek/filogic/`:
- `openwrt-25.12.5-mediatek-filogic-jiorouter_ax6000-jidu6101-initramfs-kernel.bin`
- `openwrt-25.12.5-mediatek-filogic-jiorouter_ax6000-jidu6101-squashfs-sysupgrade.bin`

Check them against the release `sha256sums`. Then in U-Boot:
```
setenv ipaddr 192.168.9.1
setenv serverip 192.168.9.2
ping 192.168.9.2
tftpboot 0x46000000 openwrt-25.12.5-mediatek-filogic-jiorouter_ax6000-jidu6101-initramfs-kernel.bin
fdt addr $(fdtcontroladdr)
fdt rm /signature
bootm 0x46000000
```
U-Boot's network is on the **WAN** port, not the LAN ports. A cable on LAN gave `host is not alive`.

## 4. Back up all flash before writing anything
In the RAM OpenWrt, give the WAN port `192.168.9.1/24`, stop the firewall, add an SSH key, then:
```
ip addr add 192.168.9.1/24 dev wan
/etc/init.d/firewall stop; nft flush ruleset
```
Run [`tools/backup.sh`](../tools/backup.sh) on the Mac. All 8 partitions matched their expected sizes (about 251 MB total). Keep the dumps private.

## 5. Flash
Copy the sysupgrade image over SSH (scp fails, no sftp-server in initramfs):
```
ssh root@192.168.9.1 "cat > /tmp/sysupgrade.bin" < <sysupgrade image>
ssh root@192.168.9.1 'sha256sum /tmp/sysupgrade.bin'   # compare
ssh root@192.168.9.1 'sysupgrade -n -v /tmp/sysupgrade.bin'
```
After the reboot, stock U-Boot rejects the image (no `/rootfs` node in the FIT, signature) and falls into web failsafe.

## 6. Make it autoboot
At the failsafe, press Ctrl+C to get `MT7986>`, then:
```
setenv bootcmd 'ubi read 46000000 kernel; fdt addr $(fdtcontroladdr); fdt rm /signature; bootm 0x46000000'
setenv ipaddr
setenv bootdelay 0
saveenv
reset
```
Unsetting `ipaddr` makes the failsafe `httpd` fail instantly, so U-Boot goes on to `bootcmd`. Result: cold boot straight into OpenWrt from flash.

## 7. Keep it from breaking on upgrades
Install [`tools/bootcmd-guard`](../tools/bootcmd-guard) as `/etc/init.d/bootcmd-guard`, `chmod +x`, `enable`. It re-sets `bootcmd`, `ipaddr` and `bootdelay` on every boot. Transfer it over SSH. Pasting it over the serial console dropped characters.

## 8. Set up as a router
- WAN port to the existing router. It got a DHCP address and internet worked.
- LuCI: `apk update && apk add luci` (25.12 uses `apk`, not `opkg`).
- LAN moved to `192.168.5.1/24` to avoid clashing with the upstream `192.168.1.x`. Set the netmask explicitly:
  ```
  uci set network.lan.ipaddr=192.168.5.1
  uci set network.lan.netmask=255.255.255.0
  uci commit network; /etc/init.d/network restart
  ```
- Wi-Fi, both radios:
  ```
  uci set wireless.default_radioN.encryption=sae-mixed   # WPA2/WPA3
  uci set wireless.default_radioN.ieee80211w=1           # PMF optional, keeps WPA2 devices working
  uci set wireless.radioN.country=IN
  uci set wireless.default_radioN.key='<your password>'
  uci commit wireless; wifi reload
  ```
- Set a root password.

## Rolling back
U-Boot and BL2 are never touched, so they survive. Catch the menu again, boot the initramfs over TFTP, then write the saved `ubi` and `u-boot-env` dumps back.
