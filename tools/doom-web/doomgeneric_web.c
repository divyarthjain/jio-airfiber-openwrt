// SPDX-License-Identifier: GPL-2.0-or-later (links into doomgeneric / Chocolate Doom, GPL-2.0)
//
// Browser backend for doomgeneric (replaces doomgeneric_ascii.c).
// The game runs here; it serves one web page and one WebSocket on port 8666 (-port N).
// Frames go out as [u16 w][u16 h][w*h RGB565], little endian.
// Keys come in as 2-byte messages [pressed, doomkey]. One player at a time.
//
// Limit: raw RGB565 is ~4.5 MB/s at 35 fps, fine on LAN; add zlib or palette frames to play over the internet.
//

#include "doomgeneric.h"
#include "m_argv.h"
#include "sha1.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static const char PAGE[] =
	"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
	"<title>DOOM on Negentropy</title>"
	"<style>html,body{margin:0;height:100%;background:#000;color:#8a8a8a;font:13px ui-monospace,monospace}"
	"body{display:flex;flex-direction:column;align-items:center;justify-content:center}"
	"canvas{image-rendering:pixelated;width:min(100vw,calc((100vh - 48px)*4/3));aspect-ratio:4/3;background:#111}"
	"p{margin:8px 16px;text-align:center}</style>"
	"<canvas id=c width=320 height=200></canvas><p id=s>connecting...</p>"
	"<script>"
	"const c=document.getElementById('c'),x=c.getContext('2d'),s=document.getElementById('s');"
	"let img=null,ws=null,n=0,t=performance.now();"
	"const help='running on the router | arrows/WASD move, Space/Ctrl fire, E use, Shift run, 1-7 weapons, Esc menu';"
	"function go(){ws=new WebSocket('ws://'+location.host+'/ws');ws.binaryType='arraybuffer';"
	"ws.onopen=()=>s.textContent=help;"
	"ws.onclose=()=>{s.textContent='disconnected, retrying...';setTimeout(go,1000)};"
	"ws.onmessage=e=>{const h=new Uint16Array(e.data,0,2),w=h[0],ht=h[1],px=new Uint16Array(e.data,4,w*ht);"
	"if(!img||img.width!=w||img.height!=ht){c.width=w;c.height=ht;img=x.createImageData(w,ht)}"
	"const d=img.data;for(let i=0,o=0;i<px.length;i++,o+=4){const v=px[i];"
	"d[o]=(v>>11)<<3;d[o+1]=((v>>5)&63)<<2;d[o+2]=(v&31)<<3;d[o+3]=255}"
	"x.putImageData(img,0,0);n++;const now=performance.now();"
	"if(now-t>1000){s.textContent=help+' | '+Math.round(n*1000/(now-t))+' fps';n=0;t=now}}}"
	"const K={ArrowUp:0xad,ArrowDown:0xaf,ArrowLeft:0xac,ArrowRight:0xae,KeyW:0xad,KeyS:0xaf,KeyA:44,KeyD:46,"
	"Space:32,ControlLeft:32,ControlRight:32,ShiftLeft:93,ShiftRight:93,Escape:27,Enter:13,Tab:9,Backspace:127};"
	"function key(e){if(e.code in K)return K[e.code];if(e.key.length!=1)return 0;const k=e.key.toLowerCase().charCodeAt(0);return k<128?k:0}"
	"const down=new Set();"
	"function send(p,k){if(ws&&ws.readyState==1)ws.send(new Uint8Array([p,k]))}"
	"addEventListener('keydown',e=>{const k=key(e);if(!k)return;e.preventDefault();if(down.has(k))return;down.add(k);send(1,k)});"
	"addEventListener('keyup',e=>{const k=key(e);if(!k)return;e.preventDefault();if(down.delete(k))send(0,k)});"
	"addEventListener('blur',()=>{for(const k of down)send(0,k);down.clear()});"
	"go();"
	"</script>";

#define MAXPEND 8 /* browsers open idle preconnects next to the real requests */

static int lfd = -1, ws = -1; /* listener, WebSocket player */
static struct pending {
	int fd;
	uint32_t since;
	size_t len;
	char buf[4096];
} pend[MAXPEND];
static unsigned char inbuf[1024];
static size_t inlen;
static unsigned char *out;
static size_t outlen, outoff;
static unsigned char keyq[256][2];
static unsigned char kq_head, kq_tail; /* wraps at 256 */
static struct timespec t0;

