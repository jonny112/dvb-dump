
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

int main(int argc, char **argv) {
    int n, fe, dmx, dvr, freq, srate, pids[16];
    char sys, pol, band, pat, pos, fec, mod;
    uint16_t tsval;
    struct dvb_frontend_info fe_inf;
    struct dtv_properties props;
    struct dtv_property prop;
    struct dmx_pes_filter_params flt;
    fe_status_t fe_stat;
    char buff[1316];
   
    fprintf(stderr, "DVB-Dump v0.1.2\n");
    if (argc < 7) {
        fprintf(stderr, "Usage: dvb-dump <system> <pol/band> <freq> <rate> <FEC> <modulation> [<PIDs>..16]\n");
        return 1;
    }
    
    // parse parameters
    if (strcmp(argv[1], "S") == 0) sys = SYS_DVBS;
    else if (strcmp(argv[1], "S2") == 0) sys = SYS_DVBS2;
    else sys = -1;
    
    pol = -1; band = -1;
    if (strlen(argv[2]) == 2) {
        if (argv[2][0] == 'V') pol = SEC_VOLTAGE_13;
        else if (argv[2][0] == 'H') pol = SEC_VOLTAGE_18;
        
        if (argv[2][1] == 'L') band = SEC_TONE_OFF;
        else if (argv[2][1] == 'H') band = SEC_TONE_ON;
    }
    
    freq = atoi(argv[3]);
    srate = atoi(argv[4]);
    fec = atoi(argv[5]);
    mod = atoi(argv[6]);
    
    // open files
    if ((fe = open("/dev/dvb/card0/frontend0", O_RDWR)) < 0) {
        perror("Failed to open frontend");
        return 1;
    }
    
    ioctl(fe, FE_GET_INFO, &fe_inf);
    fprintf(stderr, "Tuner: %s (0x%08X)\n", fe_inf.name, fe_inf.caps);
    
    if ((dmx = open("/dev/dvb/card0/demux0", O_RDWR)) < 0) {
        perror("Failed to open demuxer");
        return 1;
    }
    
    if ((dvr = open("/dev/dvb/card0/dvr0", O_RDONLY)) < 0) {
        perror("Failed to open dvr");
        return 1;
    }
    
    // tune channel
    fprintf(stderr, "Tuning to: ");
    if (sys > -1) fprintf(stderr, "%s, ", (sys == SYS_DVBS2 ? "DVB-S2" : "DVB-S"));
    fprintf(stderr, "%.3fMHz, %dkS/s", (float)freq / 1000, srate);
    if (pol > -1) fprintf(stderr, ", %s", (pol == SEC_VOLTAGE_18 ? "horizontal" : "vetical"));
    if (band > -1) fprintf(stderr, ", %s", (band == SEC_TONE_ON ? "high-band" : "low-band"));
    fprintf(stderr, "\n");
    
    props.num = 1;
    props.props = &prop;
        
    prop.cmd = DTV_CLEAR;
    prop.u.data = sys;
    ioctl(fe, FE_SET_PROPERTY, &props);    
    
    if (sys > -1) {
        prop.cmd = DTV_DELIVERY_SYSTEM;
        prop.u.data = sys;
        ioctl(fe, FE_SET_PROPERTY, &props);
    }
    
    if (pol > -1) ioctl(fe, FE_SET_VOLTAGE, pol);
    if (band > -1) ioctl(fe, FE_SET_TONE, band);
    
    struct dtv_property tune_props[] = {
        { .cmd = DTV_FREQUENCY, .u.data = freq },
        { .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
        { .cmd = DTV_SYMBOL_RATE, .u.data = srate * 1000 },
        { .cmd = DTV_INNER_FEC, .u.data = fec },
        { .cmd = DTV_MODULATION, .u.data = mod },
        { .cmd = DTV_PILOT, .u.data = PILOT_AUTO },
        { .cmd = DTV_ROLLOFF, .u.data = ROLLOFF_AUTO },
        { .cmd = DTV_TUNE }
    };
    props.num = 8;
    props.props = tune_props;
    ioctl(fe, FE_SET_PROPERTY, &props);
    
    fprintf(stderr, "Waiting for lock...\n");
    sleep(3);
    
    // tuner status
    ioctl(fe, FE_READ_STATUS, &fe_stat);
    fprintf(stderr, "Tuner status is 0x%02X (%s)\n", fe_stat, (fe_stat & FE_HAS_LOCK ? "OK" : "FAILED"));
    if (! (fe_stat & FE_HAS_LOCK)) return 1;
    
    // demux
    fprintf(stderr, "Demuxing PID 0\n");
    memset(&flt, 0, sizeof(flt));
    flt.pid = 0;
    flt.pes_type = DMX_PES_OTHER;
    flt.input = DMX_IN_FRONTEND;
    flt.output = DMX_OUT_TS_TAP;
    flt.flags = DMX_IMMEDIATE_START;
    if (ioctl(dmx, DMX_SET_PES_FILTER, &flt) < 0) {
        perror("Failed to setup demuxer");
        return 1;
    }
    
    fprintf(stderr, "Waiting for PAT...\n");
    pat = 0;
    while (! pat) {
        read(dvr, buff, 188);
        if (buff[0] == '\x47' && buff[1] == '\x40' && buff[2] == '\x00' && buff[4] == 0 && buff[5] == 0) {
            tsval = ntohs(*(uint16_t*)(buff + 8));
            fprintf(stderr, "TSID: 0x%04X (%d)\n", tsval, tsval);
            
            pos = 13;
            while (pos < 188 && pos < (buff[7] + 4)) {
                tsval = ntohs(*(uint16_t*)(buff + pos));
                fprintf(stderr, "SID: 0x%04X (%d)", tsval, tsval);
                tsval = ntohs(*(uint16_t*)(buff + pos + 2)) & 0x1FFF;
                fprintf(stderr, " -> PMT: 0x%04X (%d)\n", tsval, tsval);
                pos += 4;
            }
            
            write(1, buff, 188);
            pat = 1;
        }
    }
    
    for (n = 0; n < 16 && (n + 7) < argc; n++) {
        memset(&flt, 0, sizeof(flt));
        flt.pid = strtol(argv[n + 7], NULL, 0);
        flt.pes_type = DMX_PES_OTHER;
        flt.input = DMX_IN_FRONTEND;
        flt.output = DMX_OUT_TS_TAP;
        flt.flags = DMX_IMMEDIATE_START;
        fprintf(stderr, "Demuxing PID 0x%04X (%d)\n", flt.pid, flt.pid);
        
        if ((pids[n] = open("/dev/dvb/card0/demux0", O_RDWR)) < 0) {
            perror("Failed to open demuxer");
            return 1;
        }
        
        if (ioctl(pids[n], DMX_SET_PES_FILTER, &flt) < 0) {
            perror("Failed to setup demuxer");
            return 1;
        }
    }
    
    fprintf(stderr, "Running...\n");
    while (1) {
        read(dvr, buff, sizeof(buff));
        write(1, buff, sizeof(buff));
    }
}
