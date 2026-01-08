/* waitsconnect - Bridge ARPANET telnet to PDP-10 console */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "imp.h"
#include "ncp.h"

/* Connection states */
enum conn_state {
    CONN_CLOSED,
    CONN_LISTENING,      /* Waiting for RTS */
    CONN_ICP_PHASE1,     /* Received RTS on listen socket, sent STR size 32, waiting for ALL */
    CONN_ICP_PHASE2,     /* Sent socket number, sent STR+RTS for data, waiting for client STR+RTS */
    CONN_ESTABLISHED,    /* Connection active, console connected */
    CONN_CLOSING         /* Closing connection */
};

/* Telnet protocol type */
enum telnet_proto {
    PROTO_OLD,  /* Socket 1 - old ARPANET telnet */
    PROTO_NEW   /* Socket 23 - RFC 854 telnet */
};

/* Connection structure */
struct connection {
    enum conn_state state;
    enum telnet_proto protocol;

    /* Remote host information */
    uint8_t remote_host;

    /* ICP phase 1 (on listen socket) */
    uint32_t listen_socket;       /* 1 or 23 */
    uint32_t icp_remote_socket;   /* Client's socket */
    uint8_t icp_link;             /* Link for ICP */

    /* Data connection (phase 2 - new sockets) */
    uint32_t data_socket;         /* Our allocated socket for data (e.g., 100) */

    /* Data connection - receive path (client sends to us) */
    uint32_t data_recv_local;     /* Our receive socket (data_socket+1, e.g., 101) */
    uint32_t data_recv_remote;    /* Client's send socket */
    uint8_t data_recv_link;       /* Link client sends on */

    /* Data connection - send path (we send to client) */
    uint32_t data_send_local;     /* Our send socket (data_socket, e.g., 100) */
    uint32_t data_send_remote;    /* Client's receive socket */
    uint8_t data_send_link;       /* Link we send on */

    /* Phase 2 handshake tracking */
    int got_str;                  /* Received STR from client */
    int got_rts;                  /* Received RTS from client */

    /* Flow control */
    int send_allocation;          /* Messages we can send */
    unsigned long last_all_time;  /* Time of last ALL sent */

    /* Output buffering */
    uint8_t output_buffer[8000];  /* Buffer for console output */
    int output_buffer_len;        /* Bytes in buffer */

    /* Console connection */
    int console_fd;
    unsigned long console_close_time;  /* Time to close console (0 = not closing) */
    unsigned long console_login_time;  /* Time to send login (0 = already sent) */

    /* IAC processing state for new telnet */
    int iac_state;
    uint8_t iac_cmd;
};

/* Global state */
static struct connection conn;
static unsigned long time_tick = 0;
static const char *console_host = "127.0.0.1";
static int console_port = 1025;
static uint32_t next_data_socket = 100;  /* Allocate sockets starting from 100 */

/* Forward declarations */
static void handle_imp(void);
static void handle_console_input(void);
static void flush_output_buffer(void);
static void send_rrp(uint8_t host);
static void send_rts(uint8_t host, uint32_t lsock, uint32_t rsock, uint8_t link);
static void send_str(uint8_t host, uint32_t lsock, uint32_t rsock, uint8_t size);
static void send_cls(uint8_t host, uint32_t lsock, uint32_t rsock);
static void send_all(uint8_t host, uint8_t link, uint16_t messages, uint32_t bits);
static void send_data(uint8_t host, uint8_t link, uint8_t *data, int len);
static void send_socket_number(uint8_t host, uint8_t link, uint32_t socket);
static int connect_to_console(void);
static void disconnect_console(void);
static void process_old_telnet(uint8_t *data, int len);
static void process_new_telnet(uint8_t *data, int len);

