/* BBN Network Control Center - Passive IMP Monitor
   Listens to all IMP traffic and provides analysis */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

#include "imp.h"

#define IMP_REGULAR       0
#define IMP_LEADER_ERROR  1
#define IMP_DOWN          2
#define IMP_BLOCKED       3
#define IMP_NOP           4
#define IMP_RFNM          5
#define IMP_FULL          6
#define IMP_DEAD          7
#define IMP_DATA_ERROR    8
#define IMP_INCOMPL       9
#define IMP_RESET        10

#define NCP_NOP      0
#define NCP_RTS      1
#define NCP_STR      2
#define NCP_CLS      3
#define NCP_ALL      4
#define NCP_GVB      5
#define NCP_RET      6
#define NCP_INR      7
#define NCP_INS      8
#define NCP_ECO      9
#define NCP_ERP     10
#define NCP_ERR     11
#define NCP_RST     12
#define NCP_RRP     13

static const char *imp_type_name[] = {
  "REGULAR", "ER_LEAD", "DOWN", "BLOCKED", "NOP",
  "RFNM", "FULL", "DEAD", "ER_DATA", "INCOMPL", "RESET"
};

static const char *ncp_type_name[] = {
  "NOP", "RTS", "STR", "CLS", "ALL", "GVB", "RET",
  "INR", "INS", "ECO", "ERP", "ERR", "RST", "RRP"
};

/* 1973 IMP Throughput Message (Type 302) */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Actual data starts at byte 8 (after 8-byte padding) */
  uint8_t counter;           /* Byte 8 - increments each report */
  uint16_t field1;           /* Bytes 10-11 - small counter */
  uint16_t pattern_0628;     /* Bytes 16-17 - always 0x0628? */
  uint16_t pattern_ffff;     /* Bytes 22-23 - always 0xFFFF? */
  uint16_t variable_field;   /* Bytes 28-29 - varies by IMP */

  uint8_t raw_data[64];      /* Store full message for analysis */
} throughput_1973_t;

/* 1973 IMP Trouble Report (Type 301) */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Fields discovered from analysis */
  uint16_t anomaly;
  uint16_t restart_reload;
  uint16_t halt_pc;
  uint16_t halt_a;
  uint16_t halt_x;
  uint16_t free_count;
  uint16_t sf_count;
  uint16_t reas_count;
  uint16_t allocate_count;
  uint16_t imp_version;
  uint16_t host34;
  uint16_t tip_version;
  uint16_t host_interface_tested;
  uint16_t test_mess_send_count;
  uint16_t test_mess_recvd_count;
  struct {
    uint16_t routing_msgs_received;
    uint16_t routing_msgs_errors;
  } modem[5];
  uint16_t line_speed;
  uint16_t trap_info[3];
  uint16_t checksum;

  uint8_t raw_data[106];     /* Store full message for analysis */
} trouble_report_t;

/* IMP Throughput Message (Type 302) - 59 words, 118 bytes */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Words 4-13: 5 modem pairs */
  struct {
    uint16_t packets_out;
    uint16_t words_out;
  } modem[5];

  /* Words 14-53: 4 host throughput blocks */
  struct {
    uint16_t mess_to_net;
    uint16_t mess_from_net;
    uint16_t packet_to_net;
    uint16_t packet_from_net;
    uint16_t local_mess_sent;
    uint16_t local_mess_rcvd;
    uint16_t local_packet_sent;
    uint16_t local_packet_rcvd;
    uint16_t words_to_net;
    uint16_t words_from_net;
  } host[4];

  /* Words 54-56: Background counts */
  uint16_t background_counts[3];

  /* Word 57: Checksum */
  uint16_t checksum;
} throughput_message_t;

static struct {
  time_t start_time;
  unsigned long total_packets;
  unsigned long regular_packets;
  unsigned long ncc_status_packets;
  unsigned long ncp_control_packets;
  unsigned long user_data_packets;
  unsigned long rfnm_packets;
  unsigned long reset_packets;
  unsigned long dead_host_packets;
  unsigned long other_packets;
  unsigned long total_bytes;

  /* Per-IMP statistics for NCC monitoring */
  struct {
    unsigned long status_reports;
    unsigned long throughput_reports;
    unsigned long keepalives;
    unsigned long large_messages;  /* Count of messages with count > 1000 */
    unsigned long unknown_messages;  /* Count of unknown message types */
    time_t last_seen;
    time_t first_seen;

