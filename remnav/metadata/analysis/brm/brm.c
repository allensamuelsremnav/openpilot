#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define MATCH 0
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MAX_MD_LINE_SIZE 1000
#define MAX_TD_LINE_SIZE 1000
#define MAX_SD_LINE_SIZE 1000
#define MAX_LD_LINE_SIZE 1000
#define CAPTURE_DURATION 20 // minutes
#define FD_BUFFER_SIZE (CAPTURE_DURATION*60*30)
#define MD_BUFFER_SIZE (CAPTURE_DURATION*60*1000)
#define PR_BUFFER_SIZE (CAPTURE_DURATION*60*100)
#define TX_BUFFER_SIZE (CAPTURE_DURATION*60*1000)
#define SD_BUFFER_SIZE (CAPTURE_DURATION*60*1000)
#define LD_BUFFER_SIZE (CAPTURE_DURATION*60*1000)
#define MAX_SPIKES 5000
#define MAX_FILES 100
#define FRAME_DURATION 33.34
#define GOOD_QUALITY 0
#define POOR_QUALITY 1
#define IN_SERVICE 1
#define NOT_IN_SERVICE 0
#define BR_REDUCTION_THRESHOLD  15
#define BR_INCREASE_THRESHOLD   5

struct s_files {
    char input_directory[500];
    char rx_prefix[500];                // per carrier meta data file name
    char tx_prefix[500];                // uplink log file name
    char pr_prefix[500];                // probe log file prefix
    char sr_prefix[500];                // service log file prefix
    char la_prefix[500];                // latency log file prefix
};

struct s_service { // service log file
    // CH: 2, change to out-of-service state, latency: 0, latencyTime: 0, estimated latency: 30, 
    // stop_sending flag: 1 , uplink queue size: 17, zeroUplinkQueue: 0, service flag: 0, 
    // numCHOut: 1, Time: 1673558308240
    int channel;                // 0=att, 1=vz, 2=tmobile
    int state;                  // 1=IN-SERVICE 0=OUT-OF-SERVICE
    double state_transition_epoch_ms; // time stamp of state transition
    int bp_t2r;                 // back propagated t2r at the state_transition_epoch_ms
    double bp_t2r_epoch_ms;     // time when this back propagated info was received
    int bp_packet_num;          // packet number whose t2r was back propagated
    int est_t2r;                // estimated t2r at state_transition_epoch_ms
    int socc;                   // sampled occupancy at state_transition_epoch_ms
};

struct s_dedup {      // dedup meta data
    int         packet_num;                 // incrementing number starting from 0
    double      camera_epoch_ms;            // camera timestamp of the frame this packet belogs to
    double      vx_epoch_ms;                // encoder output timestamp
    double      tx_epoch_ms;                // transmission (to modem buffer) timestamp
    double      rx_epoch_ms;                // receiver timestamp
    int         socc;                       // sampled occupancy. 31 if no information avaialble from the mdoem
    unsigned    video_packet_len;           // packet length in bytes
    unsigned    frame_start;                // 1-bit flag; set if this packet is the start of a new frame
    unsigned    rolling_frame_number;       // 4-bit rolling frame number
    unsigned    frame_rate;                 // 2-bit field 0: 30Hz, 1: 15Hz, 2: 10_hz, 3: 5Hz
    unsigned    frame_resolution;           // 2-bit field 0: HD (1920x1080), 1: SD (960x540), 2/3: reserved
    unsigned    frame_end;                  // 1-bit flag; set if this packet is t end of the frame
    unsigned    retx;                       // retransmission index for the packet 0/2=Q0/2 1=Q1 (re-tx)
    unsigned    ch;                         // carrirer 0=ATT, 1=VZ, 2=TMobile
    int         kbps;                       // bit-rate of the channel servicing this packet
};

struct s_frame {
    int frame_number;                       // starts with a 1
    struct s_dedup *first_packet;           // first packet of the frame
    struct s_dedup *latest_packet;          // latest received packet for this frame
    int service_state_transition[3];        // 1 if a state transiton occurred in this frame
    int service_state[3];                   // new service state the channel switched to
    double service_state_transition_TS[3];  // service state transition time stamp
    int ch_quality_state_transition[3];     // 1 if quality state transition occurred
    int ch_quality_state[3];                // new quality state the channel switched to
    int socc[3];                            // queue size (occupancy)
    int socc_transition[3];                 // set to 1 if socc was updated
    double socc_transition_TS[3];           // service state transition time stamp
    int enc_quality_state;                  // 0 = enc in high bit rate, 1 otherwise
    int enc_quality_state_transition;       // 1 if a transition occurred during this frame
};

struct s_latency {
    int packet_num;             // packet number
    int channel;                // channel correspoding to this entry
    int t2r_ms;                 // latency for it 
    double bp_epoch_ms;         // time bp info was received by the sendedr
}; 

struct s_txlog {
// uplink_queue. ch: 2, timestamp: 1672344732193, queue_size: 23, elapsed_time_since_last_queue_update: 29, actual_rate: 1545, stop_sending_flag: 0, zeroUplinkQueue_flag: 0, lateFlag: 1
    int channel;                    // channel number 0, 1 or 2
    double epoch_ms;                // time modem occ was sampled
    int occ;                        // occupancy
    int time_since_last_update;     // of occupancy for the same channel
    int actual_rate;
};

struct s_merged_txlog_service {
    struct s_txlog *tdp;            // pointer to an element in td array
    struct s_service *sdp;          // pointer to an element in the sd array 
    int tdp1_sdp0;                  // if 1 then tdp is valid else sdp is valid
};

struct s_carrier {
    int packet_num;                 // packet number read from this line of the carrier meta_data finle 
    int channel;                    // channel for this entry
    double vx_epoch_ms;             // encoder output epoch time after stripping out the embedded occ and v2t delay
    double tx_epoch_ms;             // tx epoch time = encoder epoch + v2t latency
    double rx_epoch_ms;             // rx epoch time
    double ert_epoch_ms;            // expected time to retire
    int socc;                       // sampled occupancy at tx_epoch_ms either from tx log or rx metadata 
    int usocc;                      // unclipped sampled occupancy
    int iocc;                       // interpolated occupancy from tx log at tx_epoch_ms
    double socc_epoch_ms;           // time when the occupacny for this packet was sampled
    int bit_rate_kbps;              // bit rate reported by the modem
    int packet_len;
    int frame_start;
    int frame_num;
    int frame_rate;
    int frame_res;
    int frame_end;
    double camera_epoch_ms;
    int retx;
    int check_packet_num;
    float t2r_latency_ms;           // rx_epoch_ms - tx_epoch_ms
    float r2t_latency_ms;           // bp_epoch_ms - rx_epoch_ms
    float ert_ms;                   // ert_epoch_ms = tx_epoch_ms
    int est_t2r_ms;                 // t2r_ms + tx_epoch_ms + bp_epoch_ms
    int probe;                      // 1=probe packet 0=data packet
    struct s_service *sdp;          // serivce log data 
    struct s_latency *ldp;          // latency log data sorted by bp_epoch_ms
    struct s_latency *lsp;          // latency log data sorted by packet number
};

struct s_latency_table {
        double latency;
        int count; 
};

struct s_lspike {
    int active;                 // 1 if in middle of a spike, 0 otherwise
    int max_occ;                // highest occ during this spike
    double max_latency;         // highest latency during this spike
    int max_lat_packet_num;     // packet number of the packet that suffered maximum latency
    int start_packet_num;       // first packet number in inactive state where the latency exceeded latency_threshold 
    double start_tx_epoch_ms;   // tx time stamp of the starting packet
    struct s_carrier *startp;   // pointer to md data of the statrting packet
        
    int duration_pkt;           // duration in number of packets
    double duration_ms;         // duration in ms
};

struct s_ospike {
    int active;                 // 1 if in middle of a spike, 0 otherwise
    int max_occ;                // highest occ value during this spike
    int start_packet_num;       // first packet number in inactive state where occupancy is greater than threshold
    double start_tx_epoch_ms;   // tx time stamp of the starting packet
    struct s_carrier *startp;   // pointer to md data of the statrting packet

    int duration_pkt;           // duration in number of packets
    double duration_ms;         // duration
}; 

struct s_occ_by_lat_bins {
    int count;                  // count of packets in this latency bin
    double sum;                 // sum of occupancy values in this latency bin
    double squared_sum;         // sum of squared values of occupancy in this latency bin   
};

// globals
int silent = 0;                         // if 1 then dont generate any warnings on the console
FILE *warn_fp = NULL;                   // warnings output file
struct s_dedup *ddmd;                   // dedup metadata storage
struct s_carrier *md;                   // carrier metadata
struct s_carrier *pd;                   // probe log data
struct s_carrier *mpd;                  // combined metada and probe data
struct s_txlog *td;                     // tx log data array
struct s_service *sd;                   // service log data array
struct s_latency *ld;                   // latency log data array sorted by packet_num
struct s_latency *ls;                   // latency log data array sorted by bp_epoch_ms
struct s_merged_txlog_service *tsd;     // merged tx and service log array 
struct s_frame *fd;                     // frame list


#define		FATAL(STR, ARG) {printf (STR, ARG); my_exit(-1);}
#define		WARN(STR, ARG) {if (!silent) printf (STR, ARG);}
#define		FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}