/* Utility: extract 32-bit socket from NCP message */
static uint32_t extract_socket(uint8_t *data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

/* Utility: insert 32-bit socket into NCP message */
static void insert_socket(uint8_t *data, uint32_t sock) {
    data[0] = sock >> 24;
    data[1] = sock >> 16;
    data[2] = sock >> 8;
    data[3] = sock;
}

/* Send IMP NOP message */
static void send_nop(void) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_NOP;
    packet[13] = 0;   /* Destination 0 */
    packet[14] = 0;
    packet[15] = 0;

    imp_send_message(packet, 2);
    fprintf(stderr, "WAITSCONNECT: Sent NOP\n");
}

/* Send NCP RRP (reset reply) message */
static void send_rrp(uint8_t host) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = 0;   /* Control link */
    packet[15] = 0;

    /* NCP header + message */
    packet[16] = 0;
    packet[17] = 8;   /* Byte size */
    packet[18] = 0;   /* Count high */
    packet[19] = 1;   /* Count low: 1 byte */
    packet[20] = 0;
    packet[21] = NCP_RRP;  /* NCP opcode */

    imp_send_message(packet, 5);  /* (1 + 9 + 1) / 2 = 5 words */
    fprintf(stderr, "WAITSCONNECT: Sent RRP to host %03o\n", host);
}

/* Send NCP RTS (Request To Send) message */
static void send_rts(uint8_t host, uint32_t lsock, uint32_t rsock, uint8_t link) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = 0;   /* Control link */
    packet[15] = 0;

    /* NCP header + message */
    packet[16] = 0;
    packet[17] = 8;   /* Byte size */
    packet[18] = 0;   /* Count high */
    packet[19] = 10;  /* Count low: 10 bytes */
    packet[20] = 0;
    packet[21] = NCP_RTS;  /* NCP opcode */
    insert_socket(&packet[22], lsock);  /* Local socket */
    insert_socket(&packet[26], rsock);  /* Remote socket */
    packet[30] = link;

    imp_send_message(packet, 10);  /* (10 + 9 + 1) / 2 = 10 words */
    fprintf(stderr, "WAITSCONNECT: Sent RTS to host %03o sockets %u:%u link %u\n",
            host, lsock, rsock, link);
}

/* Send NCP STR (Sender To Receiver) message */
static void send_str(uint8_t host, uint32_t lsock, uint32_t rsock, uint8_t size) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = 0;   /* Control link */
    packet[15] = 0;

    /* NCP header + message */
    packet[16] = 0;
    packet[17] = 8;   /* Byte size */
    packet[18] = 0;   /* Count high */
    packet[19] = 10;  /* Count low: 10 bytes */
    packet[20] = 0;
    packet[21] = NCP_STR;  /* NCP opcode */
    insert_socket(&packet[22], lsock);  /* Local socket */
    insert_socket(&packet[26], rsock);  /* Remote socket */
    packet[30] = size;

    imp_send_message(packet, 10);  /* (10 + 9 + 1) / 2 = 10 words */
    fprintf(stderr, "WAITSCONNECT: Sent STR to host %03o sockets %u:%u size %u\n",
            host, lsock, rsock, size);
}

/* Send NCP CLS (Close) message */
static void send_cls(uint8_t host, uint32_t lsock, uint32_t rsock) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = 0;   /* Control link */
    packet[15] = 0;

    /* NCP header + message */
    packet[16] = 0;
    packet[17] = 8;   /* Byte size */
    packet[18] = 0;   /* Count high */
    packet[19] = 9;   /* Count low: 9 bytes */
    packet[20] = 0;
    packet[21] = NCP_CLS;  /* NCP opcode */
    insert_socket(&packet[22], lsock);  /* Local socket */
    insert_socket(&packet[26], rsock);  /* Remote socket */

    imp_send_message(packet, 9);  /* (9 + 9 + 1) / 2 = 9 words */
    fprintf(stderr, "WAITSCONNECT: Sent CLS to host %03o sockets %u:%u\n",
            host, lsock, rsock);
}

