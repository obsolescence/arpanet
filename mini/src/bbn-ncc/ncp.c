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

/* 1973 IMP Throughput Message (Type 303) - 59 bytes */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Actual data starts at byte 8 (after 8-byte padding) */
  uint8_t counter;           /* Byte 8 - increments each report */
  uint16_t field1;           /* Bytes 10-11 - small counter */
  uint16_t pattern_0628;     /* Bytes 16-17 - always 0x0628? */
  uint16_t pattern_ffff;     /* Bytes 22-23 - always 0xFFFF? */
  uint16_t variable_field;   /* Bytes 28-29 - varies by IMP */

  uint8_t raw_data[59];      /* Store full message for analysis */
} throughput_1973_t;

/* 1973 IMP Status Message (Type 302) - 101 bytes */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Fields discovered from analysis */
  uint16_t word1;            /* Bytes 0-1 */
  uint16_t word2;            /* Bytes 2-3 */
  uint16_t word3;            /* Bytes 4-5 - sometimes contains 400 type code */
  uint16_t word4;            /* Bytes 6-7 */
  uint16_t word5;            /* Bytes 8-9 */

  uint8_t raw_data[101];     /* Store full message for analysis */
} status_1973_t;

/* IMP Status Message (Type 304) - 36 words, 72 bytes */
typedef struct {
  uint8_t imp_number;
  uint16_t message_type;

  /* Word 4: BANOM - Anomaly/status flags (11 bits) */
  uint16_t banom;
  uint8_t mesgen_on;
  uint8_t iosec_on;
  uint8_t snapshot_on;
  uint8_t trce_on;
  uint8_t mem_off;
  uint8_t sat_up;
  uint8_t override_on;
  uint8_t ss1_on, ss2_on, ss3_on, ss4_on;

  /* Word 5: BTRANS */
  uint8_t ns_reload;
  uint8_t ns_restart;
  uint8_t restart_code;

  /* Words 6-8: Trap info */
  uint16_t trap_location;
  uint32_t trap_data;

  /* Words 9-12: Buffer counts */
  uint16_t free_count;
  uint16_t sf_count;
  uint16_t reas_count;
  uint16_t allocate_count;

  /* Words 13-16: Versions and hosts */
  uint16_t imp_version;
  uint16_t tip_version;
  uint8_t hosts_4, hosts_3;
  uint8_t sat_present, cdh_present;
  uint8_t host_state[4];

  /* Words 17-19: Host testing */
  int16_t host_test_num;
  uint16_t nops_sent;
  uint16_t nops_received;

  /* Words 20-29: 5 line/modem pairs */
  struct {
    uint16_t routing_msgs;
    uint8_t dead;
    uint8_t looped;
    uint8_t imp_other_end;
    uint8_t error_count;
  } modem[5];

  /* Words 30-33: Diagnostics */
  uint16_t modem_speed;
  uint16_t reload_location;
  uint32_t reload_data;

  /* Word 34: Checksum */
  uint16_t checksum;
} status_message_t;

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
    status_message_t last_status;
    throughput_message_t last_throughput;
    status_1973_t last_status_1973;
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

