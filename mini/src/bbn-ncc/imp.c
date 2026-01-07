/* Interface between NCP and IMP. */

#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "imp.h"

#define FLAG_LAST    0001
#define FLAG_READY   0002

static int imp_sock;
static int port;
static struct sockaddr_in destination;
static uint16_t imp_ready = 0;
static uint16_t imp_flags = 0;
static uint32_t rx_sequence, tx_sequence;

static const char *type_name[] =
{
  "REGULAR",  // 0
  "ER_LEAD",  // 1
  "DOWN",     // 2
  "BLOCKED",  // 3
  "NOP",      // 4
  "RFNM",     // 5
  "FULL",     // 6
  "DEAD",     // 7
  "ER_DATA",  // 8
  "INCOMPL",  // 9
  "RESET",    //10
  "???",      //11
  "???",      //12
  "???",      //13
  "???",      //14
  "NEW",      //15
};

static void fatal (const char *message)
{
  fprintf (stderr, "Fatal error: %s\n", message);
  exit (1);
}

void imp_host_ready (int flag)
{
  static uint8_t data[12];
  if (flag && (imp_flags & FLAG_READY) == 0) {
    imp_flags |= FLAG_READY;
    imp_send_message (data, 0);
  } else if (!flag && (imp_flags & FLAG_READY) != 0) {
    imp_flags &= ~FLAG_READY;
    imp_send_message (data, 0);
  }
}

static void args (int argc, char **argv)
{
  struct hostent *h;

  /* Hard-coded for IMP #5, host 0: localhost ports 20051/20052 */
  h = gethostbyname ("localhost");
  if (h == NULL)
    fatal ("gethostbyname");

  destination.sin_family = AF_INET;
  destination.sin_port = htons (20051);  /* IMP listening port */
  memcpy (&destination.sin_addr, h->h_addr, h->h_length);

  port = htons (20052);  /* Our listening port */

  fprintf (stderr, "IMP: Connecting to localhost:20051, listening on port 20052\n");
}

static void make_socket (void)
{
  struct sockaddr_in source;
  int enable = 1;

  imp_sock = socket (AF_INET, SOCK_DGRAM, 0);
  if (imp_sock == -1)
    fatal ("socket");

  if (setsockopt(imp_sock, SOL_SOCKET, SO_REUSEADDR,
                 &enable, sizeof enable) != 0)
    fatal ("setsockopt(SO_REUSEADDR)");

  source.sin_family = AF_INET;
  source.sin_addr.s_addr = INADDR_ANY;
  source.sin_port = port;
  if (bind (imp_sock, (struct sockaddr *)&source, sizeof source) == -1)
    fatal ("bind");
}

void imp_send_message (uint8_t *data, int length)
{
  int r;

  data[0] = 'H';
  data[1] = '3';
  data[2] = '1';
  data[3] = '6';
  data[4] = tx_sequence >> 24;
  data[5] = tx_sequence >> 16;
  data[6] = tx_sequence >> 8;
  data[7] = tx_sequence;
  data[8] = ++length >> 8;
  data[9] = length;
  data[10] = imp_flags >> 8;
  data[11] = imp_flags | FLAG_LAST;

  r = sendto (imp_sock, data, 2 * length + 10, 0,
              (struct sockaddr *)&destination, sizeof destination);
  if (r == -1) {
    fprintf (stderr, "IMP: Send error: %s\n", strerror (errno));
  }

  if (length == 1 && tx_sequence == 0)
    fprintf (stderr, "IMP: Sent host ready bit (ready=%d)\n",
             (imp_flags & FLAG_READY) ? 1 : 0);
  tx_sequence++;
}

static void ready_nop (int flag)
{
}

void (*imp_imp_ready) (int flag) = ready_nop;

static uint8_t message[200];

void imp_receive_message (uint8_t *data, int *length)
{
  uint32_t x;
  int n;

  *length = 0;

 loop:
  n = read (imp_sock, message, sizeof message);
  if (n == 0)
    return;
  else if (n == -1) {
    fprintf (stderr, "IMP: Receive error: %s\n", strerror (errno));
    return;
  }

  if (message[0] != 'H' ||
      message[1] != '3' ||
      message[2] != '1' ||
      message[3] != '6') {
    int i;
    fprintf (stderr, "IMP: Receive error: bad magic.\n");
    for (i = 0; i < n; i++)
      fprintf (stderr, "%02X ", message[i]);
    return;
  }

  x = (message[4] << 24) | (message[5] << 16) | (message[6] << 8) | message[7];
  if (x == 0 && rx_sequence != 0) {
    fprintf (stderr, "IMP: Sequence number restarted.\n");
    rx_sequence = x;
  } else if (x < rx_sequence) {
    fprintf (stderr, "IMP: Bad sequence number: %u.\n", x);
    *length = 0;
    return;
  } else if (x != rx_sequence) {
    rx_sequence = x;
  }
  rx_sequence++;

  x = message[8] << 8 | message[9];
  *length += x - 1;
  if (n != 2 * x + 10)
    fprintf (stderr, "IMP: Receive bad length.\n");

  if (*length == 0)
    return;

  x = (message[10] << 8) | message[11];
  if ((x & FLAG_READY) ^ imp_ready) {
    imp_ready = x & FLAG_READY;
    if (imp_ready)
      fprintf (stderr, "IMP: Ready.\n");
    else
      fprintf (stderr, "IMP: Not ready.\n");
    imp_imp_ready (imp_ready);
  }

  memcpy (data, message + 12, n - 12);
  data += n - 12;

  if ((x & FLAG_LAST) == 0)
    goto loop;
}

void imp_fd_set (fd_set *fdset)
{
  FD_SET (imp_sock, fdset);
}

int imp_fd_isset (fd_set *fdset)
{
  return FD_ISSET (imp_sock, fdset);
}

int imp_get_fd (void)
{
  return imp_sock;
}

void imp_init (int argc, char **argv)
{
  (void)argc;  /* Unused - parameters are hard-coded */
  (void)argv;
  args (argc, argv);
  make_socket ();
  rx_sequence = tx_sequence = 0;
  imp_flags = imp_ready = 0;
}