/* Send NCP ALL (Allocate) message */
static void send_all(uint8_t host, uint8_t link, uint16_t messages, uint32_t bits) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = 0;   /* Control link */
    packet[15] = 0;

    /* NCP header + message */
    packet[16] = 0;
    packet[17] = 8;   /* Byte size */
    packet[18] = 0;   /* Count high */
    packet[19] = 8;   /* Count low: 8 bytes */
    packet[20] = 0;
    packet[21] = NCP_ALL;  /* NCP opcode */
    packet[22] = link;
    packet[23] = messages >> 8;    /* Message space (16-bit, big-endian) */
    packet[24] = messages & 0xFF;
    packet[25] = bits >> 24;       /* Bit space (32-bit, big-endian) */
    packet[26] = bits >> 16;
    packet[27] = bits >> 8;
    packet[28] = bits & 0xFF;

    fprintf(stderr, "WAITSCONNECT: Sending ALL to host %03o, link %u: %u messages, %u bits\n",
            host, link, messages, bits);

    imp_send_message(packet, 9);  /* (8 + 9 + 1) / 2 = 9 words */
    conn.last_all_time = time_tick;
}

/* Send socket number for ICP */
static void send_socket_number(uint8_t host, uint8_t link, uint32_t socket) {
    uint8_t packet[200];

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = link;  /* Data link */
    packet[15] = 0;

    /* NCP header (byte size 32, count 1) - NO opcode for data messages! */
    packet[16] = 0;
    packet[17] = 32;     /* Byte size: 32 bits */
    packet[18] = 0;
    packet[19] = 1;      /* Count: 1 (one 32-bit word) */
    packet[20] = 0;

    /* Data: 32-bit socket number (NO opcode byte before it!) */
    insert_socket(&packet[21], socket);

    imp_send_message(packet, 7);  /* (4 + 9 + 1) / 2 = 7 words */
    fprintf(stderr, "WAITSCONNECT: Sent socket %u for ICP on link %u\n", socket, link);
}

/* Send data message */
static void send_data(uint8_t host, uint8_t link, uint8_t *data, int len) {
    uint8_t packet[200];

    if (len > 100) len = 100;  /* Limit message size */

    /* IMP header */
    packet[12] = IMP_REGULAR;
    packet[13] = host;
    packet[14] = link;  /* Data link */
    packet[15] = 0;

    /* NCP header (for data on non-zero links) - NO opcode byte! */
    packet[16] = 0;
    packet[17] = 8;      /* Byte size */
    packet[18] = len >> 8;
    packet[19] = len & 0xFF;
    packet[20] = 0;

    /* Data (NO opcode byte before it!) */
    memcpy(&packet[21], data, len);

    imp_send_message(packet, 5 + (len + 1) / 2);  /* 2 (leader) + 3 (headers) + data */
    conn.send_allocation--;
}