/* Decode IMP Status Message (Type 304) - 36 words, 72 bytes */
static int decode_status_message(uint8_t *data, int count, status_message_t *msg)
{
  if (count != 72) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(status_message_t));

  /* Word 1: BHEAD - extract IMP number */
  uint16_t word1 = get_word(data, 0);
  uint8_t imp_upper = (word1 >> 3) & 0x7;
  uint8_t imp_lower = word1 & 0x7;
  msg->imp_number = imp_upper * 8 + imp_lower;  /* Octal to decimal */

  /* Word 3: BCODE - verify message type 304 */
  uint16_t word3 = get_word(data, 2);
  uint8_t d1 = (word3 >> 6) & 0x7;
  uint8_t d2 = (word3 >> 3) & 0x7;
  uint8_t d3 = word3 & 0x7;
  msg->message_type = d1 * 100 + d2 * 10 + d3;

  if (msg->message_type != 304) {
    return 0;  /* Not a status message */
  }

  /* Word 4: BANOM - Anomaly flags (lower 11 bits) */
  uint16_t word4 = get_word(data, 3);
  msg->banom = word4 & 0x7FF;
  msg->mesgen_on = (word4 >> 10) & 1;
  msg->iosec_on = (word4 >> 9) & 1;
  msg->snapshot_on = (word4 >> 8) & 1;
  msg->trce_on = (word4 >> 7) & 1;
  msg->mem_off = (word4 >> 6) & 1;
  msg->sat_up = (word4 >> 5) & 1;
  msg->override_on = (word4 >> 4) & 1;
  msg->ss1_on = (word4 >> 3) & 1;
  msg->ss2_on = (word4 >> 2) & 1;
  msg->ss3_on = (word4 >> 1) & 1;
  msg->ss4_on = word4 & 1;

  /* Word 5: BTRANS */
  uint16_t word5 = get_word(data, 4);
  msg->ns_reload = (word5 >> 6) & 0x7;
  msg->ns_restart = (word5 >> 3) & 0x7;
  msg->restart_code = word5 & 0x7;

  /* Word 6: BBGHLT - Trap location */
  msg->trap_location = get_word(data, 5);

  /* Words 7-8: Trap data (32-bit) */
  msg->trap_data = ((uint32_t)get_word(data, 6) << 16) | get_word(data, 7);

  /* Word 9: BFREE - Free buffer count (lower 9 bits) */
  msg->free_count = get_word(data, 8) & 0x1FF;

  /* Word 10: BSANDF - Store-and-forward count (lower 9 bits) */
  msg->sf_count = get_word(data, 9) & 0x1FF;

  /* Word 11: BREAS - Reassembly count (lower 9 bits) */
  msg->reas_count = get_word(data, 10) & 0x1FF;

  /* Word 12: BALLOC - Allocate count (lower 9 bits) */
  msg->allocate_count = get_word(data, 11) & 0x1FF;

  /* Word 13: BVERS - IMP version */
  msg->imp_version = get_word(data, 12);

  /* Word 14: BHST34 */
  uint16_t word14 = get_word(data, 13);
  msg->hosts_4 = (word14 >> 15) & 1;
  msg->hosts_3 = (word14 >> 14) & 1;
  msg->sat_present = (word14 >> 1) & 1;
  msg->cdh_present = word14 & 1;

  /* Word 15: BTVERS - TIP version */
  msg->tip_version = get_word(data, 14);

  /* Word 16: BHOST - Host states (4 hosts, 4 bits each) */
  uint16_t word16 = get_word(data, 15);
  msg->host_state[0] = (word16 >> 12) & 0xF;
  msg->host_state[1] = (word16 >> 8) & 0xF;
  msg->host_state[2] = (word16 >> 4) & 0xF;
  msg->host_state[3] = word16 & 0xF;

  /* Word 17: BHLNUM - Host being tested (signed) */
  msg->host_test_num = (int16_t)get_word(data, 16);

  /* Word 18: BHLSNT - NOPs sent */
  msg->nops_sent = get_word(data, 17);

  /* Word 19: BHLRCV - NOPs received */
  msg->nops_received = get_word(data, 18);

  /* Words 20-29: 5 modem/line pairs (BMOD1-5) */
  for (int i = 0; i < 5; i++) {
    int base = 19 + (i * 2);  /* Word 20, 22, 24, 26, 28 */
    msg->modem[i].routing_msgs = get_word(data, base);

    uint16_t status = get_word(data, base + 1);
    msg->modem[i].dead = (status >> 15) & 1;
    msg->modem[i].looped = (status >> 14) & 1;
    msg->modem[i].imp_other_end = (status >> 8) & 0x3F;
    msg->modem[i].error_count = status & 0xFF;
  }

  /* Word 30: BSPEED */
  msg->modem_speed = get_word(data, 29);

  /* Word 31: BRELOD */
  msg->reload_location = get_word(data, 30);

  /* Words 32-33: BSPEED (reload data, 32-bit) */
  msg->reload_data = ((uint32_t)get_word(data, 31) << 16) | get_word(data, 32);

  /* Word 34: BCHKSM */
  msg->checksum = get_word(data, 33);

  return 1;  /* Success */
}