static void set_nonblock(int fd, int on)
{
	int fl = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
}

static int write_all(int fd, const void *buf, size_t len)
{
	const char *p = buf;
	while (len) {
		ssize_t n = write(fd, p, len);
		if (n <= 0)
			return -1;
		p += n;
		len -= n;
	}
	return 0;
}

static void base64(const unsigned char *in, size_t len, char *outs)
{
	static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i;
	for (i = 0; i + 2 < len; i += 3) {
		*outs++ = t[in[i] >> 2];
		*outs++ = t[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
		*outs++ = t[((in[i + 1] & 15) << 2) | (in[i + 2] >> 6)];
		*outs++ = t[in[i + 2] & 63];
	}
	if (len - i == 2) {
		*outs++ = t[in[i] >> 2];
		*outs++ = t[((in[i] & 3) << 4) | (in[i + 1] >> 4)];
		*outs++ = t[(in[i + 1] & 15) << 2];
		*outs++ = '=';
	} else if (len - i == 1) {
		*outs++ = t[in[i] >> 2];
		*outs++ = t[(in[i] & 3) << 4];
		*outs++ = '=';
		*outs++ = '=';
	}
	*outs = '\0';
}

static void drop_player(void)
{
	if (ws >= 0)
		close(ws);
	ws = -1;
	inlen = outlen = outoff = 0;
}

/* HTTP header names are case-insensitive; returns the value after "name:" or NULL. */
static char *header(char *req, const char *name)
{
	size_t l = strlen(name);
	char *p;
	for (p = strchr(req, '\n'); p; p = strchr(p + 1, '\n'))
		if (strncasecmp(p + 1, name, l) == 0 && p[1 + l] == ':') {
			p += 2 + l;
			while (*p == ' ')
				p++;
			return p;
		}
	return NULL;
}

/* A full HTTP request arrived: upgrade it to the WebSocket, or send the page. */
static void handle_request(struct pending *r)
{
	int hfd = r->fd;
	char *k = header(r->buf, "Sec-WebSocket-Key");
	r->fd = -1;
	set_nonblock(hfd, 0);
	if (strncmp(r->buf, "GET /ws", 7) == 0 && k) {
		char key[128], acc[32], resp[256];
		size_t i = 0;
		sha1_context_t ctx;
		sha1_digest_t dig;

		while (k[i] && k[i] != '\r' && i < 100) {
			key[i] = k[i];
			i++;
		}
		key[i] = '\0';
		strcat(key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, (byte *)key, strlen(key));
		SHA1_Final(dig, &ctx);
		base64(dig, 20, acc);
		snprintf(resp, sizeof resp,
			"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n\r\n", acc);
		if (write_all(hfd, resp, strlen(resp)) == 0) {
			int one = 1;
			drop_player(); /* newest browser tab takes over */
			ws = hfd;
			setsockopt(ws, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
			set_nonblock(ws, 1);
			return;
		}
	} else {
		char hdr[160];
		snprintf(hdr, sizeof hdr,
			"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\n"
			"Cache-Control: no-store\r\nConnection: close\r\n\r\n", sizeof PAGE - 1);
		if (write_all(hfd, hdr, strlen(hdr)) == 0)
			write_all(hfd, PAGE, sizeof PAGE - 1);
	}
	close(hfd);
}

static void flush_out(void)
{
	while (ws >= 0 && outoff < outlen) {
		ssize_t n = write(ws, out + outoff, outlen - outoff);
		if (n > 0) {
			outoff += n;
		} else {
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
				return;
			drop_player();
		}
	}
}

/* Parse client frames: masked, short. Anything else ends the session. */
static void read_player(void)
{
	ssize_t n;
	if (ws < 0)
		return;
	n = read(ws, inbuf + inlen, sizeof inbuf - inlen);
	if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
		drop_player();
		return;
	}
	if (n > 0)
		inlen += n;
	while (inlen >= 2) {
		unsigned op = inbuf[0] & 0x0f, len = inbuf[1] & 0x7f, i;
		unsigned char *mask = inbuf + 2, *p = inbuf + 6;
		if (!(inbuf[1] & 0x80) || len >= 126 || op == 8) {
			drop_player();
			return;
		}
		if (inlen < 6 + len)
			return;
		for (i = 0; i < len; i++)
			p[i] ^= mask[i & 3];
		if (op == 2 && len == 2 && (unsigned char)(kq_tail + 1) != kq_head) {
			keyq[kq_tail][0] = p[0];
			keyq[kq_tail][1] = p[1];
			kq_tail++;
		}
		memmove(inbuf, inbuf + 6 + len, inlen - 6 - len);
		inlen -= 6 + len;
	}
}

static void pump(void)
{
	int fd, i;
	while ((fd = accept(lfd, NULL, NULL)) >= 0) {
		struct pending *r = &pend[0];
		for (i = 0; i < MAXPEND; i++) { /* free slot, else evict the oldest */
			if (pend[i].fd < 0) {
				r = &pend[i];
				break;
			}
			if (pend[i].since < r->since)
				r = &pend[i];
		}
		if (r->fd >= 0)
			close(r->fd);
		set_nonblock(fd, 1);
		r->fd = fd;
		r->len = 0;
		r->since = DG_GetTicksMs();
	}
	for (i = 0; i < MAXPEND; i++) {
		struct pending *r = &pend[i];
		ssize_t n;
		if (r->fd < 0)
			continue;
		n = read(r->fd, r->buf + r->len, sizeof r->buf - 1 - r->len);
		if (n > 0) {
			r->len += n;
			r->buf[r->len] = '\0';
			if (strstr(r->buf, "\r\n\r\n"))
				handle_request(r);
			else if (r->len >= sizeof r->buf - 1)
				n = 0; /* too big: drop below */
		}
		if (r->fd >= 0 && (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))) {
			close(r->fd);
			r->fd = -1;
		}
	}
	read_player();
	flush_out();
}