/* Handle incoming RTS */
static void handle_rts(uint8_t source, uint8_t *data) {
    uint32_t remote_sock = extract_socket(&data[0]);
    uint32_t local_sock = extract_socket(&data[4]);
    uint8_t link = data[8];

    fprintf(stderr, "WAITSCONNECT: Received RTS from host %03o, sockets %u:%u link %u\n",
            source, remote_sock, local_sock, link);

    if (conn.state == CONN_LISTENING) {
        /* Phase 1: Initial RTS on listen socket (1 or 23) */
        if (local_sock != OLD_TELNET && local_sock != NEW_TELNET) {
            fprintf(stderr, "WAITSCONNECT: Not listening on socket %u, refusing\n", local_sock);
            send_cls(source, local_sock, remote_sock);
            return;
        }

        /* Start ICP Phase 1 */
        conn.state = CONN_ICP_PHASE1;
        conn.remote_host = source;
        conn.listen_socket = local_sock;
        conn.icp_remote_socket = remote_sock;
        conn.icp_link = link;
        conn.protocol = (local_sock == OLD_TELNET) ? PROTO_OLD : PROTO_NEW;

        /* Send STR with byte size 32 for ICP socket exchange */
        send_str(source, local_sock, remote_sock, 32);

        fprintf(stderr, "WAITSCONNECT: Started ICP phase 1, using %s protocol\n",
                conn.protocol == PROTO_OLD ? "OLD" : "NEW");

    } else if (conn.state == CONN_ICP_PHASE2) {
        /* Phase 2: RTS referencing our send socket */
        if (local_sock != conn.data_send_local) {
            fprintf(stderr, "WAITSCONNECT: RTS for unexpected socket %u (expected %u)\n",
                    local_sock, conn.data_send_local);
            return;
        }

        conn.data_send_remote = remote_sock;  /* Their receive socket */
        conn.data_recv_link = link;           /* Link they send on */
        conn.got_rts = 1;

        fprintf(stderr, "WAITSCONNECT: Received RTS for data connection, link %u\n", link);

        /* If we have both STR and RTS, establish connection */
        if (conn.got_str && conn.got_rts) {
            conn.state = CONN_ESTABLISHED;

            /* Connect to console */
            conn.console_fd = connect_to_console();
            if (conn.console_fd < 0) {
                fprintf(stderr, "WAITSCONNECT: Failed to connect to console, closing\n");
                send_cls(conn.remote_host, conn.data_send_local, conn.data_send_remote);
                send_cls(conn.remote_host, conn.data_recv_local, conn.data_recv_remote);
                conn.state = CONN_LISTENING;
                return;
            }

            /* Start login delay - discard stale data for 1 second before sending login */
            conn.console_login_time = time_tick + 1;
            fprintf(stderr, "WAITSCONNECT: Connection established, discarding console data for 1 second\n");
        }
    }
}

/* Handle incoming STR */
static void handle_str(uint8_t source, uint8_t *data) {
    uint32_t remote_sock = extract_socket(&data[0]);
    uint32_t local_sock = extract_socket(&data[4]);
    uint8_t size = data[8];

    fprintf(stderr, "WAITSCONNECT: Received STR from host %03o, sockets %u:%u size %u\n",
            source, remote_sock, local_sock, size);

    if (conn.state == CONN_ICP_PHASE2) {
        /* Expecting STR to our receive socket */
        if (local_sock != conn.data_recv_local) {
            fprintf(stderr, "WAITSCONNECT: STR for unexpected socket %u (expected %u)\n",
                    local_sock, conn.data_recv_local);
            return;
        }

        conn.data_recv_remote = remote_sock;  /* Their send socket */
        conn.got_str = 1;

        fprintf(stderr, "WAITSCONNECT: Received STR for data connection\n");

        /* If we have both STR and RTS, establish connection */
        if (conn.got_str && conn.got_rts) {
            conn.state = CONN_ESTABLISHED;

            /* Connect to console */
            conn.console_fd = connect_to_console();
            if (conn.console_fd < 0) {
                fprintf(stderr, "WAITSCONNECT: Failed to connect to console, closing\n");
                send_cls(conn.remote_host, conn.data_send_local, conn.data_send_remote);
                send_cls(conn.remote_host, conn.data_recv_local, conn.data_recv_remote);
                conn.state = CONN_LISTENING;
                return;
            }

            /* Start login delay - discard stale data for 1 second before sending login */
            conn.console_login_time = time_tick + 1;
            fprintf(stderr, "WAITSCONNECT: Connection established, discarding console data for 1 second\n");
        }
    }
}

