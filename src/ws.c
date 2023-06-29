#include "ws.h"
#include "io.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <openssl/sha.h>
#include <arpa/inet.h>

#define OPCODE(head) ((unsigned char) (head[0] & 0x0f))
#define PAYLOAD_LEN(head) ((unsigned char) (head[1] & 0x7f))

struct ws_frame {
	char head[2];
	char mk[4];
	uint64_t pl;
	char data[BUFSIZ];
};

#ifdef __OpenBSD__
int __b64_ntop(unsigned char const *src, size_t srclength,
	       char *target, size_t targsize);
#define b64_ntop(...) __b64_ntop(__VA_ARGS__)
#else

#include <stdint.h>

// https://github.com/yasuoka/base64/blob/master/b64_ntop.c

int
b64_ntop(u_char *src, size_t srclength, char *target, size_t target_size)
{
  int		 i, j, expect_siz;
  uint32_t	 bit24;
  const char	 b64str[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  expect_siz = ((srclength + 2) / 3) * 4 + 1;

  if (target == NULL)
    return (expect_siz);
  if (target_size < expect_siz)
    return (-1);

  for (i = 0, j = 0; i < srclength; i += 3) {
    bit24 = src[i] << 16;
    if (i + 1 < srclength)
      bit24 |= src[i + 1] << 8;
    if (i + 2 < srclength)
      bit24 |= src[i + 2];

    target[j++] = b64str[(bit24 & 0xfc0000) >> 18];
    target[j++] = b64str[(bit24 & 0x03f000) >> 12];
    if (i + 1 < srclength)
      target[j++] = b64str[(bit24 & 0x000fc0) >> 6];
    else
      target[j++] = '=';
    if (i + 2 < srclength)
      target[j++] = b64str[(bit24 & 0x00003f)];
    else
      target[j++] = '=';
  }
  target[j] = '\0';

  return j;
}

#endif

int
ws_handshake(int cfd, char *buf) {
	static char common_resp[]
		= "HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: upgrade\r\n"
		"Sec-Websocket-Protocol: text\r\n"
		"Sec-Websocket-Accept: 00000000000000000000000000000\r\n\r\n",
		kkey[] = "Sec-WebSocket-Key";
	unsigned char hash[SHA_DIGEST_LENGTH];
	char *s;

	// warn("ws_handshake %s", buf);
        for (s = buf; s && *s; s = strchr(s, '\n'))
                if (!strncasecmp(++s, kkey, sizeof(kkey) - 1)) {
                        SHA_CTX c;
                        char *s2;
                        s += sizeof(kkey) + 1;
                        SHA1_Init(&c);
                        s2 = strchr(s, '\r');
                        SHA1_Update(&c, s, s2 - s);
                        SHA1_Update(&c, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
                        SHA1_Final(hash, &c);
                        b64_ntop(hash, sizeof(hash), common_resp + 127, 29);
			memcpy(common_resp + 127 + 28, "\r\n\r\n", 5);
                        WRITE(cfd, common_resp, 127 + 28 + 4);
                        return 0;
                }

        return 1;
}

int
_ws_write(int cfd, const void *data, size_t n)
{
	unsigned char head[2] = { 0x81, 0x00 };
	size_t len = 2;
	int smallest = n < 126;
	char frame[2 + (smallest ? 0 : sizeof(uint64_t)) + n];

	if (smallest) {
		head[1] |= n;
		memcpy(frame, head, sizeof(head));
	} else if (n < (1 << 16)) {
		uint16_t nn = htons(n);
		head[1] |= 126;
		memcpy(frame, head, sizeof(head));
		memcpy(frame + sizeof(head), &nn, sizeof(nn));
		len = sizeof(head) + sizeof(nn);
	} else {
		uint64_t nn = htonl(n);
		head[1] |= 127;
		memcpy(frame, head, sizeof(head));
		memcpy(frame + sizeof(head), &nn, sizeof(nn));
		len = sizeof(head) + sizeof(nn);
	}

	memcpy(frame + len, data, n);
	return WRITE(cfd, frame, len + n) < n;
}

static inline void
ws_close_policy(int cfd) {
	unsigned char head[2] = { 0x88, 0x02 };
	unsigned code = 1008;

	WRITE(cfd, head, sizeof(head));
	WRITE(cfd, &code, sizeof(code));
}

int
ws_read(int cfd, char *data, size_t len)
{
	struct ws_frame frame;
	uint64_t pl;
	int i, n;

	n = READ(cfd, frame.head, sizeof(frame.head));
	if (n != sizeof(frame.head))
		goto error;

	pl = PAYLOAD_LEN(frame.head);

	if (pl == 126) {
		uint16_t rpl;
		n = READ(cfd, &rpl, sizeof(rpl));
		if (n != sizeof(rpl))
			goto error;
		pl = rpl;
	} else if (pl == 127) {
		uint64_t rpl;
		n = READ(cfd, &rpl, sizeof(rpl));
		if (n != sizeof(rpl))
			goto error;
		pl = rpl;
	}

	frame.pl = pl;

	n = READ(cfd, frame.mk, sizeof(frame.mk));

	if (n != sizeof(frame.mk))
		goto error;

	if (OPCODE(frame.head) == 8)
		return 0;

	n = READ(cfd, frame.data, pl);
	if (n != pl)
		goto error;

	for (i = 0; i < pl; i++)
		frame.data[i] ^= frame.mk[i % 4];

	frame.data[i] = '\0';
        memcpy(data, frame.data, i + 1);
	return pl;

error:	ws_close_policy(cfd);
        return -1;
}

int
ws_write(int cfd, const void *data, size_t n)
{
	const char *p = data;
	size_t max_len = BUFSIZ;
	while (n >= max_len) {
		_ws_write(cfd, p, max_len);
		n -= max_len;
		p += max_len;
	}
	if (n)
		_ws_write(cfd, p, n);
	return 0;

}

int
wsdprintf(int fd, const char *format, va_list ap)
{
#if 0
	size_t max_len = BUFFER_LEN * 10000;
#endif
	static char buf[BUFSIZ], *p = buf;
	ssize_t len, llen;
	llen = len = vsnprintf(buf, sizeof(buf), format, ap);
#if 0
	while (llen >= max_len) {
		_ws_write(fd, p, max_len);
		llen -= max_len;
		p += max_len;
	}
	if (llen)
		_ws_write(fd, p, llen);
#else
	_ws_write(fd, p, llen);
#endif
	return len;
}

int
wsprintf(int fd, const char *format, ...)
{
	ssize_t len;
	va_list args;
	va_start(args, format);
	len = wsdprintf(fd, format, args);
	va_end(args);
	return len;
}