void DG_Init(void)
{
	struct sockaddr_in a = { 0 };
	int one = 1, port = 8666, i = M_CheckParmWithArgs("-port", 1);

	if (i > 0)
		port = atoi(myargv[i + 1]);
	signal(SIGPIPE, SIG_IGN);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < MAXPEND; i++)
		pend[i].fd = -1;

	lfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	a.sin_port = htons(port);
	if (lfd < 0 || bind(lfd, (struct sockaddr *)&a, sizeof a) < 0 || listen(lfd, 8) < 0) {
		perror("doom-web: cannot listen");
		exit(1);
	}
	set_nonblock(lfd, 1);

	out = malloc(10 + 4 + (size_t)DOOMGENERIC_RESX * DOOMGENERIC_RESY * 2);
	printf("doom-web: open http://<router-ip>:%d in a browser (%ux%u)\n", port, DOOMGENERIC_RESX,
		DOOMGENERIC_RESY);
	fflush(stdout);
}

void DG_DrawFrame(void)
{
	size_t npx = (size_t)DOOMGENERIC_RESX * DOOMGENERIC_RESY, len = 4 + npx * 2, i;
	unsigned char *p;

	pump();
	if (ws < 0 || outoff < outlen) /* no player, or still sending the last frame: skip this one */
		return;

	p = out;
	*p++ = 0x82; /* FIN + binary */
	*p++ = 127;
	for (i = 0; i < 8; i++)
		*p++ = (unsigned char)((uint64_t)len >> (56 - 8 * i));
	*p++ = DOOMGENERIC_RESX & 0xff;
	*p++ = DOOMGENERIC_RESX >> 8;
	*p++ = DOOMGENERIC_RESY & 0xff;
	*p++ = DOOMGENERIC_RESY >> 8;
	for (i = 0; i < npx; i++) {
		uint32_t c = DG_ScreenBuffer[i]; /* 0xAARRGGBB */
		uint16_t v = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
		*p++ = v & 0xff;
		*p++ = v >> 8;
	}
	outlen = p - out;
	outoff = 0;
	flush_out();
}

void DG_SleepMs(uint32_t ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec - t0.tv_sec) * 1000 + (ts.tv_nsec - t0.tv_nsec) / 1000000;
}

void DG_ReadInput(void)
{
	pump();
}

int DG_GetKey(int *pressed, unsigned char *key)
{
	if (kq_head == kq_tail)
		return 0;
	*pressed = keyq[kq_head][0];
	*key = keyq[kq_head][1];
	kq_head++;
	return 1;
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}