/* Handle incoming CLS */
static void handle_cls(uint8_t source, uint8_t *data) {
    uint32_t remote_sock = extract_socket(&data[0]);
    uint32_t local_sock = extract_socket(&data[4]);

    fprintf(stderr, "WAITSCONNECT: Received CLS from host %03o, sockets %u:%u\n",
            source, remote_sock, local_sock);

    if (conn.state == CONN_CLOSED || conn.state == CONN_LISTENING)
        return;

    /* If this is ICP phase 1 closing, that's expected */
    if (conn.state == CONN_ICP_PHASE2 && local_sock == conn.listen_socket) {
        fprintf(stderr, "WAITSCONNECT: ICP connection closed as expected\n");
        return;
    }

    /* Otherwise, connection is closing */
    if (conn.console_fd >= 0) {
        fprintf(stderr, "WAITSCONNECT: Sending logout to console\n");
//sleep(1);
	write(conn.console_fd, "logout\r\n", 8);
        conn.console_close_time = time_tick + 3;  /* Close after 3 seconds */
        fprintf(stderr, "WAITSCONNECT: Console will close in 3 seconds\n");
sleep(1);
    }

    /* Reply with CLS on data sockets if established */
    if (conn.state == CONN_ESTABLISHED || conn.state == CONN_ICP_PHASE2) {
        send_cls(source, conn.data_send_local, conn.data_send_remote);
        send_cls(source, conn.data_recv_local, conn.data_recv_remote);
    }

    /* Return to listening */
    conn.state = CONN_LISTENING;
    fprintf(stderr, "WAITSCONNECT: Connection closed, ready for new connection\n");
}

/* Handle incoming ALL */
static void handle_all(uint8_t source, uint8_t *data) {
    uint8_t link = data[0];
    uint16_t messages = (data[1] << 8) | data[2];  /* 16-bit */
    uint32_t bits = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];  /* 32-bit */

    fprintf(stderr, "WAITSCONNECT: Received ALL from host %03o, link %u: %u messages, %u bits\n",
            source, link, messages, bits);

    if (conn.state == CONN_ICP_PHASE1) {
        /* This is the ALL for the ICP link - time to send socket number */
        if (link != conn.icp_link) {
            fprintf(stderr, "WAITSCONNECT: ALL for wrong link (expected %u)\n", conn.icp_link);
            return;
        }

        /* Allocate socket for data connection */
        conn.data_socket = next_data_socket;
        next_data_socket += 2;

        /* Set up data connection sockets - client sends TO socket, receives FROM socket+1 */
        conn.data_recv_local = conn.data_socket;      /* e.g., 100 - client sends here */
        conn.data_send_local = conn.data_socket + 1;  /* e.g., 101 - we send from here */
        conn.data_send_link = 45;  /* Our chosen send link */
        conn.got_str = 0;
        conn.got_rts = 0;

        /* Send socket number to client */
        send_socket_number(source, conn.icp_link, conn.data_socket);

        /* Close ICP connection */
        send_cls(source, conn.listen_socket, conn.icp_remote_socket);

        /* Send STR for our send path (we send from data_send_local to client ICP+2) */
        send_str(source, conn.data_send_local, conn.icp_remote_socket + 2, 8);

        /* Send RTS for our receive path (we receive on data_recv_local from client ICP+3) */
        send_rts(source, conn.data_recv_local, conn.icp_remote_socket + 3, conn.data_send_link);

        conn.state = CONN_ICP_PHASE2;
        fprintf(stderr, "WAITSCONNECT: ICP phase 2 started, sent socket %u\n", conn.data_socket);

    } else if (conn.state == CONN_ESTABLISHED) {
        /* This is ALL for our send link on data connection */
        if (link != conn.data_send_link) {
            fprintf(stderr, "WAITSCONNECT: ALL for wrong link (expected %u)\n", conn.data_send_link);
            return;
        }

        conn.send_allocation += messages;
        fprintf(stderr, "WAITSCONNECT: Send allocation now %d\n", conn.send_allocation);

        /* Try to flush any buffered output */
        flush_output_buffer();
    }
}

/* Handle incoming data */
static void handle_data(uint8_t source, uint8_t link, uint8_t *data, int len) {
    if (conn.state != CONN_ESTABLISHED)
        return;

    if (link != conn.data_recv_link)
        return;

    fprintf(stderr, "WAITSCONNECT: Received %d bytes from host %03o\n", len, source);

    /* Process according to protocol */
    if (conn.protocol == PROTO_OLD) {
        process_old_telnet(data, len);
    } else {
        process_new_telnet(data, len);
    }

    /* Grant client more send permission after receiving data */
    send_all(conn.remote_host, conn.data_recv_link, 10, 16000);
}

