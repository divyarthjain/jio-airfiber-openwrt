// TX-priority bridge: always forward keystrokes (Mac->router); never block
// on router->Mac log traffic (drop it if USB is backed up). Guarantees keys
// reach U-Boot even during the boot-time serial flood.
#define RXPIN 18   // RX_U1 <- router TX
#define TXPIN 17   // TX_U1 -> router RX
uint8_t rbuf[256], tbuf[64];
void setup() {
  Serial.begin(115200);
  Serial1.setRxBufferSize(8192);
  Serial1.begin(115200, SERIAL_8N1, RXPIN, TXPIN);
}
void loop() {
  // 1) keystrokes first, always
  int k = Serial.available();
  if (k > 0) { if (k > 64) k = 64; int m = Serial.read(tbuf, k); if (m > 0) Serial1.write(tbuf, m); }
  // 2) log traffic only if USB has room; otherwise skip (don't block)
  int space = Serial.availableForWrite();
  if (space > 0) {
    int n = Serial1.available();
    if (n > 0) { if (n > space) n = space; if (n > 256) n = 256; int m = Serial1.read(rbuf, n); if (m > 0) Serial.write(rbuf, m); }
  }
}