int my_exit (int n) {
    if (ddmd != NULL) free (ddmd); 
    if (md != NULL) free (md); 
    if (td != NULL) free (td); 
    if (pd != NULL) free (pd); 
    if (mpd != NULL) free (mpd); 
    if (sd != NULL) free (sd); 
    if (ld != NULL) free (ld); 
    if (ls != NULL) free (ls); 
    exit (n);
} // my_exit

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (int format, double *vx_epoch_ms, double *tx_epoch_ms, int *socc) {
    double real_vx_epoch_ms;
    double tx_minus_vx; 
    
    // calculate tx-vx and modem occupancy
    if (format == 1) {
        real_vx_epoch_ms = /* starts at bit 8 */ trunc (*vx_epoch_ms/256);
        *socc = 31; // not available
        tx_minus_vx =  /* lower 8 bits */ *vx_epoch_ms - real_vx_epoch_ms*256;
    } // format 1:only tx-vx available
    else if (format == 2) {
        real_vx_epoch_ms = /*starts at bit 9 */ trunc (*vx_epoch_ms/512);
        *socc = /* bits 8:4 */ trunc ((*vx_epoch_ms - real_vx_epoch_ms*512)/16); 
        tx_minus_vx =  /* bits 3:0 */ (*vx_epoch_ms - real_vx_epoch_ms*512) - (*socc *16); 
    } // format 2: tx-vx and modem occ available
    else {
        *socc = 31; // not available
        tx_minus_vx = 0; 
    } // format 0: tx-vx and modem occ not available
        
    *tx_epoch_ms = real_vx_epoch_ms + tx_minus_vx; 
    *vx_epoch_ms = real_vx_epoch_ms; 

    return;
} // end of decode_sendtime

// reads next decodeable line of the meta data file. returns 0 if reached end of file
int read_ddmd_line (FILE *fp, struct s_dedup *ddmdp) {
    char    mdline[MAX_MD_LINE_SIZE], *mdlp = mdline; 

    // read next line
    while (fgets (mdlp, MAX_MD_LINE_SIZE, fp) != NULL) {
        // parse the line
        if (sscanf (mdlp, 
            "%u, %lf, %lf, %u, %u, %u, %u, %u, %u, %lf, %u, %u", 
            &ddmdp->packet_num, 
            &ddmdp->vx_epoch_ms,
            &ddmdp->rx_epoch_ms,
            &ddmdp->video_packet_len,
            &ddmdp->frame_start, 
            &ddmdp->rolling_frame_number,
            &ddmdp->frame_rate,
            &ddmdp->frame_resolution,
            &ddmdp->frame_end,
            &ddmdp->camera_epoch_ms,
            &ddmdp->retx, 
            &ddmdp->ch) == 12) {
            // successfull scan
            decode_sendtime (2, &ddmdp->vx_epoch_ms, &ddmdp->tx_epoch_ms, &ddmdp->socc);
            return 1;
        } // scan succeded
        else
            // generate warning and try reading the next line
            FWARN (warn_fp, "read_dedumd_line: could not parse line: %s", mdlp)
    } // while there are more lines in the file
    
    // get here at the end of the file
    return 0;
} // end of read_ddmd_line

// returns pointer to the specified file
FILE *open_file (char *filep, char *modep) {
    FILE *fp;
    
    if ((fp = fopen (filep, modep)) == NULL)
        FATAL ("Could not open file %s\n", filep)
    
    return fp; 
} // end of open_file

// interpolate_occ returns interpolated value between the current and next value of occ from the tx log file
int interpolate_occ (double tx_epoch_ms, struct s_txlog *current, struct s_txlog *next) {

    if (current == next) 
        return (current->occ);
    
    float left_fraction = (next->epoch_ms - tx_epoch_ms) / (next->epoch_ms - current->epoch_ms);
        return ((left_fraction * current->occ) + ((1-left_fraction) * next->occ));

} // interpolate occ

// find_occ_frim_tdfile  returns the sampled occupancy at the closest time smaller than the specified tx_epoch_ms and 
// and interpolated occupancy, interpolated between the sampled occupancy above and the next (later) sample
void find_occ_from_tdfile (
    double tx_epoch_ms, struct s_txlog *tdp, int len_tdfile, 
    int *iocc, int *socc, double *socc_epoch_ms, int *bit_rate_kbps) {

    struct s_txlog *left, *right, *current;    // current, left and right index of the search

    left = tdp; right = tdp + len_tdfile -1; 

    if (tx_epoch_ms < left->epoch_ms) // tx started before modem occupancy was read
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is smaller than first occupancy sample time\n", tx_epoch_ms)

    if (tx_epoch_ms > right->epoch_ms + 100) // tx was done significantly later than last occ sample
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is over 100ms largert than last occupancy sample time\n", tx_epoch_ms)

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (tx_epoch_ms > current->epoch_ms)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search

    // on exiting the while, the left is smaller or equal (if current = left edge) than tx_epoch_ms, 
    // the right can be bigger or equal so need to check if the right should be used
    if (tx_epoch_ms == right->epoch_ms) 
        current = right; 

    *socc = current->occ; 
    *iocc = interpolate_occ (tx_epoch_ms, current, MIN((current+1), (tdp+len_tdfile+1)));
    *socc_epoch_ms = current->epoch_ms; 
    *bit_rate_kbps = current->actual_rate;

    return;
} // find_occ_from_tdfile