/* Process NCP control messages */
static void process_ncp(uint8_t source, uint8_t *data, int count) {
    int i = 0;

    while (i < count) {
        uint8_t opcode = data[i++];

        switch (opcode) {
        case NCP_NOP:
            /* No operation - ignore */
            break;
        case NCP_RTS:
            handle_rts(source, &data[i]);
            i += 9;
            break;
        case NCP_STR:
            handle_str(source, &data[i]);
            i += 9;
            break;
        case NCP_CLS:
            handle_cls(source, &data[i]);
            i += 8;
            break;
        case NCP_ALL:
            handle_all(source, &data[i]);
            i += 7;  /* 1 link + 2 messages + 4 bits = 7 bytes */
            break;
        case NCP_RST:
            fprintf(stderr, "WAITSCONNECT: Received RST from host %03o\n", source);
            send_rrp(source);
            break;
        case NCP_RRP:
            fprintf(stderr, "WAITSCONNECT: Received RRP from host %03o\n", source);
            break;
        case NCP_ECO:
            fprintf(stderr, "WAITSCONNECT: Received ECO from host %03o\n", source);
            /* Could send ERP reply */
            break;
        case NCP_ERR:
            fprintf(stderr, "WAITSCONNECT: Received ERR from host %03o\n", source);
            /* ERR format: error code (1 byte) + error data (10 bytes) */
            if (i < count) {
                fprintf(stderr, "WAITSCONNECT: Error code: %u\n", data[i]);
            }
            /* Skip rest of error message */
            return;
        default:
            fprintf(stderr, "WAITSCONNECT: Unknown NCP opcode %u\n", opcode);
            return;
        }
    }
}

/* Process IMP message */
static void handle_imp(void) {
    uint8_t packet[200];
    int length;

    imp_receive_message(packet, &length);

    if (length == 0)
        return;

    uint8_t type = packet[0] & 0x0F;
    uint8_t source = packet[1];
    uint8_t link = packet[2];

    switch (type) {
    case IMP_REGULAR:
        if (link == 0) {
            /* Control message - NCP protocol */
            uint16_t count = (packet[6] << 8) | packet[7];
            process_ncp(source, &packet[9], count);
        } else {
            /* Data message */
            uint16_t count = (packet[6] << 8) | packet[7];
            handle_data(source, link, &packet[9], count);
        }
        break;
    case IMP_RFNM:
        /* Ready For Next Message - can send more */
        break;
    case IMP_RESET:
        fprintf(stderr, "WAITSCONNECT: IMP reset received\n");
        /* Send NOPs in response */
        send_nop();
        sleep(1);
        send_nop();
        sleep(1);
        send_nop();
        break;
    default:
        fprintf(stderr, "WAITSCONNECT: IMP message type %u\n", type);
        break;
    }
}

/* Process old telnet protocol */
static void process_old_telnet(uint8_t *data, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];

        switch (byte) {
        case 0:  /* NUL - ignore */
            break;
        case 015:  /* CR */
            /* Look ahead for NUL or LF */
            if (i+1 < len) {
                if (data[i+1] == 0) {
                    write(conn.console_fd, "\r", 1);
                    i++;
                } else if (data[i+1] == 012) {
                    write(conn.console_fd, "\r\n", 2);
                    i++;
                } else {
                    write(conn.console_fd, "\r\n", 2);
                }
            } else {
                write(conn.console_fd, "\r\n", 2);
            }
            break;
        case OMARK:
        case OBREAK:
        case ONOP:
            fprintf(stderr, "WAITSCONNECT: Old telnet command %03o\n", byte);
            break;
        case ONOECHO:
            fprintf(stderr, "WAITSCONNECT: NOECHO requested\n");
            break;
        case OECHO:
            fprintf(stderr, "WAITSCONNECT: ECHO requested\n");
            break;
        case OHIDE:
            fprintf(stderr, "WAITSCONNECT: HIDE requested\n");
            break;
        default:
            /* Regular character */
            if (byte < 0200) {  /* Only 7-bit ASCII */
                write(conn.console_fd, &byte, 1);
            }
            break;
        }
    }
}

