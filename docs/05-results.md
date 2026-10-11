# Results: Wi-Fi channel tuning

After the OpenWrt install, a router scan from the new box found the old stock Wi-Fi sharing 2.4 GHz channel 1 with a neighbour at -43 dBm, strong enough to cost real throughput. 5 GHz was moved to channel 149 (completely clear, no radar-band waiting), 2.4 GHz to channel 11.

Both networks run on the same upstream line (same fiber, same 100 Mbps plan, tested back to back at night with no other traffic), so this is a clean comparison of the Wi-Fi hop alone.

| | Old Wi-Fi (channel 1) | OpenWrt box (channel 149) |
|---|---|---|
| Speed | 63 Mbps | 92 Mbps |

![63 Mbps on the old Wi-Fi](images/speedtest-old-wifi.jpg) ![92 Mbps on Negentropy-5G](images/speedtest-negentropy-5g.jpg)

Full tuning details and scan output: see the session's channel-selection work (not yet written up as a standalone doc — this file just records the before/after).