// reads and parses a meta data line from the specified file. 
// returns 0 if end of file reached or if the scan for all the fields fails due to an incomplete line as in last line of the file.
int read_md_line (int read_header, FILE *fp, int len_tdfile, struct s_txlog *tdp, struct s_carrier *mdp) {

    char mdline[MAX_MD_LINE_SIZE], *mdlinep = mdline; 
    // packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolut, ion	 frame_end	 camera_timestamp	 retx	 chPacketNum

    if (fgets (mdline, MAX_MD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    if (read_header)
        return 1;
    
    // parse the line
    if (sscanf (mdlinep, "%u, %lf, %lf, %u, %u, %u, %u, %u, %u, %lf, %u, %u", 
        &mdp->packet_num,
        &mdp->vx_epoch_ms,           // is actually format 2 sender time
        &mdp->rx_epoch_ms,
        &mdp->packet_len,
        &mdp->frame_start,
        &mdp->frame_num,
        &mdp->frame_rate,
        &mdp->frame_res, 
        &mdp->frame_end,
        &mdp->camera_epoch_ms,
        &mdp->retx,
        &mdp->check_packet_num) !=12) {

            // treat incomplete line as end of file, for example in the last line of the file
            WARN("could not find all the fields in the metadata line %s\n", mdlinep)
            return 0;
        
        } // scan failed

    // decode occupancy and vx2tx delay
    decode_sendtime (2, /*assume format 2 */ &mdp->vx_epoch_ms, &mdp->tx_epoch_ms, &mdp->socc);
    mdp->t2r_latency_ms = mdp->rx_epoch_ms - mdp->tx_epoch_ms; 

    // if (mdp->tx_epoch_ms == 1674932071091)
        // printf ("mdp->tx_epoch = %0lf", mdp->tx_epoch_ms);

    // if tx log exists then overwrite occupancy from tx log file
    if (len_tdfile) { // log file exists
        find_occ_from_tdfile (
            mdp->tx_epoch_ms, tdp, len_tdfile, &(mdp->iocc), &(mdp->socc), &(mdp->socc_epoch_ms),
            &(mdp->bit_rate_kbps));
    }
    // make a copy of the socc for unmodified socc output
    mdp->usocc = mdp->socc; 
    mdp->probe = 0; 

    return 1;

} // end of read_md_line

// reads and parses a proble log line from the specified file. 
// returns 0 if end of file reached or // if the scan for all the fields fails due 
// to an incomplete line as in last line of the file.
int read_pr_line ( FILE *fp, int len_tdfile, struct s_txlog *tdp, struct s_carrier *mdp) {

    char mdline[MAX_MD_LINE_SIZE], *mdlinep = mdline; 
    char dummy_str[100]; 
    int dummy_int;

    if (fgets (mdline, MAX_MD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    // ch: 2, receive_a_probe_packet. sendTime: 1673295597582,  receivedTime: 1673295597601
    // ch: 1, receive_a_probe_packet. sendTime: 1673558308243, latency: 47, receivedTime: 16735583082900
    while (
        (sscanf (mdlinep, "%s %d, %s %s %lf, %s %d, %s %lf", 
            dummy_str,
            &mdp->channel, 
            dummy_str, 
            dummy_str, 
            &mdp->tx_epoch_ms,
            dummy_str, 
            &dummy_int,
            dummy_str,
            &mdp->rx_epoch_ms)) != 9) {

        FWARN(warn_fp, "read_pr_line: Skipping line %s\n", mdlinep)

        // else did not match the specified channel
        if (fgets (mdlinep, MAX_TD_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;
    } // while not successfully scanned a probe log line

    mdp->t2r_latency_ms = mdp->rx_epoch_ms - mdp->tx_epoch_ms; 

    if (len_tdfile) { // log file exists
        find_occ_from_tdfile (
            mdp->tx_epoch_ms, tdp, len_tdfile, &(mdp->iocc), &(mdp->socc), &(mdp->socc_epoch_ms),
            &(mdp->bit_rate_kbps));
    }
    // make a copy of the socc for unmodified socc output
    mdp->usocc = mdp->socc; 
    mdp->probe = 1; 

    return 1;

} // end of read_pr_line

// reads and parses a service log file. Returns 0 if end of file reached
int read_sd_line (FILE *fp, struct s_service *sdp) {

    char sdline[MAX_SD_LINE_SIZE], *sdlinep = sdline; 
    char dummy_str[100];
    char state_str[100];
    int  dummy_int; 
    double dummy_float;

    if (fgets (sdline, MAX_SD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    // CH: 1, change to out-of-service state, latency: 64, latencyTime: 1673815478924, estimated latency: 70, stop_sending flag: 1 , 
    // uplink queue size: 19, zeroUplinkQueue: 0, service flag: 0, numCHOut: 1, Time: 1673815478930, packetNum: 8
    while (sscanf (sdlinep, 
            "%[^:]:%d, %[^,], %[^:]:%d %[^:]:%lf %[^:]:%d %[^:]:%d \
            %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%lf %[^:]:%d" ,
            dummy_str, &sdp->channel,
            state_str,  
            dummy_str, &sdp->bp_t2r,
            dummy_str, &sdp->bp_t2r_epoch_ms, 
            dummy_str, &sdp->est_t2r, 
            dummy_str, &dummy_int,

            dummy_str, &sdp->socc, 
            dummy_str, &dummy_int, 
            dummy_str, &dummy_int,
            dummy_str, &dummy_int,
            dummy_str, &sdp->state_transition_epoch_ms, 
            dummy_str, &sdp->bp_packet_num) !=23) {

        FWARN(warn_fp, "read_sd_line: Skipping line %s\n", sdlinep);

        // else did not match the channel
        if (fgets (sdline, MAX_SD_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;
    } // while not successfully scanned a transmit log line

    if (strcmp(state_str, "change to out-of-service state") == 0)
        sdp->state = 0; 
    else if (strcmp(state_str, "change to in-service state") == 0)
        sdp->state = 1; 
    else
        FATAL ("read_sd_line: could not understand state change string: %s\n", state_str) 

    return 1;
} // end of read_sd_line

// reads and parses a tx log file. Returns 0 if end of file reached
int read_td_line (FILE *fp, struct s_txlog *tdp) {

    char tdline[MAX_TD_LINE_SIZE], *tdlinep = tdline; 
    char dummy_string[100];

    if (fgets (tdline, MAX_TD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    // uplink_queue. ch: 2, timestamp: 1672344732193, queue_size: 23, elapsed_time_since_last_queue_update: 29, actual_rate: 1545, stop_sending_flag: 0, zeroUplinkQueue_flag: 0, lateFlag: 1
    while (sscanf (tdlinep, "%s %s %d, %s %lf, %s %d, %s %d, %s %d",
        dummy_string,
        dummy_string,
        &tdp->channel,
        dummy_string,
        &tdp->epoch_ms, 
        dummy_string,
        &tdp->occ,
        dummy_string,
        &tdp->time_since_last_update,
        dummy_string,
        &tdp->actual_rate) !=11) {

        FWARN(warn_fp, "read_td_line: Skipping line %s\n", tdlinep)
        if (fgets (tdline, MAX_TD_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;

    } // while not successfully scanned a transmit log line

    return 1;
} // end of read_td_line

// reads and parses a latency log file. Returns 0 if end of file reached
// ch: 0, received a latency, numCHOut:0, packetNUm: 0, latency: 28, time: 1673813692132
int read_ld_line (FILE *fp, struct s_latency *ldp) {
    char ldline[MAX_LD_LINE_SIZE], *ldlinep = ldline; 
    char dummy_str[100];
    int  dummy_int; 

    if (fgets (ldline, MAX_LD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    while (sscanf (ldlinep, 
        "%[^:]:%d, %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%lf",
        dummy_str, &ldp->channel,
        dummy_str, &dummy_int,
        dummy_str, &ldp->packet_num, 
        dummy_str, &ldp->t2r_ms, 
        dummy_str, &ldp->bp_epoch_ms) !=10) {

        FWARN(warn_fp, "read_ld_line: Skipping line %s\n", ldlinep)

        // else did not match the channel
        if (fgets (ldline, MAX_LD_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;
    } // while not successfully scanned a transmit log line

    return 1;
} // end of read_ld_line

// converts index to latency
int index2latency (int index) {
    if (index == 0)
        return 20;
    else if (index == 1)
    	return 30;
    else if (index == 2)
    	return 40;
    else if (index == 3)
    	return 50;
    else if (index == 4)
    	return 60;
    else if (index == 5)
    	return 70;
    else if (index == 6)
    	return 80;
    else if (index == 7)
    	return 90;
    else if (index == 8)
    	return 100;
    else if (index == 9) 
        return 110;
    else if (index == 10) 
        return 120;
    else if (index == 11) 
        return 130;
    else if (index == 12) 
        return 140;
    else if (index == 13) 
        return 150;
    else if (index == 14) 
        return 160;
    else if (index == 15) 
        return 200;
    else if (index == 16) 
        return 250;
    else if (index == 17) 
        return 300;
    else if (index == 18) 
        return 350;
    else // if (index == 19) 
        return 400;
} // index2latency

void emit_stats (
    int print_header, int index, 
    char *file_namep, 
    struct s_latency_table *lp, 
    int len_lspike_table, struct s_lspike *lsp, 
    int len_ospike_table, struct s_ospike *osp, 
    int lat_bins_by_occ_table[31][10], 
    struct s_occ_by_lat_bins occ_by_lat_bins_table[20],
    FILE *fp) {

    // avg latency by occupancy
    if (print_header) fprintf (fp, "count, ");          else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", lp->count);
    if (print_header) fprintf (fp, ",");                else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", index);
    if (print_header) fprintf (fp, "%s avg L,", file_namep);    else if ((index >= 31) || (lp->count ==0)) fprintf (fp, ","); 
                                                        else fprintf (fp, "%.0f, ", lp->latency/lp->count);

    // latency spike
    if (print_header) fprintf (fp, "L: occ, ");         else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->max_occ);
    if (print_header) fprintf (fp, "start_TS,");        else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", osp->start_tx_epoch_ms); 
    if (print_header) fprintf (fp, "start, ");          else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->start_packet_num); 
    if (print_header) fprintf (fp, "stop, ");           else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", (lsp->startp+lsp->duration_pkt)->packet_num); 
    if (print_header) fprintf (fp, "dur_pkt, ");        else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->duration_pkt); 
    if (print_header) fprintf (fp, "dur_ms, ");         else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", lsp->duration_ms); 
    if (print_header) fprintf (fp, "max t2r, ");        else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", lsp->max_latency); 
    if (print_header) fprintf (fp, "max t2r pnum, ");   else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->max_lat_packet_num); 

    // occupancy spike
    if (print_header) fprintf (fp, "O: occ,");          else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->max_occ); 
    if (print_header) fprintf (fp, "start_TS,");        else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", osp->start_tx_epoch_ms); 
    if (print_header) fprintf (fp, "start,");           else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->start_packet_num); 
    if (print_header) fprintf (fp, "stop,");            else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", (osp->startp+osp->duration_pkt)->packet_num);
    if (print_header) fprintf (fp, "dur_pkt,");         else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->duration_pkt); 
    if (print_header) fprintf (fp, "dur_ms,");          else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", osp->duration_ms); 

    // latency bins by occupancy table
    int i; 
    // first column
    if (print_header) fprintf (fp, ","); else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d,", index);
    // rest of the columns
    for (i=0; i < 10; i++)
        if (print_header) fprintf (fp, "%d,", i*10+30); else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d,", lat_bins_by_occ_table[index][i]); 

    // occupancy by latency bins table
    // first column
    if (print_header) fprintf (fp, ","); else if (index >= 20) fprintf (fp, ","); else if (index == 19) fprintf (fp, "350+,"); 
        else fprintf (fp, "%d,", index2latency (index));
    // rest of the columns
    if (print_header) fprintf (fp, "count,");    else if (index >=20) fprintf (fp, ",");     else fprintf (fp, "%d,", occ_by_lat_bins_table[index].count);
    if (print_header) fprintf (fp, "sum,");      else if (index >=20) fprintf (fp, ",");     else fprintf (fp, "%.0f,", occ_by_lat_bins_table[index].sum);
    if (print_header) fprintf (fp, "sq sum,");   else if (index >=20) fprintf (fp, ",");     else fprintf (fp, "%.0f,", occ_by_lat_bins_table[index].squared_sum);
    double EX, EX2, stdev; 
    if (occ_by_lat_bins_table[index].count) {
        EX = occ_by_lat_bins_table[index].sum / occ_by_lat_bins_table[index].count;
        EX2 = occ_by_lat_bins_table[index].squared_sum / occ_by_lat_bins_table[index].count;
        stdev = pow ((EX2 - pow (EX, 2)), 0.5); 
    }
    else {
        EX = 0; 
        stdev = 0; 
    }
    if (print_header) fprintf (fp, "%s avg. occ,", file_namep);     else if (index >=20) fprintf (fp, ",");     else fprintf (fp, "%.1f,", EX);
    if (print_header) fprintf (fp, "%s stdev,", file_namep);        else if (index >=20) fprintf (fp, ",");     else fprintf (fp, "%.1f,", stdev);

    fprintf(fp, "\n");

} // end of emit_stats

void emit_aux (int print_header, int probe, int have_txlog, int have_latency_log, int have_service_log, struct s_carrier *cp, FILE *fp) {

    if (print_header) fprintf (fp, "pkt#,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->packet_num); 
    if (print_header) fprintf (fp, "C_TS,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0lf,", cp->camera_epoch_ms); 
    if (print_header) fprintf (fp, "tx_TS,");       else fprintf (fp, "%.0lf,", cp->tx_epoch_ms); 
    if (print_header) fprintf (fp, "rx_TS,");       else fprintf (fp, "%.0lf,", cp->rx_epoch_ms); 
    if (print_header) fprintf (fp, "t2r,");         else fprintf (fp, "%.0f,", cp->t2r_latency_ms); 
    if (print_header) fprintf (fp, "kbps,");        else fprintf (fp, "%d, ", cp->bit_rate_kbps); 
    if (print_header) fprintf (fp, "plen,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->packet_len); 
    if (print_header) fprintf (fp, "retx,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->retx); 
    if (print_header) fprintf (fp, "frame_start,"); else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_start); 
    if (print_header) fprintf (fp, "frame_num,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_num); 
    if (print_header) fprintf (fp, "frame_rate,");  else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_rate); 
    if (print_header) fprintf (fp, "frame_res,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_res); 
    if (print_header) fprintf (fp, "frame_end,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_end); 
    if (print_header) fprintf (fp, "chk_pkt_num,"); else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->check_packet_num); 

    fprintf (fp, "\n");
    return; 
} // end of emit_aux

void emit_full (int print_header, int probe, int have_txlog, int have_latency_log, int have_service_log, struct s_carrier *cp, FILE *fp) {

    if (print_header) fprintf (fp, "pkt#,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->packet_num); 
    if (print_header) fprintf (fp, "C_TS,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0lf,", cp->camera_epoch_ms); 
    if (print_header) fprintf (fp, "tx_TS,");       else fprintf (fp, "%.0lf,", cp->tx_epoch_ms); 
    if (print_header) fprintf (fp, "rx_TS,");       else fprintf (fp, "%.0lf,", cp->rx_epoch_ms); 
    if (print_header) fprintf (fp, "r2t_TS,");      else fprintf (fp, "%.0lf,", cp->ldp->bp_epoch_ms); 
    if (print_header) fprintf (fp, "ert_TS,");      else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0lf,", cp->ert_epoch_ms); 
    if (print_header) fprintf (fp, "socc_TS,");     else fprintf (fp, "%.0lf,", cp->socc_epoch_ms);
    if (print_header) fprintf (fp, "t2r,");         else fprintf (fp, "%.0f,", cp->t2r_latency_ms); 
    if (print_header) fprintf (fp, "r2t,");         else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0f,", cp->r2t_latency_ms); 
    if (print_header) fprintf (fp, "est_t2r,");     else fprintf (fp, "%d, ", cp->est_t2r_ms); 
    if (print_header) fprintf (fp, "ert,");         else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0f,", cp->ert_ms); 
    if (print_header) fprintf (fp, "socc,");        else fprintf (fp, "%d, ", cp->usocc); 
    if (print_header) fprintf (fp, "kbps,");        else fprintf (fp, "%d, ", cp->bit_rate_kbps); 
    if (print_header) fprintf (fp, "plen,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->packet_len); 
    if (print_header) fprintf (fp, "retx,");        else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->retx); 

    if (!have_latency_log) goto SKIP_LATENCY_EMIT_FULL;
    if (print_header) fprintf (fp, "bp_pkt_TS,");   else fprintf (fp, "%.0lf,", cp->lsp->bp_epoch_ms); 
    if (print_header) fprintf (fp, "bp_pkt,");      else fprintf (fp, "%u, ", cp->lsp->packet_num); 
    if (print_header) fprintf (fp, "bp_t2r,");      else fprintf (fp, "%d, ", cp->lsp->t2r_ms); 
    SKIP_LATENCY_EMIT_FULL:

    if (!have_service_log) goto SKIP_SERVICE_EMIT_FULL;
    if (print_header) fprintf (fp, "s_st,");        else fprintf (fp, "%d, ", cp->sdp->state); 
    if (print_header) fprintf (fp, "s_socc,");      else fprintf (fp, "%d, ", cp->sdp->socc); 
    if (print_header) fprintf (fp, "s_est_t2r,");   else fprintf (fp, "%d, ", cp->sdp->est_t2r); 
    if (print_header) fprintf (fp, "s_bp_t2r,");    else fprintf (fp, "%d, ", cp->sdp->bp_t2r); 
    if (print_header) fprintf (fp, "s_st_tr,");     else fprintf (fp, "%.0lf,", cp->sdp->state_transition_epoch_ms); 
    if (print_header) fprintf (fp, "s_bp_TS,");     else fprintf (fp, "%.0lf,", cp->sdp->bp_t2r_epoch_ms); 
    if (print_header) fprintf (fp, "s_bp_pkt,");    else fprintf (fp, "%d,", cp->sdp->bp_packet_num); 
    SKIP_SERVICE_EMIT_FULL:

    if (!have_txlog) goto SKIP_TXLOG_EMIT_FULL;
    // if (print_header) fprintf (fp, "iocc,");        else fprintf (fp, "%d, ", cp->iocc);
    SKIP_TXLOG_EMIT_FULL:
    
    // if (print_header) fprintf (fp, "vx_epoch_ms,"); else if (probe) fprintf (fp, ","); else fprintf (fp, "%.0lf,", cp->vx_epoch_ms); 
    if (print_header) fprintf (fp, "frame_start,"); else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_start); 
    if (print_header) fprintf (fp, "frame_num,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_num); 
    if (print_header) fprintf (fp, "frame_rate,");  else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_rate); 
    if (print_header) fprintf (fp, "frame_res,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_res); 
    if (print_header) fprintf (fp, "frame_end,");   else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->frame_end); 
    if (print_header) fprintf (fp, "chk_pkt_num,"); else if (probe) fprintf (fp, ","); else fprintf (fp, "%d, ", cp->check_packet_num); 

    fprintf (fp, "\n");
    return; 
} // end of emit_full

/*
void emit_prb (int print_header, int have_txlog, int have_service_log, struct s_carrier *cp, FILE *fp) {

    if (print_header) fprintf (fp, "packet_num,");          else fprintf (fp, ","); 
    if (print_header) fprintf (fp, "camera_epoch_ms,");     else fprintf (fp, ","); 
    if (print_header) fprintf (fp, "tx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->tx_epoch_ms); 
    if (print_header) fprintf (fp, "rx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->rx_epoch_ms); 
    if (print_header) fprintf (fp, "t2r,");                 else fprintf (fp, "%.0lf,", cp->t2r_latency_ms); 
    if (print_header) fprintf (fp, "socc,");                else if (have_txlog) fprintf (fp, "%d, ", cp->usocc); else fprintf(fp, ","); 
    if (print_header) fprintf (fp, "kbps,");                else fprintf (fp, "%d, ", cp->bit_rate_kbps); 
    if (!have_service_log) goto SKIP_SERVICE_EMIT_PRB;
    if (print_header) fprintf (fp, "s_st,");                else fprintf (fp, "%d, ", cp->sdp->state); 
    if (print_header) fprintf (fp, "s_bpt2r,");             else fprintf (fp, "%d, ", cp->sdp->bp_t2r); 
    if (print_header) fprintf (fp, "s_est_t2r,");           else fprintf (fp, "%d, ", cp->sdp->est_t2r); 
    if (print_header) fprintf (fp, "s_socc,");              else fprintf (fp, "%d, ", cp->sdp->socc); 
    if (print_header) fprintf (fp, "s_st_tr,");             else fprintf (fp, "%.0lf,", cp->sdp->state_transition_epoch_ms); 
    if (print_header) fprintf (fp, "s_bp_epoch_ms,");       else fprintf (fp, "%.0lf,", cp->sdp->bp_t2r_epoch_ms); 
    SKIP_SERVICE_EMIT_PRB:
    if (print_header) fprintf (fp, "socc_epoch_ms,");       else if (have_txlog) fprintf (fp, "%.0lf,", cp->socc_epoch_ms); else fprintf (fp, ",");
    if (print_header) fprintf (fp, "vx_epoch_ms,");         else fprintf (fp, ",");
    if (print_header) fprintf (fp, "iocc,");                else if (have_txlog) fprintf (fp, "%d, ", cp->iocc);  else fprintf(fp, ","); 
    if (print_header) fprintf (fp, "plen,");                else fprintf (fp, ",");
    if (print_header) fprintf (fp, "frame_start,");         else fprintf (fp, ",");
    if (print_header) fprintf (fp, "frame_num,");           else fprintf (fp, ",");
    if (print_header) fprintf (fp, "frame_rate,");          else fprintf (fp, ",");
    if (print_header) fprintf (fp, "frame_res,");           else fprintf (fp, ",");
    if (print_header) fprintf (fp, "frame_end,");           else fprintf (fp, ",");
    if (print_header) fprintf (fp, "retx,");                else fprintf (fp, ",");
    if (print_header) fprintf (fp, "check_packet_num,");    else fprintf (fp, ",");

    fprintf (fp, "\n");
    return; 

} // end of emit_prb

*/

// prints program usage
void print_usage (void) {
char *usage = "Usage: occ [-help] [-o <dd>] [-l <dd>] [-s] -ipath <dir> -opath <dir> \
-no_tx|tx_pre <prefix> -no_pr|pr_pre <prefix> -no_sr|sr_pre <prefix> -no_la|la_pre <prefix> \
-rx_pre <prefix>";
//     char *usage_part1 = "Usage: occ [-help] [-o <dd>] [-l <dd>] [-s] -ipath <dir> -opath <dir> "; 
//     char *usage_part2 = "-no_tx|tx_pre <prefix> -no_pr|pr_pre <prefix> -no_sr|sr_pre <prefix> -rx_pre <prefix>";
    printf ("%s\n", usage); 
    printf ("\t -o: occupancy threshold for degraded channel. Default 10.\n"); 
    printf ("\t -l: latency threshold for degraded channel. Default 60ms.\n"); 
    printf ("\t -s: Turns on silent, no console warning. Default off.\n"); 
    printf ("\t -ipath: input path directory. last ipath name applies to next input file\n"); 
    printf ("\t -opath: out path directory \n"); 
    printf ("\t -no_tx|tx_pre: tranmsmit side log filename without the extension .log. Use -no_tx if no tx side file.\n"); 
    printf ("\t -no_pr|pr_pre: probe tx log without the extension .log. Use -no_pr if probe log unvailable.\n"); 
    printf ("\t -no_sr|sr_pre:  service log filename without the extension .log. Use -no_sr if service log unavailable\n"); 
    printf ("\t\t sr_pre is ignored if pr_pre is not specified\n"); 
    printf ("\t -no_la|la_pre:  latency log filename without the extension .log. Use -no_la if latency log unavailable\n"); 
    printf ("\t\t la_pre is ignored if pr_pre is not specified\n"); 
    printf ("\t -rx_pre: receive side metadata filename without the extension .csv. MUST BE AT THE LAST\n"); 
    return; 
} // print_usage

void sort_md_by_tx (struct s_carrier *mdp, int len) {
    int i, j; 
    for (i=1; i<len; i++) {
        j = i; 

        while (j != 0) {
            if ((mdp+j)->tx_epoch_ms < (mdp+j-1)->tx_epoch_ms) {
                // slide jth element up by 1
                struct s_carrier temp = *(mdp+j-1); 
                *(mdp+j-1) = *(mdp+j);
                *(mdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it

        /*
        if (i%10000 == 0) 
            printf ("i = %d\n", i);
        */

    } // for elements in the metadata array

    return;
} // end of sort_md_by_tx

// sorts lspike table by increasing occupancy
void sort_lspike (struct s_lspike *sp, int len) {
    int i, j; 
    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((sp+j)->max_occ < (sp+j-1)->max_occ) {
                // slide jth element up by 1
                struct s_lspike temp = *(sp+j-1); 
                *(sp+j-1) = *(sp+j);
                *(sp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it

    } // for elements in the metadata array

    return;
} // end of sort_lspike

// sorts ospike table by increasing occupancy
void sort_ospike (struct s_ospike *sp, int len) {
    int i, j; 
    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((sp+j)->max_occ < (sp+j-1)->max_occ) {
                // slide jth element up by 1
                struct s_ospike temp = *(sp+j-1); 
                *(sp+j-1) = *(sp+j);
                *(sp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it

    } // for elements in the metadata array

    return;
} // end of sort

// converts latency to index
int latency2index (int latency) {
    if (latency <= 30)
        return 0;
    else if (latency <= 40)
        return 1;
    else if (latency <= 50)
        return 2;
    else if (latency <= 60)
        return 3;
    else if (latency <= 70)
        return 4;
    else if (latency <= 80)
        return 5;
    else if (latency <= 90)
        return 6;
    else if (latency <= 100)
        return 7;
    else if (latency <= 110)
        return 8;
    else // 120+ 
        return 9;
} // latency2index

// converts latency to index
int extended_latency2index (int latency) {
    if (latency <= 20)
        return 0;
    else if (latency <= 30)
        return 1;
    else if (latency <= 40)
        return 2;
    else if (latency <= 50)
        return 3;
    else if (latency <= 60)
        return 4;
    else if (latency <= 70)
        return 5;
    else if (latency <= 80)
        return 6;
    else if (latency <= 90)
        return 7;
    else if (latency <= 100)
        return 8;
    else if (latency <= 110)
        return 9;
    else if (latency <= 120)
        return 10;
    else if (latency <= 130)
        return 11;
    else if (latency <= 140)
        return 12;
    else if (latency <= 150)
        return 13;
    else if (latency <= 160)
        return 14;
    else if (latency <= 200)
        return 15;
    else if (latency <= 250)
        return 16;
    else if (latency <= 300)
        return 17;
    else if (latency <= 350)
        return 18;
    else // 350+
        return 19;
} // latency2index

void sort_td_by_tx (struct s_txlog *tdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((tdp+j)->epoch_ms < (tdp+j-1)->epoch_ms) {
                // slide jth element up by 1
                struct s_txlog temp = *(tdp+j-1); 
                *(tdp+j-1) = *(tdp+j);
                *(tdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it

    } // for elements in the log data array

    return;
} // end of sort_td_by_tx

// sorts latency data array by receipt epoch_ms
void sort_ld_by_bp_epoch_ms (struct s_latency *ldp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((ldp+j)->bp_epoch_ms < (ldp+j-1)->bp_epoch_ms) {
                // slide jth element up by 1
                struct s_latency temp = *(ldp+j-1); 
                *(ldp+j-1) = *(ldp+j);
                *(ldp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it

    } // for elements in the log data array

    return;
} // end of sort_ld_by_bp_epoch_ms

// sorts latency data array by packet number
void sort_ld_by_packet_num (struct s_latency *ldp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((ldp+j)->packet_num < (ldp+j-1)->packet_num) {
                // slide jth element up by 1
                struct s_latency temp = *(ldp+j-1); 
                *(ldp+j-1) = *(ldp+j);
                *(ldp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it
    } // for elements in the log data array

    return;
} // end of sort_ld_by_packet_num

void copy_tdp_to_tsdp (struct s_txlog *tdp, struct  s_merged_txlog_service *tsdp) {
    tsdp->tdp = tdp; 
    tsdp->tdp1_sdp0 = 1; 
} // end of copy_sdp_to_tsdp

void copy_sdp_to_tsdp (struct s_service *sdp, struct  s_merged_txlog_service *tsdp) {
    tsdp->sdp = sdp; 
    tsdp->tdp1_sdp0 = 0; 
} // end of copy_sdp_to_tsdp

// merge_txlog_service merges uplink (tx) log with service log
void merge_txlog_service (
    struct s_txlog *tdp, int len_td, struct s_service *sdp, int len_sd, 
    struct s_merged_txlog_service *tsdp) {
    
    struct s_txlog *end_tdp = tdp + len_td; 
    struct s_service *end_sdp = sdp + len_sd; 

    while ((tdp < end_tdp) || (sdp < end_sdp)) {
        if (tdp == end_tdp) // reached end of tdp file so copy left over sdp
            copy_sdp_to_tsdp (sdp++, tsdp++); 
        else if (sdp == end_sdp) // reached end of sdp file so copy left over tdp
            copy_tdp_to_tsdp (tdp++, tsdp++); 
        else if (tdp->epoch_ms <= sdp->state_transition_epoch_ms) 
            copy_tdp_to_tsdp (tdp++, tsdp++); 
        else
            copy_sdp_to_tsdp (sdp++, tsdp++); 
    } // while there are more items to be merged
    return; 
} // end of merge_txlog_service

// merge_probe_carrier merges tx_TS sorted probe and carrier metadata arrays
// into one. If elemets in the two arrays are equal, stream that emitted previous 
// packet is given preference 
void merge_probe_carrier (
    struct s_carrier *i1p, int len_i1, struct s_carrier *i2p, int len_i2, 
    struct s_carrier *op) {

    struct s_carrier *end_i1p = i1p + len_i1; 
    struct s_carrier *end_i2p = i2p + len_i2; 

    int last_i1 = 1;        // 1 indicates that previous emitted packet was i1
    while ((i1p < end_i1p) || (i2p < end_i2p)) {
        if (i1p == end_i1p)
            *op++ = *i2p++; 
        else if (i2p == end_i2p)
            *op++ = *i1p++; 
        else if (i1p->tx_epoch_ms == i2p->tx_epoch_ms) {
            // maintain continuity with the previous packet
            if (last_i1)
                *op++ = *i1p++; // last_i1 is already set to 1
            else {
                *op++ = *i2p++; 
                last_i1 = 0; 
            }
        } // i1 = i2
        else if (i1p->tx_epoch_ms < i2p->tx_epoch_ms) {
            *op++ = *i1p++;
            last_i1 = 1; 
        } 
        else {
            *op++ = *i2p++; 
            last_i1 = 0; 
        }
    } // while there are more items to be merged

    return;
} // merge_probe_carrier 

int create_frame_list (struct s_dedup *ddmdp, int len_ddmdfile, struct s_frame *fp) {

    int frame_count = 0; 
    int j; 
    for (j=0; j<len_ddmdfile; j++, ddmdp++) {
        if (ddmdp->frame_start) {
            int i; 
            fp->first_packet = fp->latest_packet = ddmdp; 

            for (i=0; i<3; i++) {
                if (frame_count == 0) { // assume good state at the start
                    fp->service_state[i] = IN_SERVICE;
                    fp->ch_quality_state[i] = GOOD_QUALITY;
                    fp->socc[i] = 0; 
                } // first frame
                // no transitions recorded yet
                fp->service_state_transition[i] = 0; 
                fp->ch_quality_state_transition[i] = 0; 
                fp->socc_transition[i] = 0; 
            } // for all channels

            if (frame_count == 0)
                fp->enc_quality_state = GOOD_QUALITY; 
            fp->enc_quality_state_transition = 0; 

            fp->frame_number = ++frame_count; 
        } // first packet of a frame
        else {
            if (fp->latest_packet->rx_epoch_ms < ddmdp->rx_epoch_ms)
                fp->latest_packet = ddmdp; 
            if (ddmdp->frame_end)
                fp++; 
        } // non-first packet of a frame
    } // for all packets in ddmdp file

    return frame_count; 
} // create_frame_list

// returns pointer to the sdp equal to or closest smaller sdp to the specified epoch_ms
struct s_service *find_closest_sdp (double epoch_ms, struct s_service *sdp, int len_sd) {

    struct  s_service  *left, *right, *current;    // current, left and right index of the search

    left = sdp; right = sdp + len_sd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->state_transition_epoch_ms)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    if ((current+1)->state_transition_epoch_ms == epoch_ms)
        return current+1; 
    else 
        return current; 

} // find_closest_sdp

// returns pointer to the sdp equal to or closest smaller sdp to the specified epoch_ms
struct s_latency *find_ldp_by_packet_num (int packet_num, double rx_epoch_ms, struct s_latency *ldp, int len_ld) {

    struct  s_latency  *left, *right, *current;    // current, left and right index of the search

    left = ldp; right = ldp + len_ld - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (packet_num > current->packet_num)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than specified packet_num
    if (current->packet_num == packet_num)
        left = current; 
    else if ((current+1)->packet_num == packet_num)
        left = current + 1;
    else // no match, the back propagated packet got lost, so use the nearest smaller packet
        left = current ; // correct thing to do will be to find closest bigger bp_epoch_ms packet
    
    // now search to the right and see which element is closest is the specified rx_epoch_ms
    // assumes that same numbered packets are in ascending bp_epoch_ms
    while ((left->bp_epoch_ms < rx_epoch_ms) && (left < (ldp+len_ld-1)))
        left++;
    return left;

} // find_ldp_by_packet_num

// returns pointer to the sdp equal to or closest smaller sdp to the specified epoch_ms
struct s_latency *find_closest_ldp (double epoch_ms, struct s_latency *ldp, int len_ld) {

    struct  s_latency  *left, *right, *current;    // current, left and right index of the search

    left = ldp; right = ldp + len_ld - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->bp_epoch_ms)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    // if there is string of entries that match the epoch_ms, move to the right most edge as it is the most
    // recently arrived information
    while ((current+1)->bp_epoch_ms == epoch_ms)
        current++; 
    return current; 
} // find_closest_ldp

// find_packet_in_cd returns pointer to the packet in the specified metadata array
// returns NULL if no match was found, else the specified packet num
struct s_carrier* find_packet_in_cd (int packet_num, struct s_carrier *cdp, int len_cd) {

    struct  s_carrier *left, *right, *current;    // current, left and right index of the search

    left = cdp; right = cdp + len_cd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (packet_num > current->packet_num)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 

        // if current is a probe meta data then find the smaller data packet
        while (current > left && current->probe) 
            current--;     
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than packet_num

    return current;
} // find_packet_in_cd

// find_frame searches for the frame which the specified epoch_ms falls into starting from 
// the fdp. retruns updated fdp or NULL if end of fd list is reached
struct s_frame *find_frame (struct s_frame *fdp, int frame_count, double epoch_ms) {

    double next_frame_epoch_ms = 
        (fdp->frame_number == frame_count) ?  // this frame is the last frame
        fdp->first_packet->camera_epoch_ms + FRAME_DURATION : 
        (fdp+1)->first_packet->camera_epoch_ms; 

    while (epoch_ms > next_frame_epoch_ms) {
        if (fdp->frame_number == frame_count)
            return NULL; 
        fdp++; 
        next_frame_epoch_ms = 
            (fdp->frame_number == frame_count) ?  // this frame is the last frame
            fdp->first_packet->camera_epoch_ms + FRAME_DURATION : 
            (fdp+1)->first_packet->camera_epoch_ms; 
    } // while epoch_ms is bigger than the current frame

    return fdp; 
} // find_frame

void emit_frame (int print_header, struct s_frame *fdp, FILE *out_fp) {
    int c2r = fdp->latest_packet->rx_epoch_ms - fdp->first_packet->camera_epoch_ms;
    if (print_header) fprintf (out_fp, "F#,");      else fprintf (out_fp, "%d,", fdp->frame_number);
    if (print_header) fprintf (out_fp, "c2r,");     else fprintf (out_fp, "%d,", c2r);
    if (print_header) fprintf (out_fp, "QM,");
    else fprintf (out_fp, "%d,", fdp->ch_quality_state[0] + fdp->ch_quality_state[1] + fdp->ch_quality_state[2]);
    if (print_header) fprintf (out_fp, "ES,");      else fprintf (out_fp, "%d,", fdp->enc_quality_state);
    if (print_header) fprintf (out_fp, "P#1,");     else fprintf (out_fp, "%d,", fdp->first_packet->packet_num);
    if (print_header) fprintf (out_fp, "LCh,");     else fprintf (out_fp, "%d,", fdp->latest_packet->ch);
    if (print_header) fprintf (out_fp, "CTS,");     else fprintf (out_fp, "%0.lf,", fdp->first_packet->camera_epoch_ms); 

    return;
} // emit_frame

void emit_per_carrier (int print_header, int channel, struct s_frame *fdp, FILE *out_fp) {
    if (print_header) fprintf (out_fp, "%d,", channel);   else fprintf (out_fp, ","); 
    if (print_header) fprintf (out_fp, "IS,");      else fprintf (out_fp, "%d,", fdp->service_state[channel]);
    if (print_header) fprintf (out_fp, "ISx,");     else fprintf (out_fp, "%d,", fdp->service_state_transition[channel]);
    if (print_header) fprintf (out_fp, "ISx_TS,");  
    else {
        if (fdp->service_state_transition[channel]) fprintf (out_fp, "%0.lf,", fdp->service_state_transition_TS[channel]);
        else  fprintf (out_fp, ","); 
    }
    if (print_header) fprintf (out_fp, "socc,");      else fprintf (out_fp, "%d,", fdp->socc[channel]);
    if (print_header) fprintf (out_fp, "SoccX,");     else fprintf (out_fp, "%d,", fdp->socc_transition[channel]);
    if (print_header) fprintf (out_fp, "SoccX_TS,");  
    else {
        if (fdp->socc_transition[channel]) fprintf (out_fp, "%0.lf,", fdp->socc_transition_TS[channel]);
        else  fprintf (out_fp, ",");
    }
    if (print_header) fprintf (out_fp, "QM,");      else fprintf (out_fp, "%d,", fdp->ch_quality_state[channel]);
    if (print_header) fprintf (out_fp, "QMx,");     else fprintf (out_fp, "%d,", fdp->ch_quality_state_transition[channel]); 
    return; 
} // emit_per_carrier

int main (int argc, char* argv[]) {
    int occ_threshold = 10;                 // occupancy threshold for degraded channel
    int latency_threshold = 60;             // latency threshold for degraded cahnnel 
    struct s_dedup *ddmdp;                  // pointer to an entry in dedup metadata
    struct s_frame *fdp;                    // pointer to an entry in the frame list
    // struct s_carrier *mdp, *pdp, *mpdp;  // pointers to metadata, probe data and combined data
    struct s_txlog *tdp;                    // pointer to tx log data 
    struct s_service *sdp;                  // pointer to service log data 
    // struct s_latency *ldp;               // pointer to latency log data sorted by packet_num
    // struct s_latency *lsp;               // pointer to latency log data sorted by bp_epoch_ms
    struct s_merged_txlog_service *tsdp;    // merged uplink (tx) and service logs
    FILE *ddmd_fp = NULL;                   // dedup meta data file
    FILE *md_fp = NULL;                     // per carrier meta data file
    FILE *out_fp = NULL;                    // output file 
    FILE *td_fp = NULL;                     // tx log file
    FILE *pr_fp = NULL;                     // probe transmission log file
    FILE *sr_fp = NULL;                     // service log file
    FILE *ld_fp = NULL;                     // latency log file
    int len_ddmdfile = 0;                   // lines in dedup meta data file not including header line
    int len_mdfile = 0;                     // lines in meta data file not including header line
    int len_tdfile = 0;                     // lines in tx log file.  0 indicates the log does not exist
    int len_prfile = 0;                     // lines in probe log file .0 indicates the log does not exist
    int len_srfile = 0;                     // lines in service log file .0 indicates the log does not exist
    int len_ldfile = 0;                     // lines in latency file. 0 means log file does not exist
    int last_socc;                          // remembers occ from the last update from modem
    int frame_count;                        // frames in dedup metadata file
    struct s_latency_table avg_lat_by_occ_table[31];
    struct s_lspike lspike_table[MAX_SPIKES], *lspikep;
    struct s_ospike ospike_table[MAX_SPIKES], *ospikep;
    int len_lspike_table;
    int len_ospike_table;

    int lat_bins_by_occ_table[31][10];
    struct s_occ_by_lat_bins occ_by_lat_bins_table[20];

    char buffer[1000], *bp=buffer; 
    char ipath[500], *ipathp = ipath; 
    char opath[500], *opathp = opath; 
    char rx_prefix[500], *rx_prefixp = rx_prefix; 
    char tx_prefix[500], *tx_prefixp = tx_prefix; 
    char pr_prefix[500], *pr_prefixp = pr_prefix; 
    char sr_prefix[500], *sr_prefixp = sr_prefix; 
    char la_prefix[500], *la_prefixp = la_prefix; 
    int ch;                                         // channel number to use from the tx log file

    struct s_files file_table[MAX_FILES];
    int len_file_table = 0;
    int br_reduction_threshold, br_increase_threshold, br_increase_latency; 
    int min_state_duration_latency; 

    //  read command line arguments
    while (*++argv != NULL) {

        // help/usage
        if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            my_exit (-1); 
        }

        // silent mode
        else if (strcmp (*argv, "-s") == MATCH) {
            silent = 1; 
        } 

        // bit rate modulation threshold parameters
        else if (strcmp (*argv, "-bri_occ") == MATCH) {
            if (sscanf (*++argv, "%d", &br_increase_threshold) != 1)
                FATAL ("-bri_occ should be followed by digit\n%s", "")
        }
        
        else if (strcmp (*argv, "-bri_lat") == MATCH) {
            if (sscanf (*++argv, "%d", &br_increase_latency) != 1)
                FATAL ("-bri_lat should be followed by digit\n%s", "")
        }
        
        else if (strcmp (*argv, "-brd_occ") == MATCH) {
            if (sscanf (*++argv, "%d", &br_reduction_threshold) != 1)
                FATAL ("-brd_occ should be followed by digit\n%s", "")
        }
        
        else if (strcmp (*argv, "-stdr") == MATCH) {
            if (sscanf (*++argv, "%d", &min_state_duration_latency) != 1)
                FATAL ("-stdr should be followed by digit\n%s", "")
        }
        
        // input / output file locations and file prefix
        else if (strcmp (*argv, "-ipath") == MATCH) {
            strcpy (ipathp, *++argv); 
        }

        else if (strcmp (*argv, "-opath") == MATCH) {
            strcpy (opathp, *++argv); 
        }

        else if (strcmp (*argv, "-pr_pre") == MATCH) {
            strcpy (pr_prefix, *++argv); 
        }

        else if (strcmp (*argv, "-sr_pre") == MATCH) {
            strcpy (sr_prefix, *++argv); 
        }
        
        else if (strcmp (*argv, "-la_pre") == MATCH) {
            strcpy (la_prefix, *++argv); 
        }
        
        else if (strcmp (*argv, "-tx_pre") == MATCH) {
            strcpy (tx_prefix, *++argv); 
        }

        else if (strcmp (*argv, "-rx_pre") == MATCH) {
            // must be the last command line argument
            strcpy (rx_prefix, *++argv); 

            strcpy (file_table[len_file_table].input_directory, ipathp); 

            sprintf (file_table[len_file_table].rx_prefix, "%s", rx_prefix); 

            strcpy (file_table[len_file_table].tx_prefix, tx_prefix); 
             
            strcpy (file_table[len_file_table].pr_prefix, pr_prefix); 

            strcpy (file_table[len_file_table].sr_prefix, sr_prefix); 
             
            strcpy (file_table[len_file_table].la_prefix, la_prefix); 
             
            len_file_table++;
        } // -rx_pre

        // invalid option
        else {
            FATAL("Invalid option %s\n", *argv)
        }
	
    } // while there are more arguments to process

    // open files
    int file_index = 0; 

PROCESS_EACH_FILE:

    printf ("Now processing file %s\n", file_table[file_index].rx_prefix); 

    sprintf (bp, "%s%s_out_brm.csv", opath, file_table[file_index].rx_prefix); 
	out_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_warnings_occ.txt", opath, file_table[file_index].rx_prefix); 
   	warn_fp = open_file (bp, "w");

    if ((out_fp == NULL) || (warn_fp == NULL)) {
        printf ("Missing or could not open input or output file\n"); 
        print_usage (); 
        my_exit (-1); 
    }

    // read all the input data

    // 
    // uplink (tx) log
    // 
    // open file
    sprintf (bp, "%s%s.log", file_table[file_index].input_directory, file_table[file_index].tx_prefix); 
    printf ("Now reading %s\n", bp);
	td_fp = open_file (bp, "r");
    
    // allocate storage for tx log
    td = (struct s_txlog *) malloc (sizeof (struct s_txlog) * TX_BUFFER_SIZE);
    if (td==NULL) FATAL("Could not allocate storage to read the tx log file in an array%s\n", "")
    tdp = td; 

	// read tx log file into array and sort it
	len_tdfile = 0; 
	while (read_td_line (td_fp, tdp)) {
		len_tdfile++;
		if (len_tdfile == TX_BUFFER_SIZE)
		    FATAL ("TX data array is not large enough to read the tx log file. Increase TX_BUFFER_SIZE%S\n", "");
		tdp++;
	} // while there are more lines to be read

	if (len_tdfile == 0)
		FATAL("Meta data file is empty%s", "\n")

	// sort by timestamp
    sort_td_by_tx (td, len_tdfile); 

    // 
    // dedup metadata 
    //
    // open file
    sprintf (bp, "%s%s.csv", file_table[file_index].input_directory, file_table[file_index].rx_prefix); 
    printf ("Now reading %s\n", bp);
	ddmd_fp = open_file (bp, "r");

    // allocate storage for dedup meta data 
    ddmd = (struct s_dedup *) malloc (sizeof (struct s_dedup) * MD_BUFFER_SIZE);
    if (ddmd==NULL)
        FATAL("Could not allocate storage to read the dedup meta data file in an array%s\n", "")
    ddmdp = ddmd; 

    // read the file into array
    char ddmd_header[MAX_LD_LINE_SIZE]; 
    fgets (ddmd_header, MAX_MD_LINE_SIZE, ddmd_fp); // skip header 
    len_ddmdfile = 0; 
    while (read_ddmd_line(ddmd_fp, ddmdp)) {
        len_ddmdfile++;
        if (len_ddmdfile == MD_BUFFER_SIZE)
            FATAL ("Dedup meta data array is not large enough to ready the meta data file. Increase MD_BUFFER_SIZE%S\n", "");
        ddmdp++;
    } // while there are more lines to read from the dedup metadata file
    
    //
    // create frame list
    // 
    fd = (struct s_frame *) malloc (sizeof (struct s_frame) * FD_BUFFER_SIZE);
    if (fd==NULL)
        FATAL("Could not allocate storage to create frame data list%s\n", "")
    fdp = fd;
    frame_count = create_frame_list (ddmd, len_ddmdfile, fd); 

    /*
    //
    // carrier metadata
    //
    sprintf (bp, "%s%s.csv", file_table[file_index].input_directory, file_table[file_index].rx_prefix); 
    printf ("Now reading %s\n", bp);
	md_fp = open_file (bp, "r");
    
    // allocate storage for per carrier meta data 
    md = (struct s_carrier *) malloc (sizeof (struct s_carrier) * MD_BUFFER_SIZE);
    if (md==NULL)
        FATAL("Could not allocate storage to read the meta data file in an array%s\n", "")
    mdp = md; 

    // read the file into array
    len_mdfile = 0; 
    read_md_line (1, md_fp, len_tdfile, td, mdp); // skip header
    while (read_md_line (0, md_fp, len_tdfile, td, mdp)) {
        len_mdfile++;
        if (len_mdfile == MD_BUFFER_SIZE)
            FATAL ("Meta data array is not large enough to ready the meta data file. Increase MD_BUFFER_SIZE%S\n", "");
        mdp++;
    } // while there are more lines to be read
    if (len_mdfile == 0)
        FATAL("Meta data file is empty%s", "\n")

    // sort by tx_epoch_ms
    sort_md_by_tx (md, len_mdfile); 
    
    //
    // probe log
    //
    // open file
    sprintf (bp, "%s%s.log", file_table[file_index].input_directory, file_table[file_index].pr_prefix); 
    printf ("Now reading %s\n", bp);
	pr_fp = open_file (bp, "r");
    
    // allocate storage for probe data
    pd = (struct s_carrier *) malloc (sizeof (struct s_carrier) * PR_BUFFER_SIZE);
    if (pd==NULL)
        FATAL("Could not allocate storage to read the probe data file in an array%s\n", "")

    // read probe data
    len_prfile = 0; 
    printf ("Now reading %s\n", bp);
    pdp = pd; 
    while (read_pr_line (pr_fp, len_tdfile, td, pdp)) {
        len_prfile++;
        if ((len_prfile) == MD_BUFFER_SIZE)
            FATAL ("probe data array is not large enough. Increase MD_BUFFER_SIZE%S\n", "");
        pdp->socc = MIN(30, pdp->socc);
        pdp++;
    } // while there are more lines to be read

    if (len_prfile == 0)
        FATAL("Probe log file is empty%s", "\n")

    // sort by tx_epoch_ms
    sort_md_by_tx (pd, len_prfile); 

    //
    // merge probe and the meta data into a single array
    //
    // allocate storage for combined array
    mpd = (struct s_carrier *) malloc (sizeof (struct s_carrier) * (PR_BUFFER_SIZE + MD_BUFFER_SIZE));
    if (mpd==NULL)
        FATAL("Could not allocate storage for combined probe and metadata array%s\n", "")

    mpdp = mpd; 
    merge_probe_carrier (pd, len_prfile, md, len_mdfile, mpdp); 
    
    //
    // latency log
    //
    // open file
    sprintf (bp, "%s%s.log", file_table[file_index].input_directory, file_table[file_index].la_prefix); 
    printf ("Now reading %s\n", bp);
	ld_fp = open_file (bp, "r");
    
    // allocate storage
    ld = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    ls = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    if (ld==NULL || ls==NULL)
        FATAL("Could not allocate storage to read the latency log file in an array%s\n", "")

    // read latency log. assumed to be sorted by epoch_ms
    len_ldfile = 0; 
    ldp = ld; lsp = ls; 
    while (read_ld_line (ld_fp, ldp)) {
        len_ldfile++;
        if ((len_ldfile) == LD_BUFFER_SIZE)
            FATAL ("latency data array is not large enough. Increase LD_BUFFER_SIZE%S\n", "");
        *lsp = *ldp; 
        ldp++; lsp++; 
        if (len_ldfile % 10000 == 0)
            printf ("Reached line %d of latency log\n", len_ldfile); 
    } // while there are more lines to be read

    if (len_ldfile == 0)
        FATAL("latency log file is empty%s", "\n")

    // sort by bp_epoch_ms
    sort_ld_by_bp_epoch_ms (ls, len_ldfile);
    
    // sort by packet_num
    sort_ld_by_packet_num (ld, len_ldfile); 
    */

    //
    // service log
    //
    // open file
    sprintf (bp, "%s%s.log", file_table[file_index].input_directory, file_table[file_index].sr_prefix); 
    printf ("Now reading %s\n", bp);
	sr_fp = open_file (bp, "r");
    
    // allocate storage
    sd = (struct s_service *) malloc (sizeof (struct s_service) * SD_BUFFER_SIZE);
    if (sd==NULL)
        FATAL("Could not allocate storage to read the service data file in an array%s\n", "")

    // read service log. assumed to be sorted by epoch_ms
    printf ("Now reading service log\n");
    len_srfile = 0; 
    sdp = sd; 
    while (read_sd_line (sr_fp, sdp)) {
        len_srfile++;
        if ((len_srfile) == SD_BUFFER_SIZE)
            FATAL ("service data array is not large enough. Increase SD_BUFFER_SIZE%S\n", "");
        sdp++;
        if (len_srfile % 10000 == 0)
            printf ("Reached line %d of service log\n", len_srfile); 
    } // while there are more lines to be read

    if (len_srfile == 0)
        FATAL("service log file is empty%s", "\n")

    /*
    // add info from service log to the mpdp
    for (mpdp=mpd, sdp=sd; mpdp < mpd+len_mdfile+len_prfile; mpdp++)
        mpdp->sdp = find_closest_sdp (mpdp->tx_epoch_ms, sdp, len_srfile);
    */
    
    //
    // merge service data with uplink (tx) log
    // 
    tsd = (struct s_merged_txlog_service *) 
        malloc (sizeof (struct s_merged_txlog_service) * ((PR_BUFFER_SIZE + SD_BUFFER_SIZE)));
    if (tsd==NULL)
        FATAL("Could not allocate storage for combined uplink and service array%s\n", "")

    tsdp = tsd; 
    merge_txlog_service (td, len_tdfile, sd, len_srfile, tsdp); 

    //
    // record service state and channel degradation in frame data list
    //
    int service_state[3] = {1,1,1};     // current 1 = in_service; 0 = out of service
    double service_transition_TS[3];    // epoch_ms whne the service state transition took place
    double ch_quality_transition_TS[3];    // epoch_ms when the quality transitioni took place
    int ch_quality_state[3] = {1, 1, 1};// 0 = good quality; 1 = degraded quality
    int enc_quality_state;              // 0 = if encoder operating at full rate, 1 if lower rate
    double enc_quality_transition_TS;   // epoch_ms when encoder quality state transition took place
    int channel;                        // temp channel identifier

    int i; 
    for (i=0, tsdp=tsd, fdp=fd; i < (len_tdfile + len_srfile); i++, tsdp++) {
        if (tsdp->tdp1_sdp0 == 1) { // uplink (tx) log

            int sum_ch_quality = ch_quality_state[0] + ch_quality_state[1] + ch_quality_state[2];
            fdp = find_frame (fdp, frame_count, tsdp->tdp->epoch_ms);
            if (fdp==NULL) break; // reached end of frames
            channel = tsdp->tdp->channel; 

            // socc
            fdp->socc[channel] = tsdp->tdp->occ; 
            fdp->socc_transition[channel]++;
            fdp->socc_transition_TS[channel] = tsdp->tdp->epoch_ms; 

            // channel quality state machine
            if (ch_quality_state[channel] == GOOD_QUALITY) {
                if (tsdp->tdp->occ > br_reduction_threshold - 
                    // reduce the threshold if 2 of 3 channels are already degraded
                    ((sum_ch_quality >=2)? 0 : 0)) {
                    ch_quality_state[channel] = POOR_QUALITY; 
                    ch_quality_transition_TS[channel] = tsdp->tdp->epoch_ms; 
                    fdp->ch_quality_state[channel] = POOR_QUALITY;
                    fdp->ch_quality_state_transition[channel]++; 
                } // transition to POOR_QUALITY
            } // channel in good quality state

            else { // channel is in poor quality state
                // channel in poor quality; check if ready to transition
                if ((service_state[channel] == IN_SERVICE) 
                    &&
                    // sufficient time has expired since the channel came into service
                    (tsdp->tdp->epoch_ms > service_transition_TS[channel] + br_increase_latency)
                    // channel has been in current state for sufficient ducation
                    // commented out since minimum duration filter is implemented in the encoder state machine
                    // &&
                    // (tsdp->tdp->epoch_ms > ch_quality_transition_TS[channel] + min_state_duration_latency)
                    )
                    if (tsdp->tdp->occ < br_increase_threshold) {
                        ch_quality_state[channel] = GOOD_QUALITY;
                        ch_quality_transition_TS[channel] = tsdp->tdp->epoch_ms; 
                        fdp->ch_quality_state[channel] = GOOD_QUALITY;
                        fdp->ch_quality_state_transition[channel]++; 
                    } // transition to GOOD_QUALITY
            } // channel in poor quality

            // encoder quality state machine 
            if (enc_quality_state == GOOD_QUALITY) {
                if (sum_ch_quality == 3) { // all channels in bad shape
                    enc_quality_state = POOR_QUALITY; 
                    enc_quality_transition_TS = tsdp->tdp->epoch_ms; 
                    fdp->enc_quality_state = POOR_QUALITY; 
                    fdp->enc_quality_state_transition = 1; 
                }
            } // encoder in good quality state
            else { // encoder not in good quality state
                if (sum_ch_quality < 3) // at least one channel has recovered
                    // check sufficient time has expried since encoder entered poor quality state
                    if (tsdp->tdp->epoch_ms > (enc_quality_transition_TS + min_state_duration_latency)) {
                        enc_quality_state = GOOD_QUALITY; 
                        enc_quality_transition_TS = tsdp->tdp->epoch_ms; 
                        fdp->enc_quality_state = GOOD_QUALITY; 
                        fdp->enc_quality_state_transition = 1; 
                    } // if sufficient time has expired
            } // encoder not in good quality state
        } // uplink (tx) log entry

        else { // service log
            fdp = find_frame (fdp, frame_count, tsdp->sdp->state_transition_epoch_ms); 
            if (fdp==NULL) break; // reached end of the frames
            channel = tsdp->sdp->channel; 
            fdp->service_state_transition[channel]++;
            fdp->service_state[channel] = tsdp->sdp->state; 
            fdp->service_state_transition_TS[channel] = tsdp->sdp->state_transition_epoch_ms; 
            service_state[channel] = tsdp->sdp->state; 
            service_transition_TS[channel] = tsdp->sdp->state_transition_epoch_ms; 
        } // service log entry
    } // for all entries in the tsd array

    // update state for the frames where there was no change
    // first frame was initialized by create_frame_list and does not need updates
    for (i=1, fdp = fd+1; i < frame_count; i++, fdp++) { 
        int j; 
        for (j=0; j<3; j++) {
            if (!fdp->socc_transition[j])
                // continue with the socc of the previous frame
                fdp->socc[j] = (fdp-1)->socc[j];

            if (!fdp->ch_quality_state_transition[j])
                // continue with the quality state of the previous frame
                fdp->ch_quality_state[j] = (fdp-1)->ch_quality_state[j];

            if (!fdp->service_state_transition[j])
                // continue with the quality state of the previous frame
                fdp->service_state[j] = (fdp-1)->service_state[j];
        } // for all channels

        if (!fdp->enc_quality_state_transition)
            fdp->enc_quality_state = (fdp-1)->enc_quality_state; 
    } // for all frames

    // 
    // emit outputs
    //
    // print headers
    emit_frame (1, fd, out_fp);
    emit_per_carrier (1, 0, fd, out_fp); 
    emit_per_carrier (1, 1, fd, out_fp); 
    emit_per_carrier (1, 2, fd, out_fp); 
    fprintf (out_fp, "\n"); 

    // print rest of the lines, one per frame
    for (i=0, fdp = fd; i<frame_count; i++, fdp++) {
        emit_frame (0, fdp, out_fp);
        emit_per_carrier (0, 0, fdp, out_fp); 
        emit_per_carrier (0, 1, fdp, out_fp); 
        emit_per_carrier (0, 2, fdp, out_fp); 
        fprintf (out_fp, "\n"); 
    } // for all frames 

    // close files so we can process the next set in the table
    fclose (pr_fp); fclose (md_fp); fclose (out_fp); fclose(warn_fp); fclose (td_fp); 

    if (++file_index < len_file_table) goto PROCESS_EACH_FILE;    

    // else exit
    my_exit (0); 
} // end of main