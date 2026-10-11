# Gotchas

| Problem | Cause | Fix |
|---|---|---|
| Spamming spaces never stopped autoboot | The countdown is about 2 s, and keys sent during the boot-log flood did not land | A watcher that waits for the "Boot Menu" text, then sends keys. This worked. I also rewrote the bridge sketch three times (bigger buffer, interleaving, TX priority). It is unclear whether any of that was needed |
| Serial port kept resetting the ESP32-S3 | `printf ' ' > /dev/cu.usbmodemX` opens and closes the port each time | One long-lived process holds the port; send through a FIFO |
| Two captures lost each other's data | Two `cat` processes reading one port | Run exactly one reader |
| `admin`/`admin` rejected in the U-Boot menu | This unit uses a different password than the guide says | Skip it. Option 8 then Ctrl+C reaches the console without a login |
| U-Boot `ping` fails | U-Boot's Ethernet is the WAN port | Cable to the blue port for TFTP |
| `nc` backups came out truncated | Unreliable transfer | Use SSH: `ssh root@host 'cat /dev/mtdN' > file`. Use `ssh -n` inside loops, or the first call eats the loop input |
| `scp` fails | No sftp-server in the initramfs | Pipe over ssh: `ssh host 'cat > file' < local` |
| SSH to the RAM system refused | Firewall active on WAN | `/etc/init.d/firewall stop; nft flush ruleset` |
| Pasting the guard script over serial lost characters | Long line, no flow control | Send it over SSH |
| LAN came up as `192.168.5.1/32`, no DHCP | Only the IP was changed | Set `network.lan.netmask` too. Reached the box over its IPv6 ULA to fix it |
| UART USB-C port did not enumerate on the ESP32-S3 board | Unknown (cable or bridge chip) | Used the native USB port |
| First `brew install arduino-cli` failed | Homebrew directory permissions | Used the standalone arduino-cli binary |

## Hard rules
- Never write **FIP** (mtd3) with an unsigned image. BL2 then halts before UART starts and recovery needs the NAND desoldered.
- Never write **Factory** (mtd2).
- Never connect VCC on the UART.
- Take the full dump before the first `sysupgrade`.
- A later `sysupgrade` can wipe the U-Boot variables. That is why the guard script exists.