/* Decode IMP Throughput Message (Type 302) - 59 words, 118 bytes */
static int decode_throughput_message(uint8_t *data, int count, throughput_message_t *msg)
{
  if (count != 118) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(throughput_message_t));

  /* Word 1: BHEAD - extract IMP number */
  uint16_t word1 = get_word(data, 0);
  uint8_t imp_upper = (word1 >> 3) & 0x7;
  uint8_t imp_lower = word1 & 0x7;
  msg->imp_number = imp_upper * 8 + imp_lower;  /* Octal to decimal */

  /* Word 3: BCODE - verify message type 302 */
  uint16_t word3 = get_word(data, 2);
  uint8_t d1 = (word3 >> 6) & 0x7;
  uint8_t d2 = (word3 >> 3) & 0x7;
  uint8_t d3 = word3 & 0x7;
  msg->message_type = d1 * 100 + d2 * 10 + d3;

  if (msg->message_type != 302) {
    return 0;  /* Not a throughput message */
  }

  /* Words 4-13: 5 modem pairs (SMOD1-5) */
  for (int i = 0; i < 5; i++) {
    int base = 3 + (i * 2);  /* Word 4, 6, 8, 10, 12 */
    msg->modem[i].packets_out = get_word(data, base);
    msg->modem[i].words_out = get_word(data, base + 1);
  }

  /* Words 14-53: 4 host throughput blocks (HOST0-3) */
  for (int i = 0; i < 4; i++) {
    int base = 13 + (i * 10);  /* Word 14, 24, 34, 44 */
    msg->host[i].mess_to_net = get_word(data, base);
    msg->host[i].mess_from_net = get_word(data, base + 1);
    msg->host[i].packet_to_net = get_word(data, base + 2);
    msg->host[i].packet_from_net = get_word(data, base + 3);
    msg->host[i].local_mess_sent = get_word(data, base + 4);
    msg->host[i].local_mess_rcvd = get_word(data, base + 5);
    msg->host[i].local_packet_sent = get_word(data, base + 6);
    msg->host[i].local_packet_rcvd = get_word(data, base + 7);
    msg->host[i].words_to_net = get_word(data, base + 8);
    msg->host[i].words_from_net = get_word(data, base + 9);
  }

  /* Words 54-56: SNETIM - Background counts */
  msg->background_counts[0] = get_word(data, 53);
  msg->background_counts[1] = get_word(data, 54);
  msg->background_counts[2] = get_word(data, 55);

  /* Word 57: BCHKSM */
  msg->checksum = get_word(data, 56);

  return 1;  /* Success */
}

/* Decode 1973 IMP Throughput Message (Type 303) - 59 bytes */
static int decode_1973_throughput(uint8_t *data, int count, throughput_1973_t *msg, uint8_t imp_num)
{
  if (count != 59) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(throughput_1973_t));
  memcpy(msg->raw_data, data, 59);

  msg->imp_number = imp_num;
  msg->message_type = 303;

  /* Data starts at byte 8 (skip 8-byte padding) */
  msg->counter = data[8];
  msg->field1 = get_word(data, 5);       /* Bytes 10-11 */
  msg->pattern_0628 = get_word(data, 8); /* Bytes 16-17 */
  msg->pattern_ffff = get_word(data, 11); /* Bytes 22-23 */
  msg->variable_field = get_word(data, 14); /* Bytes 28-29 */

  return 1;  /* Success */
}