    /* Decoded messages */
    throughput_message_t last_throughput;
    trouble_report_t last_trouble_report;
    throughput_1973_t last_throughput_1973;
    time_t last_status_time;
    time_t last_throughput_time;
    int has_status;    /* 1 if we've received at least one status msg */
    int has_throughput;  /* 1 if we've received at least one throughput msg */
    int is_1973_format;  /* 1 if IMP is sending 1973-era messages */

    /* Legacy fields for backwards compatibility */
    uint16_t last_metrics[64];  /* Last reported metric words (extended to hold full message) */
    int last_metric_count;
    int last_message_bytes;  /* Size of last status message in bytes */
    int min_message_bytes;   /* Smallest status message seen */
    int max_message_bytes;   /* Largest status message seen */
    unsigned long total_message_bytes;  /* Sum of all message sizes */

    int configured;  /* 1 if IMP is in topology config */
    char name[32];   /* IMP name from config */
    char msg_type[16]; /* "STATUS-304", "THRU-302", or "" */
  } imps[64];

  /* Per-host statistics */
  struct {
    unsigned long packets_from;
    time_t last_seen;
  } hosts[256];

  /* Per-NCP-type counts */
  unsigned long ncp_type_count[14];
} stats;

static uint8_t packet[200];
static int debug_mode = 0;  /* Toggle with 'd' key */

static int load_topology_config(void)
{
  const char *paths[] = {"./arpanet-topology.conf",
                         "../arpanet-topology.conf",
                         "../../arpanet-topology.conf",
                         NULL};
  FILE *f = NULL;
  char line[256];
  int in_section = 0;
  int count = 0;

  /* Try to find the config file */
  for (int i = 0; paths[i] != NULL; i++) {
    f = fopen(paths[i], "r");
    if (f != NULL) {
      fprintf(stderr, "NCC: Loaded topology from %s\n", paths[i]);
      break;
    }
  }

  if (f == NULL) {
    fprintf(stderr, "NCC: Warning - topology config not found\n");
    return 0;
  }

  /* Parse the file */
  while (fgets(line, sizeof(line), f) != NULL) {
    /* Check for section header */
    if (strstr(line, "# SECTION 1: IMP NETWORK TOPOLOGY") != NULL) {
      in_section = 1;
      continue;
    }

    /* Check for next section (end of section 1) */
    if (in_section && strstr(line, "# SECTION") != NULL) {
      break;
    }

    /* Parse IMP definitions */
    if (in_section && strncmp(line, "IMP ", 4) == 0) {
      int imp_num;
      char name[32];

      /* Format: "IMP <number> #<name>" */
      if (sscanf(line, "IMP %d #%31s", &imp_num, name) == 2) {
        if (imp_num >= 0 && imp_num < 64) {
          stats.imps[imp_num].configured = 1;
          strncpy(stats.imps[imp_num].name, name, sizeof(stats.imps[imp_num].name) - 1);
          stats.imps[imp_num].name[sizeof(stats.imps[imp_num].name) - 1] = '\0';
          count++;
        }
      }
    }
  }

  fclose(f);
  fprintf(stderr, "NCC: Configured %d IMPs from topology\n", count);
  return count;
}

static void print_timestamp(void)
{
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  unsigned long elapsed = now - stats.start_time;
  printf("[%02d:%02d:%02d +%lus] ", tm->tm_hour, tm->tm_min, tm->tm_sec, elapsed);
}

/* Helper: Extract 16-bit word from byte array (big-endian) */
static uint16_t get_word(uint8_t *data, int word_index)
{
  int byte_offset = word_index * 2;
  return (data[byte_offset] << 8) | data[byte_offset + 1];
}

