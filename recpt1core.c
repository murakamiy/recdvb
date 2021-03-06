#include <stdio.h>
#include <stdlib.h>
#include "recpt1core.h"
#include "version.h"
#include <sys/poll.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include "channeldata.h"

#define ISDB_T_NODE_LIMIT 24        // 32:ARIB limit 24:program maximum
#define ISDB_T_SLOT_LIMIT 8

/* globals */
boolean f_exit = FALSE;
char  bs_channel_buf[8];

static int fefd = 0;
static int dmxfd = 0;

int
close_tuner(thread_data *tdata)
{
    int rv = 0;

    if(fefd > 0){
      close(fefd);
      fefd = 0;
    }
    if(dmxfd > 0){
      close(dmxfd);
      dmxfd = 0;
    }
    if(tdata->tfd == -1)
        return rv;

    close(tdata->tfd);
    tdata->tfd = -1;

    free(tdata->device_terrestrial.device_no);
    free(tdata->device_satellite.device_no);

    return rv;
}

void
calc_cn(void)
{
      int strength=0;
      ioctl(fefd, FE_READ_SNR, &strength);
      fprintf(stderr,"SNR: %d\n",strength);
}

int
parse_time(char *rectimestr, int *recsec)
{
    /* indefinite */
    if(!strcmp("-", rectimestr)) {
        *recsec = -1;
        return 0;
    }
    /* colon */
    else if(strchr(rectimestr, ':')) {
        int n1, n2, n3;
        if(sscanf(rectimestr, "%d:%d:%d", &n1, &n2, &n3) == 3)
            *recsec = n1 * 3600 + n2 * 60 + n3;
        else if(sscanf(rectimestr, "%d:%d", &n1, &n2) == 2)
            *recsec = n1 * 3600 + n2 * 60;
        else
            return 1; /* unsuccessful */

        return 0;
    }
    /* HMS */
    else {
        char *tmpstr;
        char *p1, *p2;
        int  flag;

        if( *rectimestr == '-' ){
	        rectimestr++;
	        flag = 1;
	    }else
	        flag = 0;
        tmpstr = strdup(rectimestr);
        p1 = tmpstr;
        while(*p1 && !isdigit(*p1))
            p1++;

        /* hour */
        if((p2 = strchr(p1, 'H')) || (p2 = strchr(p1, 'h'))) {
            *p2 = '\0';
            *recsec += atoi(p1) * 3600;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* minute */
        if((p2 = strchr(p1, 'M')) || (p2 = strchr(p1, 'm'))) {
            *p2 = '\0';
            *recsec += atoi(p1) * 60;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* second */
        *recsec += atoi(p1);
        if( flag )
        	*recsec *= -1;

        free(tmpstr);

        return 0;
    } /* else */

    return 1; /* unsuccessful */
}

void
do_bell(int bell)
{
    int i;
    for(i=0; i < bell; i++) {
        fprintf(stderr, "\a");
        usleep(400000);
    }
}

int lookup_channel(int channel_id, channel_info *output, channel_info *input, int size)
{
    int i;

    for (i = 0; i < size; i++) {
        if (channel_id == input[i].id) {
            memcpy(output, &input[i], sizeof(channel_info));
            return TRUE;
        }
    }

    return FALSE;
}

void set_tuner_device(thread_data *tdata, char *satellite, char *terrestrial)
{
    if (satellite == NULL) {
        tdata->device_satellite.count = 2;
        tdata->device_satellite.device_no = malloc(sizeof(int) * 2);
        tdata->device_satellite.device_no[0] = 0;
        tdata->device_satellite.device_no[1] = 1;
    }
    else {

    }

    if (terrestrial == NULL) {
        tdata->device_terrestrial.count = 2;
        tdata->device_terrestrial.device_no = malloc(sizeof(int) * 2);
        tdata->device_terrestrial.device_no[0] = 2;
        tdata->device_terrestrial.device_no[1] = 3;
    }
    else {

    }
}

int open_frontend_device(int dev_num) {
    char device[32];
    int ret = FALSE;

    /* open frontend device */
    if (fefd == 0) {
        sprintf(device, "/dev/dvb/adapter%d/frontend0", dev_num);
        fefd = open(device, O_RDWR);

        if (fefd < 0) {
            fefd = 0;
            ret = FALSE;
        }
        else {
            ret = TRUE;
            fprintf(stderr, "device = %s\n", device);
        }
    }

    return ret;
}

/* from checksignal.c */
int
tune(char *channel, thread_data *tdata, int dev_num)
{
    struct dtv_property prop[3];
    struct dtv_properties props;
    struct dvb_frontend_info fe_info;
    int fe_freq;
    int rc, i;
    struct dmx_pes_filter_params filter;
    struct dvb_frontend_event event;
    struct pollfd pfd[1];
    char device[32];
    int channel_id;
    channel_info channel_info = { 0 };
    isdb_type isdb = ISDB_TERRESTRIAL;
    tuner_device *tuner = NULL;
    int open_status = FALSE;

    if (strncmp("BS_", channel, 3) == 0) {
        isdb = ISDB_SATELLITE_BS;
    }
    else if (strncmp("CS_", channel, 3) == 0) {
        isdb = ISDB_SATELLITE_CS;
    }
    else {
        isdb = ISDB_TERRESTRIAL;
    }

    if (dev_num == -1) {
        if (isdb == ISDB_TERRESTRIAL) {
            tuner = &tdata->device_terrestrial;
        }
        else {
            tuner = &tdata->device_satellite;
        }

        for (i = 0; i < tuner->count; i++) {
            open_status = open_frontend_device(tuner->device_no[i]);
            if (open_status == TRUE) {
                dev_num = tuner->device_no[i];
                break;
            }
        }
        if (open_status == FALSE) {
            fprintf(stderr, "cannot open frontend device\n");
            return 1;
        }
    }
    else {
        if (open_frontend_device(dev_num) == FALSE) {
            fprintf(stderr, "cannot open frontend device\n");
            return 1;
        }
    }

    if ( (ioctl(fefd,FE_GET_INFO, &fe_info) < 0)){
      fprintf(stderr, "FE_GET_INFO failed\n");
      return 1;
    }

    if (fe_info.type == FE_QPSK) {
        channel_id = atoi(&channel[3]);
        if (isdb == ISDB_SATELLITE_BS) {
            if (FALSE == lookup_channel(channel_id, &channel_info, bs_data, ARRAY_SIZE(bs_data))) {
                fprintf(stderr, "channel:%s is not found\n", channel);
                return 1;
            }
        }
        else if (isdb == ISDB_SATELLITE_CS) {
            if (FALSE == lookup_channel(channel_id, &channel_info, cs_data, ARRAY_SIZE(cs_data))) {
                fprintf(stderr, "channel:%s is not found\n", channel);
                return 1;
            }
        }
    }
    else if (fe_info.type == FE_OFDM) {
        if( (fe_freq = atoi(channel)) == 0){
          fprintf(stderr, "fe_freq is not number\n");
          return 1;
        }
        channel_info.frequency = (fe_freq * 6000 + 395143) * 1000;
        channel_info.ts_id = 0;
    }
    else {
        fprintf(stderr, "Unknown type of adapter\n");
        return 1;
    }
    fprintf(stderr,"Using DVB card \"%s\"\n",fe_info.name);


    prop[0].cmd = DTV_FREQUENCY;
    prop[0].u.data = channel_info.frequency;
#ifdef DTV_STREAM_ID
    prop[1].cmd = DTV_STREAM_ID;
#else
    prop[1].cmd = DTV_ISDBS_TS_ID;
#endif
    prop[1].u.data = channel_info.ts_id;
    prop[2].cmd = DTV_TUNE;
    fprintf(stderr,"tuning to %d kHz\n",prop[0].u.data / 1000);

    props.props = prop;
    props.num = 3;

    if (ioctl(fefd, FE_SET_PROPERTY, &props) == -1) {
      perror("ioctl FE_SET_PROPERTY\n");
      return 1;
    }

    pfd[0].fd = fefd;
    pfd[0].events = POLLIN;
    event.status=0;
    fprintf(stderr,"polling");
    for (i = 0; (i < 5) && ((event.status & FE_TIMEDOUT)==0) && ((event.status & FE_HAS_LOCK)==0); i++) {
      fprintf(stderr,".");
      if (poll(pfd,1,5000)){
        if (pfd[0].revents & POLLIN){
          if ((rc = ioctl(fefd, FE_GET_EVENT, &event)) < 0){
            if (errno != EOVERFLOW) {
              perror("ioctl FE_GET_EVENT");
              fprintf(stderr,"status = %d\n", rc);
              fprintf(stderr,"errno = %d\n", errno);
              return -1;
            }
            else fprintf(stderr,"\nOverflow error, trying again (status = %d, errno = %d)", rc, errno);
          }
        }
      }
    }
    

    if ((event.status & FE_HAS_LOCK)==0) {
      fprintf(stderr, "\nCannot lock to the signal on the given channel\n");
      return 1;
    } else fprintf(stderr, "ok\n");

    if(dmxfd == 0){
      sprintf(device, "/dev/dvb/adapter%d/demux0", dev_num);
      if((dmxfd = open(device,O_RDWR)) < 0){
        dmxfd = 0;
        fprintf(stderr, "cannot open demux device\n");
        return 1;
      }
    }

    filter.pid = 0x2000;
    filter.input = DMX_IN_FRONTEND;
    filter.output = DMX_OUT_TS_TAP;
//    filter.pes_type = DMX_PES_OTHER;
    filter.pes_type = DMX_PES_VIDEO;
    filter.flags = DMX_IMMEDIATE_START;
    if (ioctl(dmxfd, DMX_SET_PES_FILTER, &filter) == -1) {
      fprintf(stderr,"FILTER %i: ", filter.pid);
      perror("ioctl DMX_SET_PES_FILTER");
      close(dmxfd);
      dmxfd = 0;
      return 1;
    }

    if(tdata->tfd < 0){
      sprintf(device, "/dev/dvb/adapter%d/dvr0", dev_num);
      if((tdata->tfd = open(device,O_RDONLY)) < 0){
//      if((tdata->tfd = open(device,O_RDONLY|O_NONBLOCK)) < 0){
        fprintf(stderr, "cannot open dvr device\n");
        close(dmxfd);
        dmxfd = 0;
        return 1;
      }
    }

    if(!tdata->tune_persistent) {
        /* show signal strength */
        calc_cn();
    }

    return 0; /* success */
}
