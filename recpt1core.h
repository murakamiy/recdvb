/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#ifndef _RECPT1_UTIL_H_
#define _RECPT1_UTIL_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//#include "pt1_ioctl.h"
#include "config.h"
#include "decoder.h"
#include "recpt1.h"
#include "mkpath.h"
#include "tssplitter_lite.h"

/* ipc message size */
#define MSGSZ     255

/* used in checksigna.c */
#define MAX_RETRY (2)

/* type definitions */
typedef int boolean;

typedef struct sock_data {
    int sfd;    /* socket fd */
    struct sockaddr_in addr;
} sock_data;

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

typedef struct tuner_device {
    int *device_no; // array of device no
    int count;      // count of array
} tuner_device;

typedef struct thread_data {

    tuner_device device_satellite;
    tuner_device device_terrestrial;

    int tfd;    /* tuner fd */ //xxx variable

    int wfd;    /* output file fd */ //invariable
    int lnb;    /* LNB voltage */ //invariable
    int msqid; //invariable
    time_t start_time; //invariable

    int recsec; //xxx variable

    boolean indefinite; //invaliable
    boolean tune_persistent; //invaliable

    QUEUE_T *queue; //invariable
    sock_data *sock_data; //invariable
    pthread_t signal_thread; //invariable
    decoder *decoder; //invariable
    decoder_options *dopt; //invariable
    splitter *splitter; //invariable
} thread_data;

typedef enum isdb_type {
    ISDB_SATELLITE_BS = 0,
    ISDB_SATELLITE_CS,
    ISDB_TERRESTRIAL
} isdb_type;

typedef struct channel_info {
    int id;
    const char *name;
    unsigned int frequency; // freqno or freq (terrestrial Hz, BSCS110 kHz)
    unsigned int ts_id;	// slotid or tsid
} channel_info;

extern const char *version;
extern char *bsdev[];
extern char *isdb_t_dev[];

extern boolean f_exit;

/* prototypes */
int tune(char *channel, thread_data *tdata, int dev_num);
int close_tuner(thread_data *tdata);
void calc_cn(void);
int parse_time(char *rectimestr, int *recsec);
void do_bell(int bell);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
int lookup_channel(int channel_id, channel_info *output, channel_info *input, int size);
void set_tuner_device(thread_data *tdata, char *satellite, char *terrestrial);
int open_frontend_device(int dev_num);

#endif