/* Decode IMP Throughput Message (Type 302) - 64 words, 106 or 118 bytes */
static int decode_throughput_message(uint8_t *data, int count, throughput_message_t *msg)
{
  if (count != 106 && count != 118) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(throughput_message_t));
  int j = 0;

  /* Word 1: BCODE - verify message type 302 */
  msg->message_type = get_word(data, j++);

  if (msg->message_type != 0302) {
    return 0;  /* Not a throughput message */
  }

  /* Words 1-20: 5 modem pairs (SMOD1-5) */
  for (int i = 0; i < 5; i++) {
    msg->modem[i].packets_out = get_word(data, j++);
    msg->modem[i].words_out = get_word(data, j++);
  }

  /* Words 11-50: 4 host throughput blocks (HOST0-3) */
  for (int i = 0; i < 4; i++) {
    msg->host[i].mess_to_net = get_word(data, j++);
    msg->host[i].mess_from_net = get_word(data, j++);
    msg->host[i].packet_to_net = get_word(data, j++);
    msg->host[i].packet_from_net = get_word(data, j++);
    msg->host[i].local_mess_sent = get_word(data, j++);
    msg->host[i].local_mess_rcvd = get_word(data, j++);
    msg->host[i].local_packet_sent = get_word(data, j++);
    msg->host[i].local_packet_rcvd = get_word(data, j++);
    msg->host[i].words_to_net = get_word(data, j++);
    msg->host[i].words_from_net = get_word(data, j++);
  }

  if (count == 118) {
    /* Words 51-53: SNETIM - Background counts */
    msg->background_counts[0] = get_word(data, j++);
    msg->background_counts[1] = get_word(data, j++);
    msg->background_counts[2] = get_word(data, j++);
  }

  /* Last word is a checksum. */
  msg->checksum = get_word(data, j);

  return 1;  /* Success */
}

/* Decode 1973 IMP Trouble Report (Type 301) */
static int decode_trouble_report(uint8_t *data, int count, trouble_report_t *msg, uint8_t imp_num)
{
  if (count != 64) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(trouble_report_t));
  memcpy(msg->raw_data, data, 64);
  int j = 0;

  msg->imp_number = imp_num;
  msg->message_type = get_word(data, j++);

  msg->anomaly = get_word(data, j++);
  msg->restart_reload = get_word(data, j++);
  msg->halt_pc = get_word(data, j++);
  msg->halt_a = get_word(data, j++);
  msg->halt_x = get_word(data, j++);
  msg->free_count = get_word(data, j++);
  msg->sf_count = get_word(data, j++);
  msg->reas_count = get_word(data, j++);
  msg->allocate_count = get_word(data, j++);
  msg->imp_version = get_word(data, j++);
  msg->host34 = get_word(data, j++);
  msg->tip_version = get_word(data, j++);
  msg->host_interface_tested = get_word(data, j++);
  msg->test_mess_send_count = get_word(data, j++);
  msg->test_mess_recvd_count = get_word(data, j++);

  for (int i = 0; i < 5; i++) {
    msg->modem[i].routing_msgs_received = get_word(data, j++);
    msg->modem[i].routing_msgs_errors = get_word(data, j++);
  }

  msg->line_speed = get_word(data, j++);
  msg->trap_info[0] = get_word(data, j++);
  msg->trap_info[1] = get_word(data, j++);
  msg->trap_info[2] = get_word(data, j++);
  msg->checksum = get_word(data, j++);

  return 1;  /* Success */
}

