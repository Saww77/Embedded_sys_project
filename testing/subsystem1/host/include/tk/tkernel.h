#ifndef HOST_KERNEL_H
#define HOST_KERNEL_H
#include <stddef.h>
#define IMPORT extern
#define EXPORT
#define E_NOSPT (-9)
typedef void (*FP)(void);
typedef int PRI;
typedef unsigned UW;
typedef unsigned char UB;
typedef int ID;
typedef int INT;
typedef int ER;
typedef struct { unsigned mbfatr; unsigned bufsz; int maxmsz; } T_CMBF;
#define E_OK 0
#define E_PAR (-17)
#define E_OBJ (-41)
#define E_TMOUT (-50)
#define E_NOMEM (-33)
#define TA_TFIFO 0
#define TMO_POL 0
ER tk_dly_tsk(unsigned delay);
ID tk_cre_mbf(const T_CMBF *config);
ER tk_del_mbf(ID id);
ER tk_snd_mbf(ID id, const void *message, INT length, int timeout);
INT tk_rcv_mbf(ID id, void *message, int timeout);
#endif