/* Decode 1973 IMP Status Message (Type 302) - 101 bytes */
static int decode_1973_status(uint8_t *data, int count, status_1973_t *msg, uint8_t imp_num)
{
  if (count != 101) {
    return 0;  /* Wrong size */
  }

  memset(msg, 0, sizeof(status_1973_t));
  memcpy(msg->raw_data, data, 101);

  msg->imp_number = imp_num;
  msg->message_type = 302;

  /* Extract first few words for analysis */
  msg->word1 = get_word(data, 0);
  msg->word2 = get_word(data, 1);
  msg->word3 = get_word(data, 2);
  msg->word4 = get_word(data, 3);
  msg->word5 = get_word(data, 4);

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

static void process_ncc_status(uint8_t source, uint8_t *data, int count)
{
  /* Extract IMP number and port from host number */
  int imp_num = source % 64;
  int port = source / 64;
  time_t now = time(NULL);

  /* Update IMP statistics */
  if (stats.imps[imp_num].first_seen == 0)
    stats.imps[imp_num].first_seen = now;
  stats.imps[imp_num].last_seen = now;

  /* Track message sizes */
  stats.imps[imp_num].last_message_bytes = count;
  stats.imps[imp_num].total_message_bytes += count;
  if (stats.imps[imp_num].min_message_bytes == 0 || count < stats.imps[imp_num].min_message_bytes)
    stats.imps[imp_num].min_message_bytes = count;
  if (count > stats.imps[imp_num].max_message_bytes)
    stats.imps[imp_num].max_message_bytes = count;

  /* Handle keepalive (0 bytes) */
  if (count == 0) {
    stats.imps[imp_num].keepalives++;
    if (debug_mode) {
      printf("      IMP %2d (port %d): Keepalive\n", imp_num, port);
    }
    return;
  }

  /* Handle large diagnostic messages (core dumps, etc.) */
  if (count > 1000) {
    stats.imps[imp_num].large_messages++;
    if (debug_mode) {
      printf("      IMP %2d (port %d): LARGE MESSAGE (%d bytes) - diagnostic dump\n",
             imp_num, port, count);
    }
    return;
  }

  /* Need at least 6 bytes (3 words) to read message type from Word 3 */
  if (count < 6) {
    if (debug_mode) {
      printf("      IMP %2d (port %d): INVALID message (%d bytes, too short)\n",
             imp_num, port, count);
    }
    return;
  }

  /* Detect message type from Word 3 */
  uint16_t word3 = get_word(data, 2);
  uint8_t d1 = (word3 >> 6) & 0x7;
  uint8_t d2 = (word3 >> 3) & 0x7;
  uint8_t d3 = word3 & 0x7;
  uint16_t msg_type = d1 * 100 + d2 * 10 + d3;

  if (msg_type == 304) {
    /* Status Message (Type 304) - 72 bytes */
    status_message_t status;
    if (decode_status_message(data, count, &status)) {
      stats.imps[imp_num].status_reports++;
      stats.imps[imp_num].last_status = status;
      stats.imps[imp_num].last_status_time = now;
      stats.imps[imp_num].has_status = 1;
      strcpy(stats.imps[imp_num].msg_type, "STATUS-304");

      if (debug_mode) {
        printf("      IMP %2d (port %d): STATUS-304 (%d bytes)\n", imp_num, port, count);
        printf("         BANOM: %05o", status.banom);
        if (status.mem_off) printf(" [MEM-OFF]");
        if (status.mesgen_on) printf(" [MESGEN]");
        if (status.trce_on) printf(" [TRACE]");
        printf("\n");
        printf("         Buffers: Free=%u SF=%u Reas=%u Alloc=%u\n",
               status.free_count, status.sf_count, status.reas_count, status.allocate_count);
        printf("         Version: IMP=%u TIP=%u\n", status.imp_version, status.tip_version);
        printf("         Hosts: [%u %u %u %u]\n",
               status.host_state[0], status.host_state[1],
               status.host_state[2], status.host_state[3]);
        printf("         Lines:");
        for (int i = 0; i < 5; i++) {
          if (status.modem[i].imp_other_end > 0) {
            printf(" %d→%d", i+1, status.modem[i].imp_other_end);
            if (status.modem[i].dead) printf("(DEAD)");
            else if (status.modem[i].looped) printf("(LOOP)");
            if (status.modem[i].error_count > 0) printf("[E:%u]", status.modem[i].error_count);
          }
        }
        printf("\n");
      }
    } else {
      if (debug_mode) {
        printf("      IMP %2d (port %d): STATUS-304 DECODE FAILED (%d bytes)\n",
               imp_num, port, count);
      }
    }

  } else if (msg_type == 302) {
    /* Throughput Message (Type 302) - 118 bytes */
    throughput_message_t throughput;
    if (decode_throughput_message(data, count, &throughput)) {
      stats.imps[imp_num].throughput_reports++;
      stats.imps[imp_num].last_throughput = throughput;
      stats.imps[imp_num].last_throughput_time = now;
      stats.imps[imp_num].has_throughput = 1;
      strcpy(stats.imps[imp_num].msg_type, "THRU-302");

      if (debug_mode) {
        printf("      IMP %2d (port %d): THROUGHPUT-302 (%d bytes)\n", imp_num, port, count);

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
    } else {
      if (debug_mode) {
        printf("      IMP %2d (port %d): THROUGHPUT-302 DECODE FAILED (%d bytes)\n",
               imp_num, port, count);
      }
    }

  } else {
    /* Unknown message type */
    stats.imps[imp_num].unknown_messages++;
    snprintf(stats.imps[imp_num].msg_type, sizeof(stats.imps[imp_num].msg_type),
             "TYPE-%u", msg_type);

    if (debug_mode) {
      printf("      IMP %2d (port %d): UNKNOWN TYPE %u (%d bytes)\n",
             imp_num, port, msg_type, count);

      /* Show first 32 bytes in octal for analysis */
      printf("         Data: ");
      for (int i = 0; i < count && i < 32; i++) {
        printf("%03o ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < count) printf("\n               ");
      }
      printf("\n");
    }
  }
}

static void process_regular(uint8_t *packet, int length)
{
  uint8_t source = packet[1];
  uint8_t link = packet[2];
  int actual_data_bytes = (length * 2) - 9;  /* Total bytes minus leader */

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

    /* DEBUG: Show IMP leader (first 9 bytes) */
    if (debug_mode) {
      printf("      IMP Leader (9 bytes): ");
      for (int i = 0; i < 9; i++) {
        printf("%03o ", packet[i]);
      }
      printf("\n");
    }

    /* Check IMP leader byte 5 for 1973 message type */
    uint8_t leader_type = packet[5];
    int imp_num = source % 64;
    int is_1973_message = 0;

    if (leader_type == 0xC3 && actual_data_bytes == 59) {
      /* 1973 Throughput Message (Type 303) - 59 bytes */
      throughput_1973_t throughput;
      if (decode_1973_throughput(&packet[9], actual_data_bytes, &throughput, imp_num)) {
        stats.imps[imp_num].throughput_reports++;
        stats.imps[imp_num].last_throughput_1973 = throughput;
        stats.imps[imp_num].last_throughput_time = time(NULL);
        stats.imps[imp_num].has_throughput = 1;
        stats.imps[imp_num].is_1973_format = 1;
        strcpy(stats.imps[imp_num].msg_type, "1973-303");
        is_1973_message = 1;

        if (debug_mode) {
          printf("      IMP %2d: 1973 THROUGHPUT-303 (59 bytes) Data: ", imp_num);
          for (int j = 0; j < 59; j++) {
            printf("%03o ", throughput.raw_data[j]);
          }
          printf("\n");
        }
      }
    } else if (leader_type == 0xC2 && actual_data_bytes == 101) {
      /* 1973 Status Message (Type 302) - 101 bytes */
      status_1973_t status;
      if (decode_1973_status(&packet[9], actual_data_bytes, &status, imp_num)) {
        stats.imps[imp_num].status_reports++;
        stats.imps[imp_num].last_status_1973 = status;
        stats.imps[imp_num].last_status_time = time(NULL);
        stats.imps[imp_num].has_status = 1;
        stats.imps[imp_num].is_1973_format = 1;
        strcpy(stats.imps[imp_num].msg_type, "1973-302");
        is_1973_message = 1;

        if (debug_mode) {
          printf("      IMP %2d: 1973 STATUS-302 (101 bytes) Data: ", imp_num);
          for (int j = 0; j < 101; j++) {
            printf("%03o ", status.raw_data[j]);
          }
          printf("\n");
        }
      }
    } else {
      /* Fall back to 1976 format detection or unknown */
      process_ncc_status(source, &packet[9], actual_data_bytes);
    }

    /* Only try to parse as NCP if:
       1. This is NOT a 1973 message
       2. First byte looks like valid NCP type */
    if (!is_1973_message && actual_data_bytes > 0 && packet[9] <= NCP_RRP) {
      stats.ncp_control_packets++;
      if (debug_mode) {
        process_ncp_control(source, &packet[9], actual_data_bytes);
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
  printf("STATUS MESSAGES (Type 304) - System Health & Configuration\n");
  printf("===============================================================================\n");
  printf("IMP  Name          BANOM  Buffers        Lines  Hosts  Ver   Last  Alerts\n");
  printf("                   (oct)  Fr  SF  Rs Al  U/D    Act/4  IMP   (sec)\n");
  printf("===============================================================================\n");

  for (int i = 0; i < 64; i++) {
    if (!stats.imps[i].configured) continue;

    if (stats.imps[i].has_status && !stats.imps[i].is_1973_format) {
      /* 1976 format */
      status_message_t *st = &stats.imps[i].last_status;
      time_t last_sec = now - stats.imps[i].last_status_time;

      /* Count active hosts */
      int active_hosts = 0;
      for (int h = 0; h < 4; h++) {
        if (st->host_state[h] != 0) active_hosts++;
      }

      /* Count lines up/down */
      int lines_up = 0, lines_down = 0;
      for (int m = 0; m < 5; m++) {
        if (st->modem[m].imp_other_end > 0) {
          if (st->modem[m].dead) lines_down++;
          else lines_up++;
        }
      }

      /* Build alert string */
      char alerts[20] = "";
      if (st->mem_off) strcat(alerts, "MEM ");
      if (st->trap_location) strcat(alerts, "TRAP ");
      if (st->restart_code) strcat(alerts, "RSTR ");
      if (alerts[0] == '\0') strcpy(alerts, "-");

      printf("%3d  %-12s  %05o  %3u %3u %3u %2u  %u/%u    %u/4  %4u  %4ld  %s\n",
             i, stats.imps[i].name,
             st->banom,
             st->free_count, st->sf_count, st->reas_count, st->allocate_count,
             lines_up, lines_down,
             active_hosts,
             st->imp_version,
             last_sec,
             alerts);
    } else if (stats.imps[i].has_status && stats.imps[i].is_1973_format) {
      /* 1973 format - limited data */
      status_1973_t *st = &stats.imps[i].last_status_1973;
      time_t last_sec = now - stats.imps[i].last_status_time;

      printf("%3d  %-12s  1973   W1:%04X W2:%04X W3:%04X W4:%04X W5:%04X  %4ld\n",
             i, stats.imps[i].name,
             st->word1, st->word2, st->word3, st->word4, st->word5,
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
      status_message_t *st = &stats.imps[i].last_status;

      for (int m = 0; m < 5; m++) {
        if (st->modem[m].imp_other_end > 0) {
          printf("%2d(", st->modem[m].imp_other_end);
          if (st->modem[m].dead) printf("DN");
          else if (st->modem[m].looped) printf("LP");
          else printf("UP");
          if (st->modem[m].error_count > 0) printf(",E");
          printf(")");
        } else {
          printf("-(NC)");
        }
        printf("  ");
        if (m == 2) printf("\n     ");  /* Line break after 3 modems for readability */
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
  printf("Status Messages:  %lu received (Type 302/304)\n", total_status);
  printf("Thruput Messages: %lu received (Type 302/303)\n", total_throughput);
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