static void process_ncp_control(uint8_t source, uint8_t *data, uint16_t count)
{
  int i = 0;

  while (i < count) {
    uint8_t type = data[i++];

    if (type > NCP_RRP) {
      printf("      NCP: Unknown type %u\n", type);
      break;
    }

    stats.ncp_type_count[type]++;

    switch (type) {
    case NCP_NOP:
      /* Skip printing NOPs - too verbose */
      break;

    case NCP_RTS:
      if (i + 9 <= count) {
        uint32_t rsock = (data[i] << 24) | (data[i+1] << 16) |
                         (data[i+2] << 8) | data[i+3];
        uint32_t lsock = (data[i+4] << 24) | (data[i+5] << 16) |
                         (data[i+6] << 8) | data[i+7];
        uint8_t link = data[i+8];
        printf("      NCP: RTS sockets %u:%u link %u\n", rsock, lsock, link);
        i += 9;
      } else {
        printf("      NCP: RTS (truncated)\n");
        break;
      }
      break;

    case NCP_STR:
      if (i + 9 <= count) {
        uint32_t rsock = (data[i] << 24) | (data[i+1] << 16) |
                         (data[i+2] << 8) | data[i+3];
        uint32_t lsock = (data[i+4] << 24) | (data[i+5] << 16) |
                         (data[i+6] << 8) | data[i+7];
        uint8_t size = data[i+8];
        printf("      NCP: STR sockets %u:%u size %u\n", rsock, lsock, size);
        i += 9;
      } else {
        printf("      NCP: STR (truncated)\n");
        break;
      }
      break;

    case NCP_CLS:
      if (i + 8 <= count) {
        uint32_t rsock = (data[i] << 24) | (data[i+1] << 16) |
                         (data[i+2] << 8) | data[i+3];
        uint32_t lsock = (data[i+4] << 24) | (data[i+5] << 16) |
                         (data[i+6] << 8) | data[i+7];
        printf("      NCP: CLS sockets %u:%u\n", rsock, lsock);
        i += 8;
      } else {
        printf("      NCP: CLS (truncated)\n");
        break;
      }
      break;

    case NCP_ALL:
      if (i + 7 <= count) {
        uint8_t link = data[i];
        uint16_t msgs = (data[i+1] << 8) | data[i+2];
        uint32_t bits = (data[i+3] << 24) | (data[i+4] << 16) |
                        (data[i+5] << 8) | data[i+6];
        printf("      NCP: ALL link %u msgs %u bits %u\n", link, msgs, bits);
        i += 7;
      } else {
        printf("      NCP: ALL (truncated)\n");
        break;
      }
      break;

    case NCP_ECO:
      if (i + 1 <= count) {
        printf("      NCP: ECO data=%u\n", data[i]);
        i += 1;
      }
      break;

    case NCP_ERP:
      if (i + 1 <= count) {
        printf("      NCP: ERP data=%u\n", data[i]);
        i += 1;
      }
      break;

    case NCP_RST:
      printf("      NCP: RST (Reset)\n");
      break;

    case NCP_RRP:
      printf("      NCP: RRP (Reset Reply)\n");
      break;

    default:
      printf("      NCP: %s\n", ncp_type_name[type]);
      break;
    }
  }
}

