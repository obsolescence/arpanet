/* NCP protocol definitions */
#ifndef NCP_H
#define NCP_H

#include <stdint.h>

/* NCP message opcodes */
#define NCP_NOP      0
#define NCP_RTS      1  /* Request To Send */
#define NCP_STR      2  /* Sender To Receiver */
#define NCP_CLS      3  /* Close */
#define NCP_ALL      4  /* Allocate */
#define NCP_GVB      5  /* Give Back */
#define NCP_RET      6  /* Return */
#define NCP_INR      7  /* Interrupt by Receiver */
#define NCP_INS      8  /* Interrupt by Sender */
#define NCP_ECO      9  /* Echo */
#define NCP_ERP     10  /* Echo Reply */
#define NCP_ERR     11  /* Error */
#define NCP_RST     12  /* Reset */
#define NCP_RRP     13  /* Reset Reply */

/* IMP message types */
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

/* Telnet protocol constants */
#define OLD_TELNET    1
#define NEW_TELNET   23

/* Old telnet opcodes */
#define OMARK         0200
#define OBREAK        0201
#define ONOP          0202
#define ONOECHO       0203
#define OECHO         0204
#define OHIDE         0205

/* New telnet (IAC) commands */
#define IAC   0377
#define DONT  0376
#define DO    0375
#define WONT  0374
#define WILL  0373
#define SB    0372
#define GA    0371
#define EL    0370
#define EC    0367
#define AYT   0366
#define AO    0365
#define IP    0364
#define BRK   0363
#define MARK  0362
#define NOP   0361
#define SE    0360

/* Telnet options */
#define OPT_BINARY              0
#define OPT_ECHO                1
#define OPT_SUPPRESS_GO_AHEAD   3

#endif /* NCP_H */
