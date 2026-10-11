#!/bin/sh
# Dump all MTD partitions of a running OpenWrt router over SSH.
# Usage: backup.sh <router-ip> <ssh-key> <out-dir>   (keep the output private)
host=$1; key=$2; out=$3; mkdir -p "$out"
while read idx name size; do
  ssh -n -i "$key" -o StrictHostKeyChecking=no "root@$host" "cat /dev/mtd$idx" > "$out/mtd${idx}_$name.bin"
  got=$(wc -c < "$out/mtd${idx}_$name.bin" | tr -d ' ')
  [ "$got" = "$size" ] && echo "mtd$idx $name OK" || echo "mtd$idx $name SIZE MISMATCH ($got/$size)"
done <<'LIST'
0 BL2 1048576
1 u-boot-env 524288
2 Factory 2097152
3 FIP 2097152
4 ubi 146800640
5 ubi2 94371840
6 MFG 2097152
7 Reserved 2097152
LIST
(cd "$out" && shasum -a 256 *.bin > sha256sums.txt)