static void process_regular(uint8_t *packet, int length)
{
  uint8_t source = packet[1];
  uint8_t link = packet[2];
  int actual_data_bytes = (length * 2) - 4;  /* Total bytes minus leader */

  stats.regular_packets++;
  stats.hosts[source].packets_from++;
  stats.hosts[source].last_seen = time(NULL);

  if (debug_mode) {
    print_timestamp();
    printf("DATA from host %03o (IMP %d, port %d) link %u: %d bytes\n",
           source, source % 64, source / 64, link, actual_data_bytes);
  }

  if (link == 0) {
    /* Link 0 - NCC status messages: use actual packet length, not any "count" field */
    stats.ncc_status_packets++;

    /* DEBUG: Show IMP leader (first 4 bytes) */
    if (debug_mode) {
      printf("      IMP Leader (4 bytes): ");
      for (int i = 0; i < 4; i++) {
        printf("%03o ", packet[i]);
      }
      printf("\n");
    }

    /* Check first data word for 1973 message type */
    uint8_t leader_type = get_word(packet, 2);
    int imp_num = source % 64;
    int is_1973_message = 0;

    if ((leader_type == 0301 || leader_type == 0303) &&
        actual_data_bytes == 64) {
      /* 1973 Trouble Report (Type 301) - 64 bytes */
      trouble_report_t status;
      if (decode_trouble_report(&packet[4], actual_data_bytes, &status, imp_num)) {
        stats.imps[imp_num].status_reports++;
        stats.imps[imp_num].last_trouble_report = status;
        stats.imps[imp_num].last_status_time = time(NULL);
        stats.imps[imp_num].has_status = 1;
        stats.imps[imp_num].is_1973_format = 1;
        strcpy(stats.imps[imp_num].msg_type, "1973-301");
        is_1973_message = 1;

        if (debug_mode) {
          printf("      IMP %2d: 1973 STATUS-303 Data: ", imp_num);
          for (int j = 0; j < 64; j++) {
            printf("%03o ", status.raw_data[j]);
          }
          printf("\n");
          printf("anomaly = %06o\n", status.anomaly);
          printf("restart_reload = %06o\n", status.restart_reload);
          printf("halt_pc = %06o\n", status.halt_pc);
          printf("halt_a = %06o\n", status.halt_a);
          printf("halt_x = %06o\n", status.halt_x);
          printf("free_count = %06o\n", status.free_count);
          printf("sf_count = %06o\n", status.sf_count);
          printf("reas_count = %06o\n", status.reas_count);
          printf("allocate_count = %06o\n", status.allocate_count);
          printf("imp_version = %06o\n", status.imp_version);
          printf("host34 = %06o\n", status.host34);
          printf("tip_version = %06o\n", status.tip_version);
          printf("host_interface_tested = %06o\n", status.host_interface_tested);
          printf("test_mess_send_count = %06o\n", status.test_mess_send_count);
          printf("test_mess_recvd_count = %06o\n", status.test_mess_recvd_count);
          for (int i = 0; i < 5; i++) {
            printf("modem[%d].routing_msgs_received = %06o\n",
                   i, status.modem[i].routing_msgs_received);
            printf("modem[%d].routing_msgs_errors = %06o\n",
                   i, status.modem[i].routing_msgs_errors);
          };
          printf("line_speed = %06o\n", status.line_speed);
          printf("trap_info0 = %06o\n", status.trap_info[0]);
          printf("trap_info1 = %06o\n", status.trap_info[1]);
          printf("trap_info2 = %06o\n", status.trap_info[2]);
          printf("checksum = %06o\n", status.checksum);
        }
      }
    } else if (leader_type == 0302 && (actual_data_bytes == 106)) {
      /* 1973 Throughput Message (Type 302) - 106 bytes */
      throughput_message_t throughput;
      if (decode_throughput_message(&packet[4], actual_data_bytes, &throughput)) {
        stats.imps[imp_num].throughput_reports++;
        stats.imps[imp_num].last_throughput = throughput;
        stats.imps[imp_num].last_throughput_time = time(NULL);
        stats.imps[imp_num].has_throughput = 1;
        strcpy(stats.imps[imp_num].msg_type, "THRU-302");
        is_1973_message = 1;

        if (debug_mode) {
          printf("      IMP %2d: THROUGHPUT-302 (%d bytes)\n", imp_num, actual_data_bytes);

          /* Calculate totals */
          unsigned long total_pkts = 0, total_words = 0;
          for (int i = 0; i < 5; i++) {
            total_pkts += throughput.modem[i].packets_out;
            total_words += throughput.modem[i].words_out;
          }
          printf("         Modem Total: Pkts=%lu Words=%lu\n", total_pkts, total_words);

          unsigned long total_msgs = 0;
          for (int i = 0; i < 4; i++) {
            total_msgs += throughput.host[i].mess_to_net + throughput.host[i].mess_from_net;
          }
          printf("         Host Total: Messages=%lu\n", total_msgs);
        }
      }
    }

    /* Only try to parse as NCP if:
       1. This is NOT a 1973 message
       2. First byte looks like valid NCP type */
    if (!is_1973_message && actual_data_bytes > 0 && packet[9] <= NCP_RRP) {
      stats.ncp_control_packets++;
      if (debug_mode) {
        process_ncp_control(source, &packet[4], actual_data_bytes);
      }
    }
  } else {
    /* User data */
    stats.user_data_packets++;
    if (debug_mode) {
      printf("      User data: %d bytes on link %u\n", actual_data_bytes, link);

      /* Show first 64 bytes of data in octal */
      if (actual_data_bytes > 0) {
        int display = actual_data_bytes < 64 ? actual_data_bytes : 64;
        printf("      Data: ");
        for (int i = 0; i < display; i++) {
          printf("%03o ", packet[9 + i]);
          if ((i + 1) % 16 == 0 && i + 1 < display)
            printf("\n            ");
        }
        if (actual_data_bytes > 64)
          printf("... (%d more bytes)", actual_data_bytes - 64);
        printf("\n");
      }
    }
  }
}

static void process_rfnm(uint8_t *packet, int length)
{
  uint8_t host = packet[1];
  uint8_t link = packet[2];

  stats.rfnm_packets++;

  if (debug_mode) {
    print_timestamp();
    printf("RFNM for host %03o link %u\n", host, link);
  }
}

static void process_reset(uint8_t *packet, int length)
{
  stats.reset_packets++;

  print_timestamp();
  printf("IMP RESET\n");
}

static void process_host_dead(uint8_t *packet, int length)
{
  uint8_t host = packet[1];
  uint8_t subtype = packet[3] & 0x0F;
  const char *reason;

  stats.dead_host_packets++;

  switch (subtype) {
  case 0: reason = "IMP cannot be reached"; break;
  case 1: reason = "host not up"; break;
  case 3: reason = "communication prohibited"; break;
  default: reason = "unknown reason"; break;
  }

  print_timestamp();
  printf("HOST DEAD: %03o (%s)\n", host, reason);
}