/* Process new telnet protocol */
static void process_new_telnet(uint8_t *data, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];

        if (conn.iac_state == 0) {
            /* Normal data mode */
            if (byte == IAC) {
                conn.iac_state = 1;  /* Expecting command */
            } else {
                write(conn.console_fd, &byte, 1);
            }
        } else if (conn.iac_state == 1) {
            /* Got IAC, expecting command */
            if (byte == IAC) {
                /* Escaped IAC - literal 0377 */
                write(conn.console_fd, &byte, 1);
                conn.iac_state = 0;
            } else if (byte == DO || byte == DONT || byte == WILL || byte == WONT) {
                /* Option negotiation - need one more byte */
                conn.iac_cmd = byte;
                conn.iac_state = 2;
            } else if (byte == EC) {
                /* Erase character */
                write(conn.console_fd, "\b \b", 3);
                conn.iac_state = 0;
            } else {
                /* Other IAC commands - ignore for now */
                fprintf(stderr, "WAITSCONNECT: IAC command %03o\n", byte);
                conn.iac_state = 0;
            }
        } else if (conn.iac_state == 2) {
            /* Got DO/DONT/WILL/WONT, this is the option */
            fprintf(stderr, "WAITSCONNECT: Telnet negotiation: %03o %03o\n",
                    conn.iac_cmd, byte);
            conn.iac_state = 0;
        }
    }
}

/* Flush output buffer to ARPANET */
static void flush_output_buffer(void) {
    while (conn.output_buffer_len > 0 && conn.send_allocation > 0) {
        int to_send = conn.output_buffer_len;
        if (to_send > 100) to_send = 100;  /* Max message size */

        send_data(conn.remote_host, conn.data_send_link, conn.output_buffer, to_send);

        /* Remove sent data from buffer */
        conn.output_buffer_len -= to_send;
        if (conn.output_buffer_len > 0) {
            memmove(conn.output_buffer, conn.output_buffer + to_send, conn.output_buffer_len);
        }
    }
}

/* Handle console input */
static void handle_console_input(void) {
    uint8_t buffer[100];
    int n = read(conn.console_fd, buffer, sizeof(buffer));

    /* If we're in closing delay state, discard all data and ignore EOF */
    if (conn.console_close_time > 0) {
        if (n <= 0) {
            fprintf(stderr, "WAITSCONNECT: Console disconnected during logout delay (ignoring)\n");
        } else {
            fprintf(stderr, "WAITSCONNECT: Discarding %d bytes from console during logout delay\n", n);
        }
        return;  /* Don't close, don't send - just wait for timer */
    }

    /* If we're in login delay state, discard all data and ignore EOF */
    if (conn.console_login_time > 0) {
        if (n <= 0) {
            fprintf(stderr, "WAITSCONNECT: Console disconnected during login delay (ignoring)\n");
        } else {
            fprintf(stderr, "WAITSCONNECT: Discarding %d bytes of stale console data during login delay\n", n);
        }
        return;  /* Don't close, don't send - just wait for timer */
    }

    /* Normal operation - not in any delay */
    if (n <= 0) {
        fprintf(stderr, "WAITSCONNECT: Console disconnected\n");
        disconnect_console();

        /* Close ARPANET connection */
        send_cls(conn.remote_host, conn.data_send_local, conn.data_send_remote);
        send_cls(conn.remote_host, conn.data_recv_local, conn.data_recv_remote);
        conn.state = CONN_LISTENING;
        return;
    }

    /* Add to output buffer */
    if (conn.output_buffer_len + n <= sizeof(conn.output_buffer)) {
        memcpy(conn.output_buffer + conn.output_buffer_len, buffer, n);
        conn.output_buffer_len += n;
    } else {
        fprintf(stderr, "WAITSCONNECT: Output buffer full, dropping %d bytes\n", n);
    }

    /* Try to flush buffer */
    flush_output_buffer();
}

