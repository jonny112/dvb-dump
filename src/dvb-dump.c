 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#define FRAME_SIZE 188
#define DEFAULT_FRONTEND "/dev/dvb/adapter0/frontend0"
#define DEFAULT_DEMUX "/dev/dvb/adapter0/demux0"
#define DEFAULT_DVR "/dev/dvb/adapter0/dvr0"

#ifdef ANCIENT_GLIB2_0
__asm__(".symver fprintf,fprintf@GLIBC_2.0");
#endif
#ifdef ANCIENT_GLIB2_2
__asm__(".symver fprintf,fprintf@GLIBC_2.2");
#endif


int main(int argc, char **argv) {
    int n, fe, dmx, dvr, freq, srate, pids[16];
    char pol, band, pat, pos, fec, mod, rolloff;
    uint16_t tsval, snr, strength;
    uint32_t ber;
    struct dvb_frontend_info fe_inf;
    struct dvb_frontend_parameters frontp;
#ifndef LEGACY_API
    int sys;
    struct dtv_properties props;
#endif
    struct dmx_pes_filter_params flt;
    fe_status_t fe_stat;
    int buffer_frames = 1024;
    int dmx_buffer_frames = 16732;
    char *buff;
    
    fprintf(stderr, "DVB-Dump v0.3.0\n");
    if (argc < 7) {
        fprintf(stderr, "\nUsage: dvb-dump <system> <polarisation/band[/option/positon]> <frequency> <rate/bandwidth> <FEC> <modulation> <roll-off> [<PIDs>..16]\n\nValid systems are S, S2, and T.\nPolarisation can be VL, VH, HL, HH, or - for none. DiSEqC option and position can be A or B.\nFrequencies for satellites in kHz (950-2150MHz, LO: 9750MHz for L, 10600MHz for H), terrestrial in Hz.\nUp to 16 PIDs may be specified for demuxing. PID 0 is always demuxed and 0x2000 will grab the entire stream, if supported.\n\n");
        
        fprintf(stderr, "fe_code_rate:  FEC_NONE=%d, FEC_1_2=%d, FEC_2_3=%d, FEC_3_4=%d, FEC_4_5=%d, FEC_5_6=%d, FEC_6_7=%d, FEC_7_8=%d, FEC_8_9=%d,\n               FEC_AUTO=%d, FEC_3_5=%d, FEC_9_10=%d, FEC_2_5=%d\n\n", FEC_NONE, FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, FEC_6_7, FEC_7_8, FEC_8_9, FEC_AUTO, FEC_3_5, FEC_9_10, FEC_2_5);
        
        fprintf(stderr, "fe_modulation: QPSK=%d, QAM_16=%d, QAM_32=%d, QAM_64=%d, QAM_128=%d, QAM_256=%d, QAM_AUTO=%d, VSB_8=%d, VSB_16=%d, PSK_8=%d,\n               APSK_16=%d, APSK_32=%d, DQPSK=%d, QAM_4_NR=%d\n\n", QPSK, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256, QAM_AUTO, VSB_8, VSB_16, PSK_8, APSK_16, APSK_32, DQPSK, QAM_4_NR);
        
        fprintf(stderr, "fe_rolloff:    ROLLOFF_35=%d, ROLLOFF_20=%d, ROLLOFF_25=%d, ROLLOFF_AUTO=%d\n\n", ROLLOFF_35, ROLLOFF_20, ROLLOFF_25, ROLLOFF_AUTO);
        
        fprintf(stderr, "Environment parameters:\nDVB_FRONTEND: frontend device (default: %s)\nDVB_DEMUX: demux device (default: %s)\nDVB_DVR: streaming device (default: %s)\nDVB_BUFFER_FRAMES: size of the stream buffer in frames (default: %d)\nDVB_DEMUX_BUFFER_FRAMES: size of the demux buffer in frames (default: %d)\n\n", DEFAULT_FRONTEND, DEFAULT_DEMUX, DEFAULT_DVR, buffer_frames, dmx_buffer_frames);
        
        return 1;
    }
    
    if (getenv("DVB_FRONTEND") != NULL && getenv("DVB_FRONTEND")[0] == '\0') {
        fprintf(stderr, "Skipping tuning!\n");
    } else {
        // parse parameters
    #ifndef LEGACY_API
        if (strcmp(argv[1], "S") == 0) sys = SYS_DVBS;
        else if (strcmp(argv[1], "S2") == 0) sys = SYS_DVBS2;
        else if (strcmp(argv[1], "T") == 0) sys = SYS_DVBT;
        else sys = CHAR_MAX;
    #endif
        
        pol = CHAR_MAX; band = CHAR_MAX;
        if (strlen(argv[2]) > 1) {
            if (argv[2][0] == 'V') pol = SEC_VOLTAGE_13;
            else if (argv[2][0] == 'H') pol = SEC_VOLTAGE_18;
            
            if (argv[2][1] == 'L') band = SEC_TONE_OFF;
            else if (argv[2][1] == 'H') band = SEC_TONE_ON;
        }
        
        freq = atoi(argv[3]);
        srate = atoi(argv[4]);
        fec = atoi(argv[5]);
        mod = atoi(argv[6]);
        rolloff = atoi(argv[7]);
        
        // open files
        if ((fe = open(getenv("DVB_FRONTEND") == NULL ? DEFAULT_FRONTEND : getenv("DVB_FRONTEND"), O_RDWR)) < 0) {
            perror("Failed to open frontend");
            return 1;
        }
        
        memset(&fe_inf, 0, sizeof(fe_inf));
        if (ioctl(fe, FE_GET_INFO, &fe_inf) < 0) perror("FE_GET_INFO");
        fprintf(stderr, "Tuner: %s (0x%08X)\n", fe_inf.name, fe_inf.caps);
        
        // tune channel
        fprintf(stderr, "Tuning to: ");
    #ifndef LEGACY_API
        if (sys < CHAR_MAX) fprintf(stderr, "%s, ", (sys == SYS_DVBT ? "DVB-T" : sys == SYS_DVBS2 ? "DVB-S2" : "DVB-S"));
    #endif
        if (sys == SYS_DVBT) fprintf(stderr, "%.3fMHz (%.3fMHz bandwidth)", (float)freq / 1000000, (float)srate / 1000000); else fprintf(stderr, "%.3fMHz, %dkS/s", (float)freq / 1000, srate);
        if (pol < CHAR_MAX) fprintf(stderr, ", %s", (pol == SEC_VOLTAGE_18 ? "horizontal" : "vetical"));
        if (band < CHAR_MAX) fprintf(stderr, ", %s", (band == SEC_TONE_ON ? "high-band" : "low-band"));
        fprintf(stderr, "\n");
        
        if (pol < CHAR_MAX) if (ioctl(fe, FE_SET_VOLTAGE, pol) < 0) perror("FE_SET_VOLTAGE");
        if (band < CHAR_MAX) if (ioctl(fe, FE_SET_TONE, band) < 0) perror("FE_SET_TONE");
        
    #ifndef LEGACY_API
        if (sys < 0) {
    #endif
            // legacy tuning API
            frontp.frequency = freq;
            frontp.u.qpsk.fec_inner = fec;
            frontp.u.qpsk.symbol_rate = srate * 1000;
            frontp.inversion = INVERSION_AUTO;
            if (ioctl(fe, FE_SET_FRONTEND, &frontp) < 0) perror("FE_SET_FRONTEND");
    #ifndef LEGACY_API
        } else {
            // new tuning API
            if (sys == SYS_DVBT) {
                // DVB-T
                struct dtv_property tune_props[] = {
                    { .cmd = DTV_CLEAR },
                    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = sys },
                    { .cmd = DTV_FREQUENCY, .u.data = freq },
                    { .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
                    { .cmd = DTV_CODE_RATE_LP, .u.data = FEC_NONE },
                    { .cmd = DTV_CODE_RATE_HP, .u.data = fec },
                    { .cmd = DTV_MODULATION, .u.data = mod },
                    { .cmd = DTV_TRANSMISSION_MODE, .u.data = TRANSMISSION_MODE_AUTO },
                    { .cmd = DTV_GUARD_INTERVAL, .u.data = GUARD_INTERVAL_AUTO },
                    { .cmd = DTV_HIERARCHY, .u.data = HIERARCHY_AUTO },
                    { .cmd = DTV_BANDWIDTH_HZ, .u.data =  srate },
                    { .cmd = DTV_TUNE }
                };
                props.num = 12;
                props.props = tune_props;
            } else {
                // DVB-S
                struct dtv_property tune_props[] = {
                    { .cmd = DTV_CLEAR },
                    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = sys },
                    { .cmd = DTV_FREQUENCY, .u.data = freq },
                    { .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
                    { .cmd = DTV_SYMBOL_RATE, .u.data = srate * 1000 },
                    { .cmd = DTV_INNER_FEC, .u.data = fec },
                    { .cmd = DTV_MODULATION, .u.data = mod },
                    { .cmd = DTV_PILOT, .u.data = PILOT_AUTO },
                    { .cmd = DTV_ROLLOFF, .u.data = rolloff },
                    { .cmd = DTV_TUNE }
                };
                props.num = 10;
                props.props = tune_props;
            }
            if (ioctl(fe, FE_SET_PROPERTY, &props) < 0) perror("FE_SET_PROPERTY");
        }
    #endif
        
        if (strlen(argv[2]) > 3) {
            struct dvb_diseqc_master_cmd diseqc;
            diseqc.msg[0] = 0xE0;
            diseqc.msg[1] = 0x10;
            diseqc.msg[2] = 0x38;
            diseqc.msg[3] = 0xF0;
            if (argv[2][2] == 'B') diseqc.msg[3] |= 8;
            if (argv[2][3] == 'B') diseqc.msg[3] |= 4;
            if (argv[2][0] == 'H') diseqc.msg[3] |= 2;
            if (argv[2][1] == 'H') diseqc.msg[3] |= 1;
            diseqc.msg_len = 4;
            fprintf(stderr, "Switching DiSEqC port group 0 to 0x%02X...\n", diseqc.msg[3]);
            usleep(100000);
            if (ioctl(fe, FE_DISEQC_SEND_MASTER_CMD, &diseqc) < 0) perror("FE_DISEQC_SEND_MASTER_CMD");
        }
        
        //diseqc.msg[2] = 0x27;
        //diseqc.msg_len = 3;
        //if (ioctl(fe, FE_DISEQC_SEND_MASTER_CMD, &diseqc) < 0) perror("FE_DISEQC_SEND_MASTER_CMD");
        //diseqc.msg[2] = 0x22;
        //if (ioctl(fe, FE_DISEQC_SEND_MASTER_CMD, &diseqc) < 0) perror("FE_DISEQC_SEND_MASTER_CMD");
        
        fprintf(stderr, "Waiting for lock...\n");
        sleep(3);
        
        // tuner status
        do {
            memset(&fe_stat, 0, sizeof(fe_stat));
            if (ioctl(fe, FE_READ_STATUS, &fe_stat) < 0) perror("FE_READ_STATUS");
            if (ioctl(fe, FE_READ_SIGNAL_STRENGTH, &strength) < 0) perror("FE_READ_SIGNAL_STRENGTH");
            if (ioctl(fe, FE_READ_SNR, &snr) < 0) perror("FE_READ_SNR");
            if (ioctl(fe, FE_READ_BER, &ber) < 0) perror("FE_READ_BER");
            fprintf(stderr, "Tuner status is 0x%02X (%s)  Signal: %.1f%% (%d)  SNR: %.1f%% (%d)  BER: %d\n", fe_stat, (fe_stat & FE_HAS_LOCK ? "OK" : "FAILED"), (float)strength / 0xFFFF * 100, strength, (float)snr / 0xFFFF * 100, snr, ber);
            usleep(200000);
        } while (argc > 8 && argv[8][0] == '-');
        if (! (fe_stat & FE_HAS_LOCK)) return 1;
    }
    
    if ((dmx = open(getenv("DVB_DEMUX") == NULL ? DEFAULT_DEMUX : getenv("DVB_DEMUX"), O_RDWR)) < 0) {
        perror("Failed to open demuxer");
        return 1;
    }
        
    if ((dvr = open(getenv("DVB_DVR") == NULL ? DEFAULT_DVR : getenv("DVB_DVR"), O_RDONLY)) < 0) {
        perror("Failed to open dvr");
        return 1;
    }
    
    
    if (getenv("DVB_BUFFER_FRAMES") != NULL) {
        buffer_frames = atoi(getenv("DVB_BUFFER_FRAMES"));
        fprintf(stderr, "Stream buffer set to %d frames.\n", buffer_frames);
    }
    
    buff = malloc(FRAME_SIZE * buffer_frames);
    
    if (getenv("DVB_DEMUX_BUFFER_FRAMES") != NULL) {
        dmx_buffer_frames = atoi(getenv("DVB_DEMUX_BUFFER_FRAMES"));
        fprintf(stderr, "Demux buffer set to %d frames.\n", dmx_buffer_frames);
    }
    
    if (ioctl(dmx, DMX_SET_BUFFER_SIZE, FRAME_SIZE * dmx_buffer_frames) < 0) {
        perror("Failed to set demux buffer size");
        return 1;
    }
    
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
        read(dvr, buff, FRAME_SIZE);
        if (buff[0] == '\x47' && buff[1] == '\x40' && buff[2] == '\x00' && buff[4] == 0 && buff[5] == 0) {
            tsval = ntohs(*(uint16_t*)(buff + 8));
            fprintf(stderr, "TSID: 0x%04X (%d)\n", tsval, tsval);
            
            pos = 13;
            while (pos < FRAME_SIZE && pos < (buff[7] + 4)) {
                tsval = ntohs(*(uint16_t*)(buff + pos));
                fprintf(stderr, "SID: 0x%04X (%d)", tsval, tsval);
                tsval = ntohs(*(uint16_t*)(buff + pos + 2)) & 0x1FFF;
                fprintf(stderr, " -> PMT: 0x%04X (%d)\n", tsval, tsval);
                pos += 4;
            }
            
            write(1, buff, FRAME_SIZE);
            pat = 1;
        }
    }
    
    for (n = 0; n < 16 && (n + 8) < argc; n++) {
        memset(&flt, 0, sizeof(flt));
        flt.pid = strtol(argv[n + 8], NULL, 0);
        flt.pes_type = DMX_PES_OTHER;
        flt.input = DMX_IN_FRONTEND;
        flt.output = DMX_OUT_TS_TAP;
        flt.flags = DMX_IMMEDIATE_START;
        fprintf(stderr, "Demuxing PID 0x%04X (%d)\n", flt.pid, flt.pid);
        
        if ((pids[n] = open(getenv("DVB_DEMUX") == NULL ? DEFAULT_DEMUX : getenv("DVB_DEMUX"), O_RDWR)) < 0) {
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
        read(dvr, buff, FRAME_SIZE * buffer_frames);
        write(1, buff, FRAME_SIZE * buffer_frames);
    }
}