static void process_other(uint8_t *packet, int length, int type)
{
  stats.other_packets++;

  print_timestamp();
  printf("IMP: %s\n", type <= IMP_RESET ? imp_type_name[type] : "UNKNOWN");
}

static void process_imp(uint8_t *packet, int length)
{
  int type;

  stats.total_packets++;
  stats.total_bytes += length * 2;  /* 16-bit words */

  if (length < 2) {
    print_timestamp();
    printf("ERROR: Leader too short (%d words)\n", length);
    return;
  }

  type = packet[0] & 0x0F;

  switch (type) {
  case IMP_REGULAR:
    process_regular(packet, length);
    break;
  case IMP_RFNM:
    process_rfnm(packet, length);
    break;
  case IMP_RESET:
    process_reset(packet, length);
    break;
  case IMP_DEAD:
    process_host_dead(packet, length);
    break;
  default:
    process_other(packet, length, type);
    break;
  }
}

/* Print Status Messages table (Type 304) */
static void print_status_table(time_t now)
{
  printf("===============================================================================\n");
  printf("STATUS MESSAGES (Type 301) - System Health & Configuration\n");
  printf("===============================================================================\n");
  printf("IMP  Name          BANOM  Buffers        Lines  Hosts  Ver   Last  Alerts\n");
  printf("                   (oct)  Fr  SF  Rs Al  U/D    Act/4  IMP   (sec)\n");
  printf("===============================================================================\n");

  for (int i = 0; i < 64; i++) {
    if (!stats.imps[i].configured) continue;

    if (stats.imps[i].has_status && stats.imps[i].is_1973_format) {
      /* 1973 format - limited data */
      time_t last_sec = now - stats.imps[i].last_status_time;
      trouble_report_t *st = &stats.imps[i].last_trouble_report;

      /* Count lines up/down */
      int lines_up = 0, lines_down = 0;
      for (int m = 0; m < 5; m++) {
        if (st->modem[m].routing_msgs_errors & 0100000) lines_down++;
        else lines_up++;
      }

      printf("%3d  %-12s  %3u %3u %3u %2u  %u/%u    %4u  %4ld\n",
             i, stats.imps[i].name,
             st->free_count, st->sf_count, st->reas_count, st->allocate_count,
             lines_up, lines_down,
             st->imp_version,
             last_sec);
    } else {
      printf("%3d  %-12s  *** NO STATUS MESSAGE RECEIVED ***\n", i, stats.imps[i].name);
    }
  }
  printf("===============================================================================\n\n");
}

/* Print Line Details table (from Status messages) */
static void print_line_details_table(void)
{
  printf("===============================================================================\n");
  printf("STATUS DETAILS - Line Connectivity (from last Status messages)\n");
  printf("===============================================================================\n");
  printf("IMP  Line1      Line2      Line3      Line4      Line5\n");
  printf("===============================================================================\n");

  for (int i = 0; i < 64; i++) {
    if (!stats.imps[i].configured) continue;

    printf("%3d  ", i);

    if (stats.imps[i].has_status) {
      trouble_report_t *st = &stats.imps[i].last_trouble_report;

      for (int m = 0; m < 5; m++) {
        if (st->modem[m].routing_msgs_errors & 034700) {
          printf("%2d(", (st->modem[m].routing_msgs_errors >> 8) & 077);
          if (st->modem[m].routing_msgs_errors & 0100000) printf("DN");
          else if (st->modem[m].routing_msgs_errors & 040000) printf("LP");
          else printf("UP");
          if (st->modem[m].routing_msgs_errors & 0377) printf(",E");
          printf(")");
        } else {
          printf("-(NC)");
        }
        printf("  ");
      }
      printf("\n");
    } else {
      printf("NO STATUS\n");
    }
  }

  printf("===============================================================================\n");
  printf("Legend: UP=operational, DN=down, LP=looped, E=errors, NC=not connected\n\n");
}