/* Connect to console */
static int connect_to_console(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "WAITSCONNECT: socket() failed: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(console_port)
    };

    if (inet_pton(AF_INET, console_host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "WAITSCONNECT: Invalid console address\n");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "WAITSCONNECT: Cannot connect to console at %s:%d: %s\n",
                console_host, console_port, strerror(errno));
        close(fd);
        return -1;
    }

    fprintf(stderr, "WAITSCONNECT: Connected to console at %s:%d\n",
            console_host, console_port);
    return fd;
}

/* Disconnect from console */
static void disconnect_console(void) {
    if (conn.console_fd >= 0) {
        close(conn.console_fd);
        conn.console_fd = -1;
    }
}

/* Periodic tasks */
static void periodic_tasks(void) {
    /* Check if we need to send login after initial delay */
    if (conn.console_login_time > 0 && time_tick >= conn.console_login_time) {
        fprintf(stderr, "WAITSCONNECT: Sending login to console\n");
        write(conn.console_fd, "login\r", 6);
//write(conn.console_fd, "\003\rlogin\r", 8);

        /* Grant client send permission */
        send_all(conn.remote_host, conn.data_recv_link, 10, 16000);

        conn.console_login_time = 0;
        fprintf(stderr, "WAITSCONNECT: Login sent, connection fully established\n");
    }

    /* Check if we need to close the console after logout delay */
    if (conn.console_close_time > 0 && time_tick >= conn.console_close_time) {
        fprintf(stderr, "WAITSCONNECT: Closing console after logout delay\n");
        disconnect_console();
        conn.console_close_time = 0;
    }
}

/* Signal handler */
static void cleanup(int sig) {
    fprintf(stderr, "\nWAITSCONNECT: Shutting down\n");
    disconnect_console();
    exit(0);
}

/* Main */
int main(int argc, char **argv) {
    fprintf(stderr, "WAITSCONNECT: PDP-10 ARPANET Console Bridge\n");
    fprintf(stderr, "WAITSCONNECT: Host 11, Console at %s:%d\n",
            console_host, console_port);

    /* Set up signal handling */
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    /* Initialize IMP connection (hardcoded) */
    char *imp_argv[] = { "waitsconnect", "localhost", "20111", "20112" };
    imp_init(4, imp_argv);
    imp_host_ready(1);

    /* Initialize connection state */
    memset(&conn, 0, sizeof(conn));
    conn.state = CONN_LISTENING;
    conn.console_fd = -1;

    /* Send NOPs to announce ourselves */
    sleep(1);
    send_nop();
    sleep(1);
    send_nop();
    sleep(1);
    send_nop();

    fprintf(stderr, "WAITSCONNECT: Listening on sockets 1 (old) and 23 (new telnet)\n");

    /* Main event loop */
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);

        /* Monitor IMP */
        imp_fd_set(&rfds);
        int maxfd = 10;  /* IMP socket */

        /* Monitor console if connected */
        if (conn.console_fd >= 0) {
            FD_SET(conn.console_fd, &rfds);
            if (conn.console_fd > maxfd)
                maxfd = conn.console_fd;
        }

        /* 1 second timeout */
        struct timeval timeout = {1, 0};
        int n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "WAITSCONNECT: select() error: %s\n", strerror(errno));
            break;
        }

        /* Process IMP messages */
        if (imp_fd_isset(&rfds)) {
            handle_imp();
        }

        /* Process console input */
        if (conn.console_fd >= 0 && FD_ISSET(conn.console_fd, &rfds)) {
            handle_console_input();
        }

        /* Periodic tasks */
        time_tick++;
        periodic_tasks();
    }

    cleanup(0);
    return 0;
}