/* Print Throughput table (Type 302) */
static void print_throughput_table(time_t now)
{
  unsigned long net_total_pkts = 0, net_total_words = 0;
  unsigned long net_total_msgs = 0, net_total_host_pkts = 0;

  printf("===============================================================================\n");
  printf("THROUGHPUT MESSAGES (Type 302) - Traffic Statistics\n");
  printf("===============================================================================\n");
  printf("IMP  Name          Modem Traffic      Host Traffic         Rates\n");
  printf("                   Pkts    Words      Msgs    Pkts        Pk/s  KB/s  Last\n");
  printf("===============================================================================\n");

  for (int i = 0; i < 64; i++) {
    if (!stats.imps[i].configured) continue;

    if (stats.imps[i].has_throughput && !stats.imps[i].is_1973_format) {
      /* 1976 format */
      throughput_message_t *th = &stats.imps[i].last_throughput;
      time_t last_sec = now - stats.imps[i].last_throughput_time;

      /* Sum modem traffic */
      unsigned long total_pkts = 0, total_words = 0;
      for (int m = 0; m < 5; m++) {
        total_pkts += th->modem[m].packets_out;
        total_words += th->modem[m].words_out;
      }

      /* Sum host traffic */
      unsigned long total_msgs = 0, total_host_pkts = 0;
      for (int h = 0; h < 4; h++) {
        total_msgs += th->host[h].mess_to_net + th->host[h].mess_from_net;
        total_host_pkts += th->host[h].packet_to_net + th->host[h].packet_from_net;
      }

      net_total_pkts += total_pkts;
      net_total_words += total_words;
      net_total_msgs += total_msgs;
      net_total_host_pkts += total_host_pkts;

      /* Format with K suffix for large numbers */
      char pkts_str[10], words_str[10], msgs_str[10], hpkts_str[10];
      snprintf(pkts_str, sizeof(pkts_str), total_pkts > 9999 ? "%luK" : "%lu",
               total_pkts > 9999 ? total_pkts / 1000 : total_pkts);
      snprintf(words_str, sizeof(words_str), total_words > 9999 ? "%luK" : "%lu",
               total_words > 9999 ? total_words / 1000 : total_words);
      snprintf(msgs_str, sizeof(msgs_str), total_msgs > 9999 ? "%luK" : "%lu",
               total_msgs > 9999 ? total_msgs / 1000 : total_msgs);
      snprintf(hpkts_str, sizeof(hpkts_str), total_host_pkts > 9999 ? "%luK" : "%lu",
               total_host_pkts > 9999 ? total_host_pkts / 1000 : total_host_pkts);

      printf("%3d  %-12s  %6s  %7s    %6s  %6s       -     -  %4ld\n",
             i, stats.imps[i].name,
             pkts_str, words_str, msgs_str, hpkts_str,
             last_sec);
    } else if (stats.imps[i].has_throughput && stats.imps[i].is_1973_format) {
      /* 1973 format - show basic fields */
      throughput_1973_t *th = &stats.imps[i].last_throughput_1973;
      time_t last_sec = now - stats.imps[i].last_throughput_time;

      printf("%3d  %-12s  1973  Cntr:%3u Fld1:%04X Pat:%04X/%04X Var:%04X %4ld\n",
             i, stats.imps[i].name,
             th->counter, th->field1,
             th->pattern_0628, th->pattern_ffff, th->variable_field,
             last_sec);
    } else {
      printf("%3d  %-12s  *** NO THROUGHPUT MESSAGE RECEIVED ***\n",
             i, stats.imps[i].name);
    }
  }

  printf("===============================================================================\n");
  printf("Network Total:   %6luK %7luK   %6luK %6luK\n",
         net_total_pkts / 1000, net_total_words / 1000,
         net_total_msgs / 1000, net_total_host_pkts / 1000);
  printf("===============================================================================\n\n");
}

/* Print network summary */
static void print_network_summary(time_t now, unsigned long elapsed)
{
  int configured_imps = 0, status_reporting = 0, thru_reporting = 0;

  for (int i = 0; i < 64; i++) {
    if (stats.imps[i].configured) {
      configured_imps++;
      if (stats.imps[i].has_status) status_reporting++;
      if (stats.imps[i].has_throughput) thru_reporting++;
    }
  }

  unsigned long total_status = 0, total_throughput = 0, total_keepalives = 0;
  unsigned long total_large = 0, total_unknown = 0;
  for (int i = 0; i < 64; i++) {
    total_status += stats.imps[i].status_reports;
    total_throughput += stats.imps[i].throughput_reports;
    total_keepalives += stats.imps[i].keepalives;
    total_large += stats.imps[i].large_messages;
    total_unknown += stats.imps[i].unknown_messages;
  }

  printf("===============================================================================\n");
  printf("NETWORK SUMMARY\n");
  printf("===============================================================================\n");
  printf("Active IMPs:      %d/%d configured\n", status_reporting, configured_imps);
  printf("Status Messages:  %lu received (Type 301/303/304)\n", total_status);
  printf("Thruput Messages: %lu received (Type 302/305)\n", total_throughput);
  printf("Keepalives:       %lu received (0 bytes)\n", total_keepalives);
  printf("Large Messages:   %lu received (>1000 bytes, diagnostics)\n", total_large);
  printf("Unknown Messages: %lu received (unrecognized types)\n", total_unknown);
  printf("\n");
  printf("NCP Control:      %lu packets\n", stats.ncp_control_packets);
  printf("User Data:        %lu packets\n", stats.user_data_packets);
  printf("RFNM:             %lu packets\n", stats.rfnm_packets);
  printf("Host Dead:        %lu packets\n", stats.dead_host_packets);
  printf("IMP Reset:        %lu packets\n", stats.reset_packets);
  printf("Other IMP:        %lu packets\n", stats.other_packets);
  printf("\n");
  printf("Total Packets:    %lu (%.1f/sec)\n",
         stats.total_packets,
         elapsed > 0 ? (double)stats.total_packets / elapsed : 0);
  printf("Total Bytes:      %lu (%.1f KB/sec)\n",
         stats.total_bytes,
         elapsed > 0 ? (double)stats.total_bytes / elapsed / 1024.0 : 0);
  printf("Runtime:          %luh %lum %lus\n",
         elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60);
  printf("===============================================================================\n\n");
}

/* Main statistics display - calls all sub-tables */
static void print_statistics(void)
{
  time_t now = time(NULL);
  unsigned long elapsed = now - stats.start_time;

  printf("\n");
  print_status_table(now);
  print_line_details_table();
  print_throughput_table(now);
  print_network_summary(now, elapsed);
}

static void ncp_imp_ready(int flag)
{
  print_timestamp();
  if (flag)
    printf("IMP is READY\n");
  else
    printf("IMP is NOT READY\n");
}

int main(int argc, char **argv)
{
  printf("BBN Network Control Center - IMP Monitor\n");
  printf("=========================================\n");
  printf("Monitoring IMP #5, Host 0\n");
  printf("Press Ctrl+C to stop and see statistics\n");
  printf("\n");

  memset(&stats, 0, sizeof(stats));
  stats.start_time = time(NULL);

  /* Load network topology */
  load_topology_config();

  imp_init(argc, argv);
  imp_imp_ready = ncp_imp_ready;
  imp_host_ready(1);  /* Tell IMP we're ready to receive */

  print_timestamp();
  printf("Monitor started and ready\n");
  printf("\n");

  /* Main receive loop */
  printf("================================================================================\n");
  printf("COMMANDS:\n");
  printf("  d + ENTER  - Toggle debug output (show detailed packet decoding)\n");
  printf("  s + ENTER  - Display statistics table immediately\n");
  printf("  q + ENTER  - Quit program\n");
  printf("\n");
  printf("Statistics will be displayed automatically every 30 seconds.\n");
  printf("================================================================================\n");
  printf("\n");

  for (;;) {
    int n, maxfd;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    imp_fd_set(&rfds);
    FD_SET(STDIN_FILENO, &rfds);  /* Monitor stdin for commands */

    /* Calculate max fd for select() */
    maxfd = imp_get_fd();
    if (STDIN_FILENO > maxfd)
      maxfd = STDIN_FILENO;

    tv.tv_sec = 30;  /* 30 second timeout for statistics */
    tv.tv_usec = 0;

    n = select(maxfd + 1, &rfds, NULL, NULL, &tv);

    if (n == -1) {
      perror("select");
      break;
    } else if (n == 0) {
      /* Timeout - print periodic statistics */
      print_statistics();
    } else {
      /* Check for keyboard input */
      if (FD_ISSET(STDIN_FILENO, &rfds)) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
          if (c == 'd' || c == 'D') {
            debug_mode = !debug_mode;
            printf("\n>>> Debug mode %s <<<\n\n", debug_mode ? "ENABLED" : "DISABLED");
          } else if (c == 's' || c == 'S') {
            print_statistics();
          } else if (c == 'q' || c == 'Q') {
            printf("\nExiting...\n");
            break;
          }
        }
      }

      if (imp_fd_isset(&rfds)) {
        memset(packet, 0, sizeof(packet));
        imp_receive_message(packet, &n);
        if (n > 0)
          process_imp(packet, n);
      }
    }
  }

  print_statistics();
  return 0;
}
