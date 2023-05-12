#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#define	FATAL(STR, ARG) {printf (STR, ARG); my_exit(-1);}
#define	WARN(STR, ARG) {if (!silent) printf (STR, ARG);}
#define	FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MATCH   0
#define HZ_30   0
#define HZ_15   1
#define HZ_10   2
#define HZ_5    3
#define RES_HD  0
#define RES_SD  1
#define CRITICAL_RUN_LENGTH 20
#define MD_BUFFER_SIZE (20*60*30*15)
#define MAX_LINE_SIZE    1000 
#define MAX_SD_LINE_SIZE 1000
#define NUM_OF_MD_FIELDS    12
#define NUM_OF_PARAMS 3
#define NUMBER_OF_BINS      20
#define LATENCY_BIN_SIZE    10
#define BIT_RATE_BIN_SIZE   0.5
#define MAX_PACKETS_IN_A_FRAME  30
#define MIN_PACKETS_IN_A_FRAME  1
#define MAX_TRANSIT_LATENCY_OF_A_FRAME  500
#define MIN_TRANSIT_LATENCY_OF_A_FRAME  100
#define MAX_BYTES_IN_A_FRAME    25000
#define MIN_BYTES_IN_A_FRAME    2000
#define MAX_LATE_PACKETS_IN_A_FRAME     20
#define MIN_LATE_PACKETS_IN_A_FRAME     0
#define MAX_MISSING_PACKETS_IN_A_FRAME  20
#define MIN_MISSING_PACKETS_IN_A_FRAME  0 
#define MAX_OOO_PACKETS_IN_A_FRAME  20
#define MIN_OOO_PACKETS_IN_A_FRAME  0 
#define MAX_BIT_RATE_OF_A_FRAME 10
#define MIN_BIT_RATE_OF_A_FRAME 0
#define MAX_NUM_OF_ANNOTATIONS 100
#define MAX_C2V_LATENCY 200
#define MIN_C2V_LATENCY 20
#define MAX_C2R_LATENCY 200
#define MIN_C2R_LATENCY 20
#define MAX_T2R_LATENCY 100
#define MIN_T2R_LATENCY 10
#define MAX_EST_T2R_LATENCY 500
#define MIN_EST_T2R_LATENCY 10
// #define FRAME_PERIOD_MS 33.34 33.34049421
// #define FRAME_PERIOD_MS 33.36490893
// #define FRAME_PERIOD_MS 33.34136441
#define FRAME_PERIOD_MS 33.3656922228
#define MAX_GPS			25000				// maximum entries in the gps file. fatal if there are more.
#define TX_BUFFER_SIZE (20*60*1000)
#define CD_BUFFER_SIZE (20*60*1000)
#define BRM_BUFFER_SIZE (20*60*100*3)
#define AVGQ_BUFFER_SIZE (20*60*1000*3)
#define LD_BUFFER_SIZE (20*60*1000)
#define SD_BUFFER_SIZE (20*60*1000)
#define FD_BUFFER_SIZE (20*60*30)
#define IN_SERVICE 1
#define OUT_OF_SERVICE 0

struct s_file_table_entry {
    char rx_pre[500];           // receive side file name prefix
    char tx_pre[500];           // transmit side file name prefix
};

struct s_service {
    int state;                  // 1=IN-SERVICE 0=OUT-OF-SERVICE
    double state_transition_epoch_ms; // time stamp of state transition
    int bp_t2r;                 // back propagated t2r at the state_transition_epoch_ms
    double bp_t2r_epoch_ms;     // time when this back propagated info was received
    int bp_packet_num;          // packet number whose t2r was back propagated
    int est_t2r;                // estimated t2r at state_transition_epoch_ms
    int socc;                   // sampled occupancy at state_transition_epoch_ms
    int zeroUplinkQueue;       // 1 if occupancy monitor is working
};

struct s_latency {
    int packet_num;             // packet number
    int t2r_ms;                 // latency for it 
    double bp_epoch_ms;         // time bp info was received by the sendedr
}; 

struct s_coord {
	double	lon;
	double	lat;
}; 

struct s_brmdata {
    int bit_rate;
    int encoder_state;
    int channel_quality_state[3];
    double epoch_ms;
}; 

struct s_avgqdata {
    int channel;
    float rolling_average;
    int quality_state;
    int queue_size;
    double tx_epoch_ms;
    int packet_num;
}; 

struct s_txlog {
    int channel;                    // channel number 0, 1 or 2
    double epoch_ms;                // time modem occ was sampled
    int occ;                        // occupancy 0-30
    int time_since_last_update;     // of occupancy for the same channel
    int actual_rate;
};

struct s_cmetadata {
    int packet_num;                 // packet number read from this line of the carrier s_metadata finle 
    double vx_epoch_ms;
    double tx_epoch_ms; 
    double rx_epoch_ms;
    int socc; 
    int retx; 
    struct s_avgqdata *avgqdp;
    int len_avgqd; 
    struct s_latency *ldp;          // latency data for this packet
    struct s_latency *lsp;          // latency data for the packet whose bp_TS is closest to tx_TS of this packet
    int len_ld; 
    struct s_brmdata *brmdp;
}; 

struct s_metadata {      // dedup meta data
    int         packet_num;                 // incrementing number starting from 0
    double      vx_epoch_ms;                // time since epoch in ms
    double      tx_epoch_ms;                // time since epoch in ms
    double      rx_epoch_ms;                // time sicne epoch in ms
    int         socc;                       // sampled occupancy. 31 if no information avaialble from the mdoem
    unsigned    video_packet_len;           // packet length in bytes
    unsigned    frame_start;                // 1-bit flag; set if this packet is the start of a new frame
    unsigned    rolling_frame_number;       // 4-bit rolling frame number
    unsigned    frame_rate;                 // 2-bit field 0: 30Hz, 1: 15Hz, 2: 10_hz, 3: 5Hz
    unsigned    frame_resolution;           // 2-bit field 0: HD (1920x1080), 1: SD (960x540), 2/3: reserved
    unsigned    frame_end;                  // 1-bit flag; set if this packet is t end of the frame
    double      camera_epoch_ms;            // time since epoch in ms
    unsigned    retx;                       // retransmission index for the packet 0/2=Q0/2 1=Q1 (re-tx)
    unsigned    ch;                         // carrirer 0=ATT, 1=VZ, 2=TMobile
    int         kbps;                       // bit-rate of the channel servicing this packet
};

struct frame {
    int         frame_count;                // frame number. starts at 1
    unsigned    size;                       // size in bytes
    unsigned    late;                       // number of late packets in this frame
    unsigned    missing;                    // number of missing packets in this frames
    unsigned    out_of_order;               // number of out of order packets in this frame
    int         first_packet_num;           // first packet number of the frame
    int         last_packet_num;            // last packet number of the frame
    unsigned    packet_count;               // number of packets in this frame
    double      tx_epoch_ms_1st_packet;     // tx timestamp of the first packet of the frame
    double      tx_epoch_ms_last_packet;    // tx timestamp of the last packet of the frame
    double      rx_epoch_ms_last_packet;    // rx timestamp of last packet of the frame
    double      rx_epoch_ms_earliest_packet;// rx time stamp of the earliest arriving packet for this frame
    double      rx_epoch_ms_latest_packet;  // rx timestamp of the late-most packet of the frame (not necessarily last packet)
    unsigned    latest_packet_count;        // the latest packet count of the frame
    int         latest_packet_num;          // the latest packet number of the frame
    unsigned    latest_retx;                // queue the latest packet came from
    unsigned    frame_rate;                 // in HZ
    unsigned    frame_resolution;           // HD or SD
    double      camera_epoch_ms;            // camera timestamp of this frame
    double      nm1_camera_epoch_ms;        // previous frame's camera time stamp
    unsigned    has_annotation;             // set to 1 if this frame was marked in the annotation file; else 0

    float       nm1_bit_rate;               // bit rate of previous (n-1) frame in Mbps
    unsigned    nm1_size;                   // size of the previous frame
    double      nm1_rx_epoch_ms_earliest_packet;    //rx time stamp of the earliest arriving packet for the previous (n-1) frame

    double      cdisplay_epoch_ms;          // the time stamp (current) where this frame is expeceted to be displayed
    double      ndisplay_epoch_ms;          // the time stamp when this frame's display will end. Next frame should arrive by this itme.
    double      display_period_ms;          // 1/frame rate
    double      early_ms;                   // how early did this frame arrived relative to start of display of previous
    int         repeat_count;               // number of times this frame caused a previous frame to be repeated
    int         skip_count;                 // if 1 then this frame will cause previous frame to be skipped
    float       c2d_frames;                 // camera to display latency in units of frame time
    int         fast_channel_count;         // number of packets in the frame that had a fast channel available

	struct s_coord	coord;					// interpolated coordinates based on gps file
	float			speed; 					// interpolated speed of this frame

    struct s_brmdata *brmdp;                // bit-rate modulation for this frame

    int brm_changed;                        // 1 if this encoder quality state change in this frame
    int brm_run_length;                     // valid if brm_changed==1 and counts run length of the previous brm state
}; // frame

struct stats {
    unsigned    count;
    double      mean; 
    double      var;
    double      min;
    double      max;
    double      distr[NUMBER_OF_BINS];
};

struct s_session {
//  unsigned        frame_count;            // total fames in the session
    struct stats    pc, *pcp;               // packet count stats: count=total packets in the session, rest per frame
    struct stats    l, *lp;                 // latecncy stats: count=n/a, rest per frame
    struct stats    bc, *bcp;               // byte count stats: count = total bytes in the session, rest per frame
    struct stats    i, *ip;                 // Frames with missing packets stats: count = number of incomplete frames in the session; 
    struct stats    o, *op;                 // Frames with out of order packets stats: count = number of OOO frames in the session; 
    struct stats    late, *latep;           // late frame stats; count = number of late frames in the session; 
    struct stats    br, *brp;               // frame bit-rate stats: count = number of frames below too_low_bit_rate parameter
    struct stats    cts, *ctsp;             // camera time-stamp
    struct stats    c2d, *c2dp;             // camera to display latency
    struct stats    rpt, *rptp;             // repeated frames
    // packet level stats
    struct stats    c2v, *c2vp;             // camera to video latency
    struct stats    v2t, *v2tp;             // best encoder to transmit for a packet
    struct stats    c2r, *c2rp;             // camera to receiver latency
    struct stats    best_t2r, *best_t2rp;   // best tx to rx delay for a packet
    struct stats    c0_est_t2r, *c0_est_t2rp;     // estimated t2r
    struct stats    c1_est_t2r, *c1_est_t2rp;     // estimated t2r
    struct stats    c2_est_t2r, *c2_est_t2rp;     // estimated t2r
};

struct s_carrier {
    struct s_txlog *tdhead;             // pointer to the tx data array
    int len_td;                         // number of entries in the td array

    // current packet info
    struct s_cmetadata *cdhead;            // carrier metadata array sorted by packet number
    struct s_cmetadata *csp;            // carrier metadata array sorted by rx TS
    int len_cd;                         // number of enteries in the cd array

    int packet_num;                     // packet number read from this line of the carrier s_metadata finle 
    double vx_epoch_ms; 
    double tx_epoch_ms; 
    double rx_epoch_ms;
    double ert_epoch_ms;                // expected time to retire from the in-transit queue
    int retx;                           // retransmission index
    float t2r_ms;                       // tx-rx latency of the packet
    float r2t_ms;                       // back propagated rx-tx latency of the packet
    float est_t2r_ms;                   // estimated t2r of this packet based on bp info
    float ert_ms;                       // expected duration to retire from in-transit queue
    int socc;                           // sampled occupancy either from the tx log or the rx metadata file
    int usocc;                          // unclipped sampled occupancy
    int iocc;                           // interpolated occupancy from tx log
    double socc_epoch_ms;               // time when the occupacny for this packet was sampled

    int tx;                             // set to 1 if this carrier tranmitted the packet being considered
    int cr;                             // set to 1 if this carrier supplied the dedup output
    int cr_value_ms;                    // how far ahead was this carrier against second best
    struct s_cmetadata *bp_csp;         // pointer to the carrier metadata that provided the bp info

    int last_run_length;                // run length of the last in_serivice state
    int run_length;                     // run length of the current in_service state
    int start_of_run_flag;                // previous run length finished with last packet
    int fast_run_length;                // # of packets with socc < 10 in a run
    int last_fast_run_length;           // # of packets in the last fast run
    int pending_packet_count;           // number of packets pending to be txed when channel goes in service
    int tgap;                           // gen time of the lastest packet - tx time of resuming packet
    int critical;                       // 1 if this pending packet used by dedup
    int enable_indicator; 
    int indicator;                      // indicator function to mark start of critical pkt delivery
    double state_xp_TS;                 // TS of closest state transition of this packet 
    double prev_state_xp_TS;            // TS of state transition closest to state_xp
    double out_of_service_duration;     // duration of the last out service. valid ONLY at the start of a run
    int socc_when_going_oos;             // occupancy when channel was going out of service
    int unused_pkts_at_start_of_run;    // number of packets not used by dedup at the start of a run
    int continuing_channel;             // 1 if ths channel retired the resuming packet - 1

    double prev_pkt_tx_epoch_ms;        // tx time stamp of previous packet
    double prev_pkt_state_xp_TS;        // TS of closest state transition of previous packet
    int prev_pkt_num;                // packet num of previous transmitted packet

    int resume_from_beginning;          // 0=not start of run 1 = start from beginning of the queue 2 = do not
    int channel_x_in_good_shape;        // 1 if channel x is in good shape
    int channel_y_in_good_shape;        // 1 if channel y is in good shape
    int reason_debug; 
    int bp_TS_gap;                      // gap between bp_TS of this and other channesl for current packet - 1 (last retired packet)
    int skip_pkts;                      // number of packets to skip at the start of the run
    int unretired_pkts_debug;           // unretired packets from other channel

    // structures associated with current packet
    struct s_avgqdata *avgqdp; 
    int len_avgqd;
    struct s_latency *ldp, *lsp;        // ldp sorted by packet_num, lsp sorted by bp_epoch_ms
    int len_ld; 
    struct s_brmdata *brmdp;
    struct s_txlog *tdp; 
    // len_brmd is a global

    int probe;                          // 1=probe packet 0=data packet
}; // s_carrier

struct s_gps {
	double 			epoch_ms; 					// time stamp
	int 			mode;						// quality indicator
	struct s_coord	coord; 						// lon, lat of the gps point (at 1Hz)
	float			speed; 						// vehicle speed 
	int				count; 						// number of frames that mapped into this gps coord
}; 

// frees up storage before exiting
void  my_free (); 
int my_exit (int n);

// reads a new meta data line. returns 0 if reached end of the file
int read_md (
    int skip_header,                        // if 1 then skip the header lines
    FILE *fp, 
    struct s_metadata *p);    

// initializes the frame structure for a new frame
void init_frame (
    struct frame *fp,
    struct s_metadata *lmdp,                 // last packet's meta data
    struct s_metadata *cmdp,                 // current packet's meta data; current may be NULL at EOF
    int    first_frame);                    // 1 if first frame

// updates per packet stats of the frame
void update_frame_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct s_metadata *lmdp,                 // last packet's meta data
    struct s_metadata *cmdp);                // current packet's meta data; current may be NULL at EOF

// called after receiving the last packet of the frame and updates brm stats
void update_brm (struct frame *fp);

// calculates and update bit-rate of n-1 frame
void update_bit_rate (                      // updates stats of the last frame
    struct frame *fp,
    struct s_metadata *lmdp,                 // last packet's meta data
    struct s_metadata *cmdp);                 // current packet's meta data; current may be NULL at EOF

void initialize_cp_from_cd (
    struct s_metadata *mdp,
    struct s_carrier *cp, 
    int t_mobile);

// assumes called at the end of a frame after frame and session stats have been updated
void emit_frame_stats (
    int print_header,                       // set to 1 if only header is to be emitted
    struct frame *p);                        // current frame pointer

// initializes the session stat structures for the specified metrics
void init_metric_stats (
    struct stats *p);

// initializes session stats
void init_session_stats (
    struct s_session *p);

// Computes the mean/variance of the specified metric. 
void compute_metric_stats (
    struct stats *p, 
    unsigned count);

// Collects data for calculating mean/variance/distribution for the specified metric
void update_metric_stats (
    struct stats *p, 
    unsigned count,                         // number of frames this metric occurred in the session
    double value,                            // value of the metric in the frame
    double range_max,                        // max value this metric can take in a frame
    double range_min);                       // min value this mnetric can take in a frame

// updates session stats. assumes that it is called only at the end of a frame
void update_session_frame_stats (                 // updates stats for the specified frame
    struct s_session *ssp,
    struct frame *p);

// updates session per packet stats - called after every meta data line read
void update_session_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct s_metadata *mdp, 
    struct s_session *ssp); 

// emits the stats for the specified metric
void emit_metric_stats (
    char            *p1,                     // name of the metric
    char            *p2,                     // name of the metric
    struct stats    *s,                     // pointer to where the stats are stored
    int             print_count,            // count not printed if print_count = 0
    double          range_max,              // min and max range this metric can take in a frame
    double          range_min);

// outputs session  stats
void emit_session_stats (
    struct s_session *ssp,                  // session stats
    struct frame *fp                        // current frame
);

void per_packet_analytics (
    struct s_session *ssp, 
    struct frame *fp,
    struct s_metadata *mdp,
    struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p);

void carrier_metadata_clients (int print_header, struct s_session *ssp, struct frame *fp, 
    struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p);

// prints program usage
void print_usage (void);

// tracks length in packets reasoably faset (occ<=10) of service of a resuming channel
void run_length_state_machine (int channel, struct s_carrier *cp, 
    struct s_service *sd, int len_sd, struct frame *fp, double c2t, 
    int dedup_used_this_channel);

// returns the next channel state
int channel_state_machine (int channels_in_service, struct s_carrier *cp);

// finds the backpropagated t2r at the time tx_epoch_ms
// float bp_t2r_ms (struct s_carrier *cp, struct s_cmetadata *csp, int len_cs);

void init_packet_stats (
    struct s_txlog *tdhead, int len_td, 
    struct s_cmetadata *cdhead, int len_cd, 
    struct s_cmetadata *csp, 
    struct s_carrier *cp);

// returns 1 if successfully read the annotation file
int read_annotation_file (void); 

// returns 1 if the current frame count is marked in the annotation file
int check_annotation (unsigned frame_num); 

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (double *vx_epoch_ms, double *tx_epoch_ms, int *socc);

// outputs resumption time of any one of the 3 channels. If multiple channels are resuming
// at the same time then the smallest channel number is printed
void emit_resumption_stats (struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p, 
    double c2t);

// outputs per carrier stats
void emit_packet_stats (struct s_carrier *cp, struct s_metadata *mdp, int carrier_num, struct frame *fp); 

void emit_packet_header (FILE *fp);

void emit_frame_header (FILE *fp);

void skip_combined_md_file_header (FILE *fp);

void skip_carrier_md_file_header (FILE *fp, char *cname);

void print_command_line (FILE *fp);

// returns number of enteries found in the gps file
int read_gps (FILE *);							// reads and error checks the gps file

// reads per carrier metadat into cd array 
void read_cd (char *);

// reads latency log file in ld array
void read_ld (FILE *);

// reads service log file in sd array
void read_sd (FILE *);

// reads transmit log file into td array 
void read_td (FILE *);

// returns 1 if able to find coordinates for the frame; else 0
int get_gps_coord (struct frame *);

FILE *open_file (char *filep, char *modep);

// reads bit-rate modulation file into brmd array sorted by timestamp
void read_brmd (FILE *fp);

// reads bit-rate modulation file into avgqd array sorted by timestamp
void read_avgqd (FILE *fp);

// find_closest returns pointer to the td array element at closest time smaller than the 
// specified tx_epoch_ms 
struct s_txlog *find_closest_tdp (double tx_epoch_ms, struct s_txlog *head, int len_td);
    
// returns pointer to equal to or closest smaller mdp to the specified camera_epoch_ms
struct s_metadata *find_closest_mdp_by_camera_TS (double camera_epoch_ms, struct s_metadata *headp, int len);

// returns pointer closest smaller mdp by tx_epoch to the specified TS
struct s_metadata *find_closest_mdp_by_rx_TS (double TS_ms, struct s_metadata *headp, int len);

// returns pointer to equal to or closest smaller brmdata to the specified epoch_ms
struct s_brmdata *find_closest_brmdp (double epoch_ms, struct s_brmdata *headp, int len);

// returns pointer to equal to or closest smaller packet number closest to the specified tx_epoch_ms
struct s_avgqdata *find_avgqdp (int packet_num, double tx_epoch_ms, struct s_avgqdata *avgqdatap, int len_avgqd);

// returns pointer to the sdp equal to or closest smaller sdp to the specified epoch_ms
struct s_service *find_closest_sdp (double epoch_ms, struct s_service *sdp, int len_sd);

// returns pointer to the ldp equal to specified packet num with the bp_epoch closes to 
// specified epoch. If the packet num does not match, then it returns the packet cloest
// to the specified epoch
struct s_latency *find_ldp_by_packet_num (int packet_num, double rx_epoch_ms, struct s_latency *ldp, int len_ld);

// sorts latency data array by receipt epoch_ms
void sort_ld_by_bp (struct s_latency *ldp, int len);

// reads and parses a latency log file. Returns 0 if end of file reached
// ch: 0, received a latency, numCHOut:0, packetNUm: 0, latency: 28, time: 1673813692132
int read_ld_line (FILE *fp, int ch, struct s_latency *ldp);

// reads and parses a service log file. Returns 0 if end of file reached
int read_sd_line (FILE *fp, int ch, struct s_service *sdp);

// reads and parses a proble log line from the specified file. 
// returns 0 if end of file reached or // if the scan for all the fields fails due 
// to an incomplete line as in last line of the file.
int read_pr_line (
    FILE *fp, int len_tdfile, struct s_txlog *tdhead, int ch, 
    struct s_carrier *mdp);

// find_occ_from_td  returns the sampled occupancy at the closest time smaller than the specified tx_epoch_ms 
// and interpolated occupancy, interporated between the sampled occupancy above and the next (later) sample
struct s_txlog *find_occ_from_td (
    double tx_epoch_ms, struct s_txlog *tdhead, int len_td, 
    int *iocc, int *socc, double *socc_epoch_ms);

// globals - command line inputs
int     debug = 1; 
int     silent = 0;                             // if 1 then suppresses warning messages
char    ipath[500], *ipathp = ipath;            // input files directory
char    opath[500], *opathp = opath;            // output files directory
char    tx_pre[500], *tx_prep = tx_pre;         // transmit log prefix not including the .log extension
char    brm_pre[500], *brm_prep = brm_pre;      // bit-rate modulation log file
char    avgq_pre[500], *avgq_prep = avgq_pre;   // rolling average queue size log file
char    rx_pre[500], *rx_prep = rx_pre;         // receive side meta data prefix not including the .csv extension
char    ld_pre[500], *ld_prep = ld_pre;         // latency file prefix 
char    sd_pre[500], *sd_prep = sd_pre;         // service file prefix 
FILE    *md_fp = NULL;                          // meta data file name
FILE    *an_fp = NULL;                          // annotation file name
FILE    *fs_fp = NULL;                          // frame statistics output file
FILE    *ss_fp = NULL;                          // statistics output file name
FILE    *ps_fp = NULL;                          // late frame output file name
FILE    *c0_fp = NULL;                          // carrier 0 meta data file
FILE    *c1_fp = NULL;                          // carrier 0 meta data file
FILE    *c2_fp = NULL;                          // carrier 0 meta data file
FILE    *gps_fp = NULL;                         // gps data file
FILE    *td_fp = NULL;                          // transmit log data file
FILE    *ld_fp = NULL;                          // latency log data file
FILE    *sd_fp = NULL;                          // service log data file
FILE    *brmd_fp = NULL;                        // bit-rate modulation data file pointer
FILE    *avgqd_fp = NULL;                       // rolling average queue size data file pointer
FILE    *warn_fp = NULL;                        // warning outputs go to this file
FILE    *dbg_fp = NULL;                       // debug output file
char    annotation_file_name[500];              // annotation file name
float   minimum_acceptable_bitrate = 0.5;       // used for stats generation only
unsigned maximum_acceptable_c2rx_latency = 110; // frames considered late if the latency exceeds this 
unsigned anlist[MAX_NUM_OF_ANNOTATIONS][2];     // mmanual annotations
int     len_anlist=0;                           // length of the annotation list
int     have_carrier_metadata = 0;              // set to 1 if per carrier meta data is available
int     have_tx_log = 0;                        // set to 1 if transmit log is available
int     have_ld_log = 0;                        // set to 1 if latency log is available
int     have_sd_log = 0;                        // set to 1 if service log is available
int     have_brm_log = 0;                       // 1 if bit-rate modulation log is available
int     have_avgq_log = 0;                      // 1 if rolling average log is available
int     new_sendertime_format = 2;              // set to 1 if using embedded sender time format
int     verbose = 1;                            // 1 or 2 for the new sender time formats
int     rx_jitter_buffer_ms = 10;               // buffer to mitigate skip/repeats due to frame arrival jitter
int     fast_channel_t2r = 40;                  // channels with latency lower than this are considered fast
int     fast_frame_t2r = 60;                    // frames with latency lower than this are considered fast
int     frame_size_modulation_latency = 3;      // number of frames the size is expected to be modulated up or down
int     frame_size_modulation_threshold = 6000; // size in bytes the frame size is expected to be modulated below or aboveo
char    comamnd_line[5000];                     // string store the command line 

// globals - storage
struct  s_metadata md[MD_BUFFER_SIZE];           // buffer to store meta data lines*/
int     len_md=1; 
int     md_index;                               // current meta data buffer pointer
struct  s_gps gps[MAX_GPS];                     // gps data array 
int	    len_gps; 		                        // length of gps array
struct  s_cmetadata *cd0=NULL, *cd1=NULL, *cd2=NULL; // per carrier metadata file stored by packet number
int     len_cd0=0, len_cd1=0, len_cd2=0;        // len of the transmit data logfile
struct  s_cmetadata *cs0=NULL, *cs1=NULL, *cs2=NULL; // per carrier metadata file stored by rx TS
struct  s_txlog *td0=NULL, *td1=NULL, *td2=NULL;// transmit log file stored in this array
int     len_td0=0, len_td1=0, len_td2=0;        // len of the transmit data logfile
struct  s_brmdata *brmdata = NULL;              // pointer to the brm data array
int     len_brmd=0; 
struct  s_avgqdata *avgqd0=NULL, *avgqd1=NULL, *avgqd2=NULL; // rolling avg data array
int     len_avgqd0=0, len_avgqd1=0, len_avgqd2=0;        
// struct  s_service *sd;                          // service log data array
// int     len_sd; 
struct  s_latency *ld0=NULL, *ld1=NULL, *ld2=NULL;  // latency log data array sorted by packet num
struct  s_latency *ls0=NULL, *ls1=NULL, *ls2=NULL;  // latency log data array sorted by bp_epoch_ms
int     len_ld0=0, len_ld1=0, len_ld2=0;
struct  s_service *sd0=NULL, *sd1=NULL, *sd2=NULL;  // serivce log data array sorted by state transition time
int     len_sd0=0, len_sd1=0, len_sd2=0;
struct  frame *fd = NULL;                           // frame array`
int     len_fd; 

int main (int argc, char* argv[]) {
    struct  s_file_table_entry file_table[100]; // file list
    int     flist_idx = 0;                      // file list index
    int     have_file_list = 0;                 // no file list read yet
    struct  s_metadata *cmdp, *lmdp;            // last and current meta data lines
    struct  frame cf, *cfp = &cf;               // current frame
    struct  s_session sstat, *ssp=&sstat;       // session stats 
    struct  s_carrier c0, c1, c2; 
    struct s_carrier *c0p=&c0, *c1p=&c1, *c2p=&c2; 

    unsigned waiting_for_first_frame;        // set to 1 till first clean frame start encountered
    char     buffer[1000], *bp=buffer;              // temp_buffer

    clock_t start, end;
    double execution_time;

    int     short_arg_count = 0; 

    //  read command line arguments
    while (*++argv != NULL) {

        // input directory
        if (strcmp (*argv, "-ipath") == MATCH) {
            strcpy (ipathp, *++argv); 
        }

        // output directory
        else if (strcmp (*argv, "-opath") == MATCH) {
            strcpy (opathp, *++argv); 
        }

        // transmit log file prefix
        else if (strcmp (*argv, "-tx_pre") == MATCH) {
            have_tx_log = 1;
            strcpy (tx_prep, *++argv); 
        }

        else if (strcmp (*argv, "-ld_pre") == MATCH) {
            strcpy (ld_prep, *++argv); 
            have_ld_log = 1; 
        }
        
        else if (strcmp (*argv, "-sd_pre") == MATCH) {
            strcpy (sd_prep, *++argv); 
            have_sd_log = 1; 
        }
        
        // bit-rate modulation logfile
        else if (strcmp (*argv, "-brm_pre") == MATCH) {
            have_brm_log = 1;
            strcpy (brm_prep, *++argv); 
        }

        // rollign average log file
        else if (strcmp (*argv, "-avgq_pre") == MATCH) {
            have_avgq_log = 1;
            strcpy (avgq_prep, *++argv); 
        }

        // per carrier meta data availabiliyt
        else if (strcmp (*argv, "-mdc") == MATCH) {
            have_carrier_metadata = 1; 
        }

        // gps data file
        else if (strcmp (*argv, "-gps") == MATCH) {
            sprintf (bp, "%s%s.csv", ipathp, *++argv); 
            if ((gps_fp = open_file (bp, "r")) != NULL)
                len_gps = read_gps(gps_fp);
        }

        // annotation file
        else if (strcmp (*argv, "-a") == MATCH) {
            sprintf (bp, "%s%s.csv", ipathp, *++argv); 
            if ((an_fp = open_file (bp, "r")) != NULL)
                read_annotation_file (); 
        }

        // new sender time format
        else if (strcmp (*argv, "-ns1") == MATCH) {
            new_sendertime_format = 1; 
        }

        // new sender time format
        else if (strcmp (*argv, "-ns2") == MATCH) {
            new_sendertime_format = 2; 

        // verbose
        } else if (strcmp (*argv, "-v") == MATCH) {
            verbose = 1; 
        }

        // maximum_acceptable_c2rx_latency
        else if (strcmp (*argv, "-l") == MATCH) {
            if (sscanf (*++argv, "%u", &maximum_acceptable_c2rx_latency) != 1) {
                printf ("missing maximum acceptable camera->rx latency specification\n");
                print_usage (); 
                my_exit (-1); 
            }
        }

        // minimum_acceptable_bitrate
        else if (strcmp (*argv, "-b") == MATCH) {
            if (sscanf (*++argv, "%f", &minimum_acceptable_bitrate) != 1) {
                printf ("missing minimum acceptable bit rate specification\n");
                print_usage (); 
                my_exit (-1); 
            }
        }
        
        // fast frame definition
        else if (strcmp (*argv, "-ff") == MATCH) {
            if (sscanf (*++argv, "%d", &fast_frame_t2r) != 1) {
                printf ("Missing specification of the fast latency of frames\n");
                print_usage (); 
                my_exit (-1); 
            }
        }
        
        // frame size modulation latency
        else if (strcmp (*argv, "-ml") == MATCH) {
            if (sscanf (*++argv, "%d", &frame_size_modulation_latency) != 1) {
                printf ("Missing specification of the frame_size_modulation_latency\n");
                print_usage (); 
                my_exit (-1); 
            }
        }
        
        // frame size modulation threshold
        else if (strcmp (*argv, "-mt") == MATCH) {
            if (sscanf (*++argv, "%d", &frame_size_modulation_threshold) != 1) {
                printf ("Missing specification of the frame_size_modulation_threshold\n");
                print_usage (); 
                my_exit (-1); 
            }
        }
        
        // fast channel definition
        else if (strcmp (*argv, "-fc") == MATCH) {
            if (sscanf (*++argv, "%d", &fast_channel_t2r) != 1) {
                printf ("Missing specification of the fast latency of channels\n");
                print_usage (); 
                my_exit (-1); 
            }
        }
        
        // rx arrival jitter buffer
        else if (strcmp (*argv, "-j") == MATCH) {
            if (sscanf (*++argv, "%d", &rx_jitter_buffer_ms) != 1) {
                printf ("missing frame arrival jitter buffer length specification \n");
                print_usage (); 
                my_exit (-1); 
            }
        }

        // help/usage
        else if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            my_exit (0); 
        }

        // receive dedup meta data file prefix
        else if (strcmp (*argv, "-rx_pre") == MATCH) {
            strcpy (rx_prep, *++argv);
        }

        // receive dedup meta data file prefix (MUST BE BEFORE -stx)
        else if (strcmp (*argv, "-srx_pre") == MATCH) {
            strcpy (file_table[flist_idx].rx_pre, *++argv); 
            short_arg_count++; 
            have_file_list = 1; 
        }
        // short tx prefix (MUST BE AFTER -srx)
        else if (strcmp (*argv, "-stx_pre") == MATCH) {
            strcpy (file_table[flist_idx].tx_pre, *++argv); 
            short_arg_count++; 
            have_file_list = 1; 
        }

        // invalid argument
        else {
            printf ("Invalid argument %s\n", *argv);
            print_usage (); 
            my_exit (-1); 
        }

        if (short_arg_count == 2) { // both -srx and -stx args recad
            flist_idx++;
            short_arg_count = 0; 
        }
    } // while there are more arguments to process

    flist_idx--; // points to the last entry of the file list or -1 if no list
    if (flist_idx < 0)
        goto SKIP_FILE_LIST; 

PROCESS_NEXT_FILE: 
    rx_prep = file_table[flist_idx].rx_pre; 
    sprintf (tx_prep, "%s_%s", "uplink_queue", file_table[flist_idx].tx_pre); 
    sprintf (ld_prep, "%s_%s", "latency", file_table[flist_idx].tx_pre); 
    sprintf (sd_prep, "%s_%s", "service", file_table[flist_idx].tx_pre); 
    sprintf (brm_prep, "%s_%s", "bitrate", file_table[flist_idx].tx_pre); 
    sprintf (avgq_prep, "%s_%s", "avgQ", file_table[flist_idx].tx_pre); 
    have_tx_log = have_ld_log = have_sd_log = have_brm_log = have_avgq_log = 1;

    printf ("rx_prep: %s\n", rx_prep); 
    printf ("tx_prep: %s\n", tx_prep); 
    printf ("ld_prep: %s\n", ld_prep); 
    printf ("sd_prep: %s\n", sd_prep); 
    printf ("brm_prep: %s\n", brm_prep); 
    printf ("avgq_prep: %s\n", avgq_prep); 

SKIP_FILE_LIST: 
    start = clock();

    // initialize variables for next iteration
	len_md=1; 
	len_cd0=0, len_cd1=0, len_cd2=0;
	len_td0=0, len_td1=0, len_td2=0;
	len_brmd=0; 
	len_avgqd0=0, len_avgqd1=0, len_avgqd2=0;        
	len_ld0=0, len_ld1=0, len_ld2=0;
	len_sd0=0, len_sd1=0, len_sd2=0;

    // open remaining input and output files

    // output files
    sprintf (bp, "%s%s_packet_vqfilter.csv",opath, rx_prep); 
    ps_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_frame_vqfilter.csv", opath, rx_prep);
    fs_fp = open_file (bp, "w");
            
    sprintf (bp, "%s%s_session_vqfilter.csv", opath, rx_prep);
    ss_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_warnings_vqfilter.txt", opath, rx_prep);
    warn_fp = open_file (bp, "w");
    if (debug)
        sprintf (bp, "%s%s_debug_vqfilter.txt", opath, rx_prep);
        dbg_fp = open_file (bp, "w");

    // bit-rate modulation log file
    if (have_brm_log) {
         sprintf (bp, "%s%s.log", ipathp, brm_prep); 
        if ((brmd_fp = open_file (bp, "r")) != NULL) {
            read_brmd(brmd_fp);
        }
    }

    // rolling average queue size log file
    if (have_avgq_log) {
         sprintf (bp, "%s%s.log", ipathp, avgq_prep); 
        if ((avgqd_fp = open_file (bp, "r")) != NULL) {
            read_avgqd(avgqd_fp);
        }
    }

    // transmit (uplink queue size) log file
    if (have_tx_log) {
         sprintf (bp, "%s%s.log", ipathp, tx_prep); 
        if ((td_fp = open_file (bp, "r")) != NULL) {
            read_td(td_fp);
        }
    }

    // latency log file
    if (have_ld_log) {
         sprintf (bp, "%s%s.log", ipathp, ld_prep); 
        if ((ld_fp = open_file (bp, "r")) != NULL) {
                read_ld(ld_fp);
        }
    }

    // service log file
    if (have_sd_log) {
         sprintf (bp, "%s%s.log", ipathp, sd_prep); 
        if ((sd_fp = open_file (bp, "r")) != NULL) {
                read_sd(sd_fp);
        }
    }

    // dedup metadata file
    sprintf (bp, "%s%s.csv", ipathp, rx_prep);
    md_fp = open_file (bp, "r");
    skip_combined_md_file_header (md_fp);
    while (read_md (0, md_fp, md+len_md) != 0) {
        len_md++; 
        if (md_index == MD_BUFFER_SIZE) 
            FATAL("Dedump meta data buffer full. Increase MD_BUFFER_SIZE\n", "")
    }
        
    // per carrier meta data files
    if (have_carrier_metadata) {
        sprintf (bp, "%s%s", ipathp, rx_prep); 
        read_cd (bp); 
    } // per carrier meta data files

    // check if any missing arguments
    if (md_fp==NULL || fs_fp==NULL || ss_fp==NULL || ps_fp==NULL) {
        print_usage (); 
        my_exit (-1);
    }

    // control initialization
    waiting_for_first_frame = 1; 

    // metadata structures intializetion
    lmdp = md; 
    lmdp->rx_epoch_ms = -1; // so out-of-order calculations for the first frame are correct.
    cmdp = lmdp+1; 
    md_index = 1; 
    fd = (struct frame *) malloc (sizeof (struct frame) * FD_BUFFER_SIZE); 
    struct frame *fdp = fd; 
    len_fd = 0;

    // session stat structure initialization
    init_session_stats (ssp);

    // packet structures intialization
    init_packet_stats (td0, len_td0, cd0, len_cd0, cs0, c0p);
    init_packet_stats (td1, len_td1, cd1, len_cd1, cs1, c1p);
    init_packet_stats (td2, len_td2, cd2, len_cd2, cs2, c2p);

    // print headers
    emit_frame_header (fs_fp);
    emit_packet_header (ps_fp);
    // print_command_line (ss_fp);

    // while there are more lines to read from the meta data file
    while (md_index < len_md) { 
        cmdp = md + md_index; 

        if (cmdp->camera_epoch_ms == 0)
            printf ("camera_epoch=0\n");

        //  if first frame then skip till a clean frame start 
        if (waiting_for_first_frame) {
            if (cmdp->frame_start) {
                // found a clean start
                init_frame (cfp, lmdp, cmdp, 1); // first frame set to 1
                get_gps_coord (cfp); 
                update_session_packet_stats (cfp, cmdp, ssp);
                waiting_for_first_frame = 0;
            } else 
                // skip packets till we find a clean start 
                ; 
        } 

        // else if conntinuation of the current frame
        else if (cmdp->rolling_frame_number == lmdp->rolling_frame_number) {

            // update per packet stats of the current frame
            update_frame_packet_stats (cfp, lmdp, cmdp);
            update_session_packet_stats (cfp, cmdp, ssp);
        }

        // else start of a new frame. 
        // lmdp has the last packet of the current frame. cmdp first of the new frame. 
        else {

            // first finish processing the last frame

            // check if the last frame ended with packets missing
            if (!lmdp->frame_end) // frame ended abruptly
                if (cmdp->frame_start) // next frame started cleanly so all the missing packets belong to this frame
                    cfp->missing += cmdp->packet_num - (lmdp->packet_num+1);
                else 
                    cfp->missing += 1; // not the correct number of missing packets but can't do better

            // check when the frame arrived and will it cause repeats or skips
		    if (cfp->frame_count == 1) { // we just reached the end of first frame
		        // set up display clock
                // by definition the first frame does not have repeat/skip
		        cfp->cdisplay_epoch_ms = cfp->rx_epoch_ms_latest_packet + rx_jitter_buffer_ms;
		        cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms;
		        cfp->repeat_count = 0; // nothing repeated yet
                cfp->skip_count = 0; 
                cfp->early_ms = 0;  // by definition not early
		    } // set up display clock

		    else { // check if this frame caused a repeat or skip (repeat_count=0 means arrived in time)

                cfp->early_ms = cfp->rx_epoch_ms_latest_packet - cfp->cdisplay_epoch_ms;

                // check if frame arrived early
                if (cfp->rx_epoch_ms_latest_packet < (cfp->cdisplay_epoch_ms - rx_jitter_buffer_ms)) {
                    // do not advance the display clock but reset repeat_count since late frames can
                    // advance the display clock by arbitrarily large amount and next frame may arrive 
                    // earlier than this simulated display
                    cfp->repeat_count = 0;
                    cfp->skip_count = 1; 
                } // of frame arrived early and will cause previous frame(s) not yet displayed to be skipped

                // check if frame arrived in time
                else if (cfp->rx_epoch_ms_latest_packet < cfp->ndisplay_epoch_ms) {
                    // advance display clock
                    // this case is separated out from late arrival to protect again numerical instability where
                    // early arrival is determined using cdisplay_epoch and late arrival using ndisplay_epoch_ms
                    cfp->repeat_count = 0; 
                    cfp->skip_count = 0; 
                    cfp->cdisplay_epoch_ms += cfp->display_period_ms; 
                    cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms; 
                } // of frame arrived in time
                
                // frame arrived late. Calculate number of repeats
                else {
                    // advance display clock
                    double delta = cfp->rx_epoch_ms_latest_packet - cfp->ndisplay_epoch_ms; 
                    cfp->repeat_count = ceil (delta/cfp->display_period_ms); 
                    cfp-> skip_count = 0; 
                    cfp->cdisplay_epoch_ms += (cfp->repeat_count+1) * cfp->display_period_ms; 
                    cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms;
                } // frame arrived in late

		    } // check if the last frame that arrived caused repeats or skips

            // camera to display latency
            cfp->c2d_frames = (cfp->cdisplay_epoch_ms - cfp->camera_epoch_ms) / cfp->display_period_ms; 

            // bit-rate of last to last frame
            update_bit_rate (cfp, lmdp, cmdp);

            // update bit-rate-modulation data
            update_brm (cfp);

            //
            // call end of frame clients
            //
            *fdp = *cfp; fdp++; len_fd++;
            emit_frame_stats (0, cfp);
            if (have_carrier_metadata)
                carrier_metadata_clients (0, ssp, (fdp-1), c0p, c1p, c2p); 
            update_session_frame_stats (ssp, cfp);


            // now process the new frame
            init_frame (cfp, lmdp, cmdp, 0);    
            get_gps_coord (cfp); 
            update_session_packet_stats (cfp, cmdp, ssp);
        } // start of a new frame

        if (cmdp->camera_epoch_ms == 0)
            printf ("camera_epoch=0\n");

        md_index = (md_index + 1) % MD_BUFFER_SIZE; 
        lmdp = cmdp;
        cmdp = md+md_index; 
    } // while there are more lines to be read from the meta data file

    if (waiting_for_first_frame) { // something horribly wrong
        printf ("ERROR: no frames found in this metadata file\n");
        my_exit (-1); 
    } 
    
    // update stats for the last frame
    // check if the last frame of the file had any missing packets
    if (!lmdp->frame_end) // frame ended abruptly
       cfp->missing += 1; // not the correct number of missing packets but can't do better
    update_bit_rate (cfp, lmdp, cmdp); 
    update_brm (cfp);
    update_session_frame_stats (ssp, cfp);

    // end of the file so emit both last frame and session stats
    emit_frame_stats(0, cfp);
    if (have_carrier_metadata)
        carrier_metadata_clients (0, ssp, cfp, c0p, c1p, c2p); 
    emit_session_stats(ssp, cfp);

    my_free (); 

    end = clock();
    execution_time = ((double)(end - start))/CLOCKS_PER_SEC;
    printf ("Program execution time = %0.1fs\n", execution_time); 

    if (flist_idx-- > 0) 
            goto PROCESS_NEXT_FILE; 
    else    return (0);

} // end of main


// returns pointer to the sdp equal to or closest smaller sdp to the specified epoch_ms
struct s_service *find_closest_sdp (double epoch_ms, struct s_service *sdp, int len_sd) {

    struct  s_service  *left, *right, *current;    // current, left and right index of the search

    left = sdp; right = sdp + len_sd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->state_transition_epoch_ms) { // move to the right
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    if (right->state_transition_epoch_ms == epoch_ms)
        current = right; 

    return current; 
} // find_closest_sdp

// returns pointer to the sdp closest smaller sdp to the specified epoch_ms. 
// equal if smaller does not exist
struct s_service *find_closest_smaller_sdp (double epoch_ms, struct s_service *sdp, int len_sd) {

    struct  s_service  *left, *right, *current;    // current, left and right index of the search

    left = sdp; right = sdp + len_sd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->state_transition_epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        }
        else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    return current; 

} // find_closest_smaller_sdp

// returns pointer to the ldp equal to specified packet num with the bp_epoch closes to 
// specified epoch. If the packet num does not match, then it returns the packet cloest
// to the specified epoch
struct s_latency *find_ldp_by_packet_num (int packet_num, double rx_epoch_ms, struct s_latency *ldp, int len_ld) {

    struct  s_latency  *left, *right, *current;    // current, left and right index of the search

    left = ldp; right = ldp + len_ld - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (packet_num > current->packet_num) {
            left = current;
            current = right - (right-left)/2;
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current (and left) is equal to (if current = left edge) or less than specified packet_num
    if (right->packet_num == packet_num) 
        left = right; 
    // else no match the back propagated packet got lost, so use the nearest smaller packet

    // a packet may be back propagated multiple times due to retx or the network. 
    // so search to the right and see which element is closest is the specified rx_epoch_ms
    // assumes that same numbered packets are in ascending bp_epoch_ms
    while ((left->bp_epoch_ms < rx_epoch_ms) && (left < (ldp+len_ld-1)))
        left++;
    return left;

} // find_ldp_by_packet_num

// returns pointer to the lsp equal to or closest smaller lsp to the specified epoch_ms
struct s_latency *find_closest_lsp (double epoch_ms, struct s_latency *lsp, int len_ld) {

    struct  s_latency  *left, *right, *current;    // current, left and right index of the search

    left = lsp; right = lsp + len_ld - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->bp_epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    // if there is sring of entries that match the epoch_ms, move to the right most edge as it is the most
    // recently arrived information
    while ((current < lsp + len_ld - 1) && ((current+1)->bp_epoch_ms == epoch_ms))
        current++; 

    return current; 
} // find_closest_lsp

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

// sorts latency data array by bp_epoch_ms
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

// reads and parses a latency log file. Returns 0 if end of file reached
// ch: 1, received a latency, numCHOut:1, packetNum: 48851, latency: 28, time: 1681255250906, sent from ch: 0
// ch: 1, received a latency, numCHOut:0, packetNum: 4294967295, latency: 14, time: 1681255230028, sent from ch: 1
int read_ld_line (FILE *fp, int ch, struct s_latency *ldp) {
    char ldline[MAX_LINE_SIZE], *ldlinep = ldline; 
    char dummy_str[100];
    int  dummy_int; 

    if (fgets (ldline, MAX_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    int n; 
    int rx_channel, tx_channel;

    while (
        ((n = sscanf (ldlinep, 
            "%[^:]:%d, %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%lf %[^:]:%d",
            dummy_str, &rx_channel,
            dummy_str, &dummy_int,
            dummy_str, &ldp->packet_num, 
            dummy_str, &ldp->t2r_ms, 
            dummy_str, &ldp->bp_epoch_ms,
            dummy_str, &tx_channel)) !=12)
        || (rx_channel !=ch) 
        || (tx_channel !=ch)) {

        if (n != 12) // did not successfully parse this line
            FWARN(warn_fp, "read_ld_line: Skipping line %s\n", ldlinep)

        // else did not match the channel
        if (fgets (ldline, MAX_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;
    } // while not successfully scanned a transmit log line

    return 1;
} // end of read_ld_line

void read_sd (FILE *fp) {

    // allocate storate for service data log
    sd0 = (struct s_service *) malloc (sizeof (struct s_latency) * SD_BUFFER_SIZE); 
    sd1 = (struct s_service *) malloc (sizeof (struct s_latency) * SD_BUFFER_SIZE); 
    sd2 = (struct s_service *) malloc (sizeof (struct s_latency) * SD_BUFFER_SIZE); 

    if (sd0==NULL || sd1==NULL || sd2==NULL)
        FATAL("Could not allocate storage to read the service log file in an array%s\n", "")

    int i; 
    struct s_service *sdp;
	int *len_sdp;
    
    for (i=0; i<3; i++) {

        printf ("Reading service log data file for channel %d\n", i); 

        sdp = i==0? sd0 : i==1? sd1 : sd2; 
        len_sdp = i==0? &len_sd0 : i==1? &len_sd1 : &len_sd2; 
        
    	// read latency log file into array and sort it
    	while (read_sd_line (fp, i, sdp)) {
    		(*len_sdp)++;
    		if (*len_sdp == SD_BUFFER_SIZE)
    		    FATAL ("Service data array is not large enough to read the log file. Increase SD_BUFFER_SIZE%S\n", "")
    		sdp++;
    	} // while there are more lines to be read
    
    	if (*len_sdp == 0)
    		FATAL("service log file is empty%s", "\n")
    
        fseek (fp, 0, SEEK_SET); 

    } // for each channel

    return; 
} // end of read_sd

// reads carrier metadata files in cd arrays
void read_ld (FILE *fp) {

    // allocate storage for latency log data sorted by packet number
    ld0 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    ld1 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    ld2 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    
    // allocate storage for latency log data sorted by bp_epoch_ms
    ls0 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    ls1 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    ls2 = (struct s_latency *) malloc (sizeof (struct s_latency) * LD_BUFFER_SIZE);
    
    if (ld0==NULL || ld1==NULL || ld2==NULL)
        FATAL("Could not allocate storage to read the latency log file in an array%s\n", "")

    int i; 
     struct s_latency *ldp, *lsp;
	int *len_ldp;
    for (i=0; i<3; i++) {

        printf ("Reading bp latency data file for channel %d\n", i); 

        ldp = i==0? ld0 : i==1? ld1 : ld2; 
        lsp = i==0? ls0 : i==1? ls1 : ls2; 
        len_ldp = i==0? &len_ld0 : i==1? &len_ld1 : &len_ld2; 
        
    	// read latency log file into array and sort it
    	while (read_ld_line (fp, i, ldp)) {
    		(*len_ldp)++;
    		if (*len_ldp == LD_BUFFER_SIZE)
    		    FATAL ("Latency data array is not large enough to read the log file. Increase LD_BUFFER_SIZE%S\n", "")
            *lsp = *ldp;
    		lsp++; ldp++;
    	} // while there are more lines to be read
    
    	if (*len_ldp == 0)
    		FATAL("latency data log file is empty%s", "\n")
    
        // sort 
        ldp = i==0? ld0 : i==1? ld1 : ld2; 
        sort_ld_by_packet_num (ldp, *len_ldp); 

        lsp = i==0? ls0 : i==1? ls1 : ls2; 
        sort_ld_by_bp_epoch_ms (lsp, *len_ldp);

        fseek (fp, 0, SEEK_SET); 

     } // for each channel

    return; 
} // read_ld

// reads and parses a service log file. Returns 0 if end of file reached
int read_sd_line (FILE *fp, int ch, struct s_service *sdp) {

    char sdline[MAX_LINE_SIZE], *sdlinep = sdline; 
    char dummy_str[100];
    char state_str[100];
    int  dummy_int; 
    double dummy_float;

    if (fgets (sdline, MAX_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    int n; 
    int channel;
    // CH: 1, change to out-of-service state, latency: 64, latencyTime: 1673815478924, estimated latency: 70, stop_sending flag: 1 , 
    // uplink queue size: 19, zeroUplinkQueue: 0, service flag: 0, numCHOut: 1, Time: 1673815478930, packetNum: 8

    while (
        ((n = sscanf (sdlinep, 
            "%[^:]:%d, %[^,], %[^:]:%d %[^:]:%lf %[^:]:%d %[^:]:%d \
            %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%lf %[^:]:%d" ,
            dummy_str, &channel,
            state_str,  
            dummy_str, &sdp->bp_t2r,
            dummy_str, &sdp->bp_t2r_epoch_ms, 
            dummy_str, &sdp->est_t2r, 
            dummy_str, &dummy_int,

            dummy_str, &sdp->socc, 
            dummy_str, &sdp->zeroUplinkQueue, 
            dummy_str, &dummy_int,
            dummy_str, &dummy_int,
            dummy_str, &sdp->state_transition_epoch_ms, 
            dummy_str, &sdp->bp_packet_num)) !=23)
        || channel !=ch // skip if not this channel
        /*
        // collect only going in service timestampls
        || (strcmp(state_str, "change to out-of-service state") == 0)*/) {

        if (n != 23) // did not successfully parse this line
            FWARN(warn_fp, "read_sd_line: Skipping line %s\n", sdlinep);

        // else did not match the channel
        if (fgets (sdline, MAX_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;
    } // while not successfully scanned a transmit log line

    if (strcmp(state_str, "change to out-of-service state") == 0)
        sdp->state = 0;  // should not happen
    else if (strcmp(state_str, "change to in-service state") == 0)
        sdp->state = 1; 
    else
        FATAL ("read_sd_line: could not understand state change string: %s\n", state_str) 

    return 1;
} // end of read_sd_line

// reads and parses a proble log line from the specified file. 
// returns 0 if end of file reached or // if the scan for all the fields fails due 
// to an incomplete line as in last line of the file.
int read_pr_line (
    FILE *fp, int len_tdfile, struct s_txlog *tdhead, int ch, 
    struct s_carrier *mdp) {

    char mdline[MAX_LINE_SIZE], *mdlinep = mdline; 
    char dummy_str[100]; 
    int dummy_int;

    if (fgets (mdline, MAX_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    // ch: 2, receive_a_probe_packet. sendTime: 1673295597582,  receivedTime: 1673295597601
    // ch: 1, receive_a_probe_packet. sendTime: 1673558308243, latency: 47, receivedTime: 16735583082900
    int n, pr_ch;     // channel number
    while (
        ((n = sscanf (mdlinep, "%s %d, %s %s %lf, %s %d, %s %lf", 
            dummy_str,
            &pr_ch, 
            dummy_str, 
            dummy_str, 
            &mdp->tx_epoch_ms,
            dummy_str, 
            &dummy_int,
            dummy_str, 
            &mdp->rx_epoch_ms)) != 9) 
        ||
        (pr_ch != ch)) {

        if (n != 9) // did not successfully parse this line
            FWARN(warn_fp, "read_pr_line: Skipping line %s\n", mdlinep)

        // else did not match the specified channel
        else if (fgets (mdlinep, MAX_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;

    } // while not successfully scanned a probe log line

    mdp->t2r_ms = mdp->rx_epoch_ms - mdp->tx_epoch_ms; 
    if (len_tdfile) { // log file exists
        mdp->tdp = find_occ_from_td (mdp->tx_epoch_ms, tdhead, len_tdfile, &(mdp->iocc), &(mdp->socc), &(mdp->socc_epoch_ms));
    }
    // make a copy of the socc for unmodified socc output
    mdp->usocc = mdp->socc; 
    mdp->probe = 1; 

    return 1;

} // end of read_pr_line

void my_free () {

    if (fd !=NULL) free (fd); 

    if (td0 != NULL) free (td0); 
    if (td1 != NULL) free (td1); 
    if (td2 != NULL) free (td2); 

    if (cd0 != NULL) free (cd0); 
    if (cd1 != NULL) free (cd1); 
    if (cd2 != NULL) free (cd2); 

    if (cs0 != NULL) free (cs0); 
    if (cs1 != NULL) free (cs1); 
    if (cs2 != NULL) free (cs2); 

    if (sd0 != NULL) free (sd0); 
    if (sd1 != NULL) free (sd1); 
    if (sd2 != NULL) free (sd2); 

    if (ld0 != NULL) free (ld0); 
    if (ld1 != NULL) free (ld1); 
    if (ld2 != NULL) free (ld2); 
    
    if (ls0 != NULL) free (ls0); 
    if (ls1 != NULL) free (ls1); 
    if (ls2 != NULL) free (ls2); 

    if (avgqd0 != NULL) free (avgqd0);
    if (avgqd1 != NULL) free (avgqd1);
    if (avgqd2 != NULL) free (avgqd2);

    if (brmdata != NULL) free (brmdata);

	if (md_fp) fclose (md_fp);
	if (an_fp) fclose (an_fp);
	if (fs_fp) fclose (fs_fp); 
	if (ss_fp) fclose (ss_fp); 
	if (ps_fp) fclose (ps_fp); 
	if (c0_fp) fclose (c0_fp); 
	if (c1_fp) fclose (c1_fp); 
	if (c2_fp) fclose (c2_fp); 
	if (gps_fp) fclose (gps_fp); 
	if (td_fp) fclose (td_fp); 
	if (ld_fp) fclose (ld_fp); 
	if (sd_fp) fclose (sd_fp); 
	if (brmd_fp) fclose (brmd_fp); 
	if (avgqd_fp) fclose (avgqd_fp); 
	if (warn_fp) fclose (warn_fp); 
    if (dbg_fp) fclose (dbg_fp); 
} // end of my_free

// free up storage before exiting
int my_exit (int n) {

    my_free (); 
    exit (n);

} // my_exit

FILE *open_file (char *filep, char *modep) {
    FILE *fp;
    if ((fp = fopen (filep, modep)) == NULL)
        FATAL ("Could not open file %s\n", filep)
    
    return fp; 
} // end of open_file

// returns number of enteries found in the gps file
// modifies global gps. if verbose then writes into outfp also
int read_gps (FILE *fp) {
	char	line[MAX_LINE_SIZE], *lp = line; 
	char	time[100];						// time field of gps file
	int		index = -1; 					// gps array index
	
	// skip header
	if (fgets (lp, MAX_LINE_SIZE, fp) == NULL)
		FATAL ("Empty or incomplete gps file%s\n", "")

	// while there are more lines to read from the gps file
	while (fgets (lp, MAX_LINE_SIZE, fp) != NULL) {
		struct s_gps *gpsp = gps + ++index;

		if (index >= MAX_GPS)
			FATAL ("gps file has more lines that gps[MAX_GPS]. Increase MAX_GPS%d\n", index)

		if (sscanf (lp, "%[^,],%lf,%d,%lf,%lf,%f", 
			time,
			&gpsp->epoch_ms,
			&gpsp->mode,
			&gpsp->coord.lat,
			&gpsp->coord.lon,
			&gpsp->speed) != 6)
			FATAL ("Invalid gps line - missing fields: %s\n", lp)
		
		gpsp->count = 0; // initialization
		
		// error cheecking
		if (index == 0) continue; // on the first line; nothing to error check
		if (gpsp->epoch_ms < (gpsp-1)->epoch_ms) 
			FATAL ("gps file: time is not in increasing order: %s\n", lp)
	} // while there are more lines to be read from the gps file
	
	if (index < 0) 
		FATAL ("empty gps file %s\n", "")
	
	return index+1;
} // end of read_gps

// reads and parses a bit-rate modulation log file line. Returns 0 if end of file reached
int read_brmd_line (FILE *fp, struct s_brmdata *brmdp) {

    char brmdline[MAX_LINE_SIZE], *brmdlinep = brmdline; 
    char dummy_str[100], *dummy_strp = dummy_str;
    int n; 

    // parse the line
    while (fgets (brmdline, MAX_LINE_SIZE, fp) != NULL) {
        int n = sscanf (brmdlinep, 
            // send_bitrate: 1000000, encoder state: 0, ch0 quality state: 0, ch1 quality state: 0, ch2 quality state: 0, time: 1675115599835
            "%[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%d %[^:]:%lf",
            dummy_strp, &brmdp->bit_rate, 
            dummy_strp, &brmdp->encoder_state,
            dummy_strp, &brmdp->channel_quality_state[0], 
            dummy_strp, &brmdp->channel_quality_state[1], 
            dummy_strp, &brmdp->channel_quality_state[2], 
            dummy_strp, &brmdp->epoch_ms);

        if (n != 12) // did not successfully parse this line
            FWARN(warn_fp, "read_brmdp_line: Skipping line %s\n", brmdlinep)
        else
            return 1;
    } // while not successfully scanned a transmit log line and not reached end of the file

    return 0;
} // end of read_brmd_line

// sorts brmdp array by time stamp
void sort_brmd (struct s_brmdata *brmdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((brmdp+j)->epoch_ms < (brmdp+j-1)->epoch_ms) {
                // slide jth element up by 1
                struct s_brmdata temp = *(brmdp+j-1); 
                *(brmdp+j-1) = *(brmdp+j);
                *(brmdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it
    } // for elements in the log data array

    return;
} // end of sort_brmd

// reads bit-rate modulation file into brmd array sorted by timestamp
void read_brmd (FILE *fp) {

    // allocate storage for tx log
    brmdata = (struct s_brmdata *) malloc (sizeof (struct s_brmdata) * BRM_BUFFER_SIZE);
    if (brmdata==NULL) FATAL("Could not allocate storage to read the bit rate log file in an array%s\n", "")

	// read brm log file into array and sort it
    printf ("Reading bit-rate modulation data file\n"); 
    struct s_brmdata *brmdp = brmdata; 
	while (read_brmd_line (fp, brmdp)) {
		(len_brmd)++;
		if (len_brmd == BRM_BUFFER_SIZE)
		    FATAL ("BRM data array is not large enough to read the log file. Increase BRM_BUFFER_SIZE%S\n", "")
		brmdp++;
	} // while there are more lines to be read

	if (len_brmd == 0)
		FATAL("Bit rate modulation log file is empty%s", "\n")

    sort_brmd (brmdata, len_brmd); // sort by epoch_ms

} // read_brmdp

// reads and parses a bit-rate modulation log file line. Returns 0 if end of file reached
int read_avgqd_line (FILE *fp, int channel, struct s_avgqdata *avgqdp) {

    char avgqdline[MAX_LINE_SIZE], *avgqdlinep = avgqdline; 
    char dummy_str[100], *dummy_strp = dummy_str;
    int ch;

    // parse the line
    while (fgets (avgqdline, MAX_LINE_SIZE, fp) != NULL) {

        avgqdp->packet_num = -1; // not initialized yet
        int n1 = sscanf (avgqdlinep,   
        // RollingAvg75. Probe. CH: 0, RollingAvg75: 0.000000, qualityState: 0, queue size: 0, time: 1675115599812, packetNum: 36
            "%[^:]:%d %[^:]:%f %[^:]:%d %[^:]:%d %[^:]:%lf %[^:]:%d",
            dummy_strp, &ch, 
            dummy_strp, &avgqdp->rolling_average,
            dummy_strp, &avgqdp->quality_state, 
            dummy_strp, &avgqdp->queue_size,
            dummy_strp, &avgqdp->tx_epoch_ms,
            dummy_strp, &avgqdp->packet_num
            );

        int n2 = sscanf (avgqdlinep,   
        // RollingAvg75. Probe. CH: 1, RollingAvg75: 0.240000, qualityState: 0, queue size: 18, time: 1675115599897
            "%[^:]:%d %[^:]:%f %[^:]:%d %[^:]:%d %[^:]:%lf", 
            dummy_strp, &ch, 
            dummy_strp, &avgqdp->rolling_average,
            dummy_strp, &avgqdp->quality_state, 
            dummy_strp, &avgqdp->queue_size,
            dummy_strp, &avgqdp->tx_epoch_ms
            );

        if ((ch == channel) && (n1==12 || n2==10)) // successfully parsed this line
            return 1;

    } // while not successfully scanned a transmit log line and not reached end of the file

    return 0;
} // end of read_avgqd_line

// sorts avgqdp array by packet number
void sort_avgqd (struct s_avgqdata *avgqdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((avgqdp+j)->packet_num < (avgqdp+j-1)->packet_num) {
                // slide jth element up by 1
                struct s_avgqdata temp = *(avgqdp+j-1); 
                *(avgqdp+j-1) = *(avgqdp+j);
                *(avgqdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it
    } // for elements in the log data array

    return;
} // end of sort_avgqd

// reads bit-rate modulation file into avgqd array sorted by timestamp
void read_avgqd (FILE *fp) {

    // allocate storage for avgq og
    avgqd0 = (struct s_avgqdata *) malloc (sizeof (struct s_avgqdata) * AVGQ_BUFFER_SIZE);
    avgqd1 = (struct s_avgqdata *) malloc (sizeof (struct s_avgqdata) * AVGQ_BUFFER_SIZE);
    avgqd2 = (struct s_avgqdata *) malloc (sizeof (struct s_avgqdata) * AVGQ_BUFFER_SIZE);

    if (avgqd0==NULL || avgqd1==NULL || avgqd2==NULL) 
        FATAL("Could not allocate storage to read the avgq log file in an array%s\n", "")

     int i;
     struct s_avgqdata *avgqdp;
     int *len_avgqdp;
     for (i=0; i<3; i++) {

        printf ("Reading rolling average data file for channel %d\n", i); 

        avgqdp = i==0? avgqd0 : i==1? avgqd1 : avgqd2; 
        len_avgqdp = i==0? &len_avgqd0 : i==1? &len_avgqd1 : &len_avgqd2; 
        
    	// read avgq log file into array and sort it
    	while (read_avgqd_line (fp, i, avgqdp)) {
    		(*len_avgqdp)++;
    		if (*len_avgqdp == AVGQ_BUFFER_SIZE)
    		    FATAL ("AVGQ data array is not large enough to read the log file. Increase AVGQ_BUFFER_SIZE%S\n", "")
    		avgqdp++;
    	} // while there are more lines to be read
    
    	if (*len_avgqdp == 0)
    		FATAL("AVGQ log file is empty%s", "\n")
    
        avgqdp = i==0? avgqd0 : i==1? avgqd1 : avgqd2; 
        printf ("Sorting rolling average data\n");
        sort_avgqd (avgqdp, *len_avgqdp); // sort by packet num

        fseek (fp, 0, SEEK_SET); 

     } // for each channel

     return; 
} // read_avgqdp

// reads transmit log file into 3 td arrays Returns 0 if end of file reached
int read_td_line (FILE *fp, int ch, struct s_txlog *tdp) {

    char tdline[MAX_LINE_SIZE], *tdlinep = tdline; 
    char dummy_string[100];

    if (fgets (tdline, MAX_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    // parse the line
    // uplink_queue. ch: 2, timestamp: 1672344732193, queue_size: 23, elapsed_time_since_last_queue_update: 29, actual_rate: 1545, stop_sending_flag: 0, zeroUplinkQueue_flag: 0, lateFlag: 1
    int n; 
    while (
        ((n = sscanf (tdlinep, "%s %s %d, %s %lf, %s %d, %s %d, %s %d",
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
        &tdp->actual_rate)) !=11) || (tdp->channel !=ch)) {

        if (n != 11) // did not successfully parse this line
            FWARN(warn_fp, "read_td_line: Skipping line %s\n", tdlinep)

        if (fgets (tdline, MAX_LINE_SIZE, fp) == NULL)
            // reached end of the file
            return 0;

    } // while not successfully scanned a transmit log line

    return 1;
} // end of read_td_line

// sorts td array by time stamp
void sort_td (struct s_txlog *tdp, int len) {

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
} // end of sort_td

// reads transmit log file into 3 td arrays 
void read_td (FILE *fp) {

    // allocate storage for tx log
    td0 = (struct s_txlog *) malloc (sizeof (struct s_txlog) * TX_BUFFER_SIZE);
    td1 = (struct s_txlog *) malloc (sizeof (struct s_txlog) * TX_BUFFER_SIZE);
    td2 = (struct s_txlog *) malloc (sizeof (struct s_txlog) * TX_BUFFER_SIZE);

    if (td0==NULL || td1==NULL || td2==NULL)
        FATAL("Could not allocate storage to read the tx log file in an array%s\n", "")

    int i; 
    for (i=0; i<3; i++) {
        struct s_txlog *tdp; 
	    int *len_td;

        tdp = i==0? td0 : i==1? td1 : td2; 
        len_td = i==0? &len_td0 : i==1? &len_td1 : &len_td2; 

		// read tx log file into array and sort it
        printf ("Reading uplink queue size file for channel %d\n", i);
		while (read_td_line (fp, i, tdp)) {
		    (*len_td)++;
		    if (*len_td == TX_BUFFER_SIZE)
		        FATAL ("TX data array is not large enough to read the tx log file. Increase TX_BUFFER_SIZE%S\n", "")
		    tdp++;
		} // while there are more lines to be read

		// if (*len_td == 0)
		   // FATAL("Meta data file is empty%s", "\n")

		// sort by timestamp
        tdp = i==0? td0 : i==1? td1 : td2; 
        sort_td (tdp, *len_td); 

        // reset the file pointer to the start of the file for the next channel
        fseek (fp, 0, SEEK_SET);
    } // for each carrier

    return;
} // read_td

// reads a line from the carrier metadata file. returns 0 at the end of file.
int read_cd_line (int skip_header, FILE *fp, struct s_cmetadata *cdp) {

    char line[MAX_LINE_SIZE];

    if (skip_header) {
	    fgets(line, MAX_LINE_SIZE, fp);
        return 1; 
    }
    // packe_number	 sender_timestamp	 receiver_timestamp	 
    // video_packet_len	 frame_start frame_number frame_rate frame_resolution frame_end	 
    // camera_timestamp	 retx	 chPacketNum
    int dummy_int; 
    double dummy_double; 
	if (fgets(line, MAX_LINE_SIZE, fp) != NULL) {
	    if (sscanf (line, 
            "%u, %lf, %lf, %d,  %d,  %d,  %d,  %d,  %d, %lf, %d, %d,", 
            &cdp->packet_num,   // packet_number
            &cdp->vx_epoch_ms,  // sender_timestamp
            &cdp->rx_epoch_ms,  // receiver_timestamp
            &dummy_int,         // video_packet_len
            &dummy_int,         // frame_start
            &dummy_int,         // frame_number
            &dummy_int,         // frame_rate
            &dummy_int,         // frame_resolution
            &dummy_int,         // frme_end
            &dummy_double,      // camera_timestamp
            &cdp->retx,         // retx
            &dummy_int          // chPacketNum
            ) != 12) {
	        printf ("read_cd_line: could not find all the fields in the meta data file line: %s\n", line); 
            return 0; // consider it end of file
	    } // if scan failed
        else // scan passed
            return 1;
    } 
    else // reached end of file
        return 0;
} // read_cd_line

// sorts cd array by packet number
void sort_cd (struct s_cmetadata *cdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((cdp+j)->packet_num < (cdp+j-1)->packet_num) {
                // slide jth element up by 1
                struct s_cmetadata temp = *(cdp+j-1); 
                *(cdp+j-1) = *(cdp+j);
                *(cdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it
    } // for elements in the log data array

    return;
} // end of sort_cd

// sorts cs array by rx TS number
void sort_cs (struct s_cmetadata *cdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((cdp+j)->rx_epoch_ms < (cdp+j-1)->rx_epoch_ms) {
                // slide jth element up by 1
                struct s_cmetadata temp = *(cdp+j-1); 
                *(cdp+j-1) = *(cdp+j);
                *(cdp+j) = temp; 
            } // if slide up
            else 
                // done sliding up
                break;
            j--;
        } // end of while jth elment is smaller than one above it
    } // for elements in the log data array

    return;
} // end of sort_cs

// reads carrier metadata files in cd arrays
void read_cd (char *fnamep) {

    // allocate storage for carrier metadata sorted by packet number
    cd0 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);
    cd1 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);
    cd2 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);
    
    // carrier metadata sorted by receive timestamp
    cs0 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);
    cs1 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);
    cs2 = (struct s_cmetadata *) malloc (sizeof (struct s_cmetadata) * CD_BUFFER_SIZE);

    if (cd0==NULL || cd1==NULL || cd2==NULL)
        FATAL("Could not allocate storage to read the carrier metadata file in an array%s\n", "")

    int i; 
    for (i=0; i<3; i++) {
        struct s_cmetadata *cdp, *csp; 
        struct s_avgqdata *avgqdp; 
        struct s_latency *ldp, *lsp;
	    int *len_cdp, len_avgqd, len_ld;
        char bp[1000];
        FILE *fp; 

        sprintf (bp, "%s_ch%d.csv", fnamep, i);
        if ((fp = open_file (bp, "r")) == NULL)
            FATAL("read_cd: Could not open carrier metadata file %s", bp)
        printf ("Reading carrier meta data file %s\n", bp); 

        switch (i) {
            case 0: cdp = cd0; len_cdp = &len_cd0; csp = cs0; 
                avgqdp = avgqd0; len_avgqd = len_avgqd0; 
                ldp = ld0; lsp = ls0; len_ld = len_ld0; 
                break;
            case 1: cdp = cd1; len_cdp = &len_cd1; csp = cs1;
                avgqdp = avgqd1; len_avgqd = len_avgqd1; 
                ldp = ld1; lsp = ls1; len_ld = len_ld1; 
                break;
            case 2: cdp = cd2; len_cdp = &len_cd2; csp = cs2;
                avgqdp = avgqd2; len_avgqd = len_avgqd2; 
                ldp = ld2; lsp = ls2; len_ld = len_ld2; 
                break;
        } // of switch

		// read carrier metadata into array and sort it
        read_cd_line (1, fp, cdp); // skip header
		while (read_cd_line (0, fp, cdp)) {

            decode_sendtime (&cdp->vx_epoch_ms, &cdp->tx_epoch_ms, &cdp->socc);

            if (cdp->len_avgqd = len_avgqd) { // average queue size log file exists 
                // read the average queue size for this packet
                if ((cdp->avgqdp = find_avgqdp (cdp->packet_num, cdp->tx_epoch_ms, avgqdp, len_avgqd))
                    == NULL)
                    FATAL("read_cd: could not find packet %d in the avgq array\n", cdp->packet_num)
            } // have queue size log file

            if (cdp->len_ld = len_ld) { // latency log file exists
                // read the latency data for thi spacket
                cdp->ldp = find_ldp_by_packet_num (cdp->packet_num, cdp->rx_epoch_ms, ldp, len_ld);
                cdp->lsp = find_closest_lsp (cdp->tx_epoch_ms, lsp, len_ld);
            } // have latency log file

            if (len_brmd) // bit-rate modulation data file exists
                cdp->brmdp = find_closest_brmdp (cdp->tx_epoch_ms, brmdata, len_brmd);

		    (*len_cdp)++;
		    if (*len_cdp == CD_BUFFER_SIZE)
		        FATAL ("read_cd: CD data array is not large enough to read the data file. Increase CD_BUFFER_SIZE%s\n", "")

            *csp = *cdp; 
		    cdp++; csp++; 
		} // while there are more lines to be read

		if (*len_cdp == 0)
		    FATAL("read_cd: Meta data file %s is empty\n", bp);

        // sort the arrays
        switch (i) {
            case 0: cdp = cd0; csp = cs0; break;
            case 1: cdp = cd1; csp = cs1; break; 
            case 2: cdp = cd2; csp = cs2; break;
        } // of switch
        sort_cd (cdp, *len_cdp); // sort by packet number
        sort_cs (csp, *len_cdp); // sort by rx TS
    } // for each carrier

    return; 
} // read_cd

// find_packet_in_cd returns pointer to the packet in the carrier metadata array 
// with the earliest rx_time matching the specified packet. 
// returns NULL if no match was found, else the specified packet num
struct s_cmetadata* find_packet_in_cd (int packet_num, struct s_cmetadata *cdhead, int len_cd) {

    struct  s_cmetadata *left, *right, *current;    // current, left and right index of the search

    left = cdhead; right = cdhead + len_cd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (packet_num > current->packet_num) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than packet_num
    if ((left->packet_num != packet_num) && (right->packet_num != packet_num))
        // no match
        return NULL; 

    // now search to the right and see if there is smaller rx_epoch_ms
    if (right->packet_num == packet_num)
        left = right; 
    struct s_cmetadata *smallest = left; 
    while ((left != (cdhead+len_cd-1)) && ((++left)->packet_num == packet_num)) {
        if (smallest->rx_epoch_ms > left->rx_epoch_ms) 
            smallest = left; 
    }

    return smallest;
} // find_packet_in_cd

// returns 1 if able to find coordinates for the frame; else 0. Assumes gps file is in increasing time order
// uses globals gps and len_gps
int get_gps_coord (struct frame *framep) {
	int i;
	struct s_gps	*prev;
	struct s_gps	*next;

	// check if the frame time is in the range
	if ((framep->camera_epoch_ms < gps->epoch_ms) || (framep->camera_epoch_ms > (gps+len_gps-1)->epoch_ms)) {
		return 0;
	} // frame timestamp deos not map into the gps file range

	// find the closest before timestamp in gps array (linear search now should be replaced with binary)
	for (i=0; i<len_gps; i++) {
		if ((gps+i)->epoch_ms > framep->camera_epoch_ms)
			break; 
	}

	// i could be larger than the array if the last element was equal to the frame time
	if (i==len_gps) { // len_gps is 1 more than the last index
		prev = gps+len_gps-1;
		next = gps+len_gps-1;
		framep->coord.lat = prev->coord.lat;
		framep->coord.lon = prev->coord.lon;
		framep->speed = prev->speed;
	}  else { // interpolate
		prev = gps+i-1;
		next = gps+i;
		double dt = (framep->camera_epoch_ms - prev->epoch_ms)/(next->epoch_ms - prev->epoch_ms);
		framep->coord.lat = (1-dt)*prev->coord.lat + (dt * next->coord.lat);
		framep->coord.lon = (1-dt)*prev->coord.lon + (dt * next->coord.lon); 
		framep->speed = (1-dt)*prev->speed + (dt * next->speed); 
	} // interpolate

		return 1;
} // end of get_gps_coord

// returns 1 if successfully read the annotation file
int read_annotation_file (void) {
    char    line[100]; 

    // skip header
    if (fgets (line, 100, an_fp) == NULL) {
        printf ("Empty annotation file\n");
        my_exit (-1); 
    }

    // read annotation line
    while (fgets (line, 100, an_fp) != NULL) {
        if (sscanf (line, "%u, %u", &anlist[len_anlist][0], &anlist[len_anlist][1]) == 2) {
            if (anlist[len_anlist][1] < anlist[len_anlist][0]) {
                printf ("Invalid annotation. Start frame is bigger than then End\n");
                my_exit (-1);
            }
        } else if (sscanf (line, "%u", &anlist[len_anlist][0]) == 1)
            anlist[len_anlist][1] = anlist[len_anlist][0];
        else {
            printf ("Invalid annnotation format %s\n", line); 
            my_exit(-1); 
        }
        len_anlist++;
    } // while there are more lines to be read
    
    // checks
    if (len_anlist == 0) {
        printf ("WARNING: the annotation file had no annotations\n"); 
    }
    return 1; 
}  // end of read_annotation_file

// returns 1 if the current frame count is marked in the annotation file
int check_annotation (unsigned frame_num) {
    int i;
    for (i=0; i < len_anlist; i++)
        if ((frame_num >= anlist[i][0]) && (frame_num <= anlist[i][1]))
            return 1;
    return 0;
} // end of check_annotation_file

// prints program usage
void print_usage (void) {
    char    *usage1 = "Usage: vqfilter [-help] [-v] [-l <ddd>] [-b <dd.d>] [-fc <ddd>] [-ff <ddd>] [-j <dd>] [-ns1|ns2]  [-sa] [-ml <d>] [-mt <dddd>] ";
    char    *usage2 = "[-a <file name>] [-gps <prefex>] [-tx_pre <prefix>] [-ld_pre <prefix>] [-brm_pre <prefix>] [-avgq_pre <prefix>]i [-mdc] -ipath -opath -rx_pre <prefix>";
    printf ("%s%s\n", usage1, usage2);
    printf ("\t-v enables full output - all 3, session, frame and late_frame files. Default 1 (enabled)\n");
    printf ("\t-l <ddd> is the maximumum acceptable camera to rx latency in ms. Default 110ms. Frames with longer lantency marked late.\n");
    printf ("\t-b <dd.d> is the minimum acceptable bit rate in Mbps. Default 0.5Mbps. Only used for session stats generation.\n");
    printf ("\t-fc <ddd> tx to rx latency lower than this for a packet means fast channel. Default 40ms. Used to find if have 0, 1, 2 or 3 fast channels.\n");
    printf ("\t-ff <ddd> tx to rx latency lower than this implies fast frame delivery. Default 60ms. Used to check frame size modulation. \n");
    printf ("\t-j <dd> jitter buffer to reduce skip/repeat. this latency gets added to the Camera to display path. Default 10ms\n");
    printf ("\t-ns1/2 sender time format specification. ns1 = no occ, ns2 = with occ. Default 2\n");
    printf ("\t-sa enables self annotation to augment file based annotation. Currently 0 fast channel only\n"); 
    printf ("\t-ml <d> frame size modulation latency. Default 3 frames. Frame size expected to be modulated within this latecy.\n");
    printf ("\t-mt <dddd> frame size modulation threshold. Default 6000B. Frame size expected to be modulated below or above this size\n");
    printf ("\t-a <file name>. manual annotation file name. See POR for syntax\n");
    printf ("\t-gps <prefix> gps file prefix if available\n"); 
    printf ("\t-tx_pre prefix (without .log) of the transmit side log file\n");
    printf ("\t-brm_pre prefix (without .log) of the bit-rate modulation log file\n");
    printf ("\t-avgq prefix (without .log) rolling average queue size log file\n");
    printf ("\t-mdc shouild be used if per channel meta data is also avalible. Name of the per channel file should be <rx_prefix>_ch0/1/2\n");
    printf ("\t-ipath input directory\n");
    printf ("\t-opath output directory\n");
    printf ("\t-rx_pre prefix (without .csv) of the receive side dedup meta data file without the .csv. Output files take the same prefix.");
    return; 
} // print_usage

// updates session per packet stats - called after every meta data line read
void update_session_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct s_metadata *mdp,                  // current dedup meta data line
    struct s_session *ssp) {                // current packet's meta data; current may be NULL at EOF

    update_metric_stats (ssp->c2vp, 0, mdp->vx_epoch_ms - fp->camera_epoch_ms, MAX_C2V_LATENCY, MIN_C2V_LATENCY);
    update_metric_stats (ssp->v2tp, 0, mdp->tx_epoch_ms - mdp->vx_epoch_ms, 50, 0);
    update_metric_stats (ssp->c2rp, 0, mdp->rx_epoch_ms - fp->camera_epoch_ms, MAX_C2R_LATENCY, MIN_C2R_LATENCY);

} // end of update_session_packet_stats

// updates per packet stats of the frame
void update_frame_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct s_metadata *lmdp,                 // last packet's meta data
    struct s_metadata *cmdp) {               // current packet's meta data; current may be NULL at EOF

    fp->packet_count++; 

    fp->size += cmdp->video_packet_len;
    fp->late += cmdp->rx_epoch_ms > (fp->camera_epoch_ms + maximum_acceptable_c2rx_latency); 
    fp->missing += cmdp->packet_num - (lmdp->packet_num+1);  
    fp->out_of_order += cmdp->rx_epoch_ms < lmdp->rx_epoch_ms;

    // check if this packet is earliest or latest arriving packet so far
    if (cmdp->rx_epoch_ms > fp->rx_epoch_ms_latest_packet) { // this is the latest packet so far
        fp->rx_epoch_ms_latest_packet = cmdp->rx_epoch_ms;
        fp->latest_packet_count = fp->packet_count; 
        fp->latest_packet_num = cmdp->packet_num; 
        fp->latest_retx = cmdp->retx; 
    }
    fp->rx_epoch_ms_earliest_packet = MIN(fp->rx_epoch_ms_earliest_packet, cmdp->rx_epoch_ms);

    // assume that this could be the last packet of the frame
    fp->last_packet_num = cmdp->packet_num;
    fp->tx_epoch_ms_last_packet = cmdp->tx_epoch_ms;
    fp->rx_epoch_ms_last_packet = cmdp->rx_epoch_ms;

} // end of update_frame_packet_stats

// initializes the frame structure for a new frame
void init_frame (
    struct frame *fp,                       // current frame pointer
    struct s_metadata *lmdp,                 // last packet's meta data
    struct s_metadata *cmdp,                 // current packet's meta data; current may be NULL at EOF
    int	   first_frame) {                   // 1 if it is the first frame. NOT USED

    fp->frame_count = first_frame? 1 : fp->frame_count + 1; 

    // update n-1 frame stats before we overwrite. Will have undefined value for first_frame.
    fp->nm1_camera_epoch_ms = fp->camera_epoch_ms;
    fp->nm1_rx_epoch_ms_earliest_packet = fp->rx_epoch_ms_earliest_packet; 
    fp->nm1_size = fp->size; 

    fp->size = cmdp->video_packet_len;
    if (cmdp->frame_start) {
        // clean start of the frame
        fp->missing = 0; 
        fp->camera_epoch_ms = cmdp->camera_epoch_ms;
    } else if (lmdp->frame_end) {
        // abrupt start of this frame but clean end of previous frame.
        // missing the camera_epoch_ms. So will add one frame worth of delay to previous camera time stamp
        fp->missing = cmdp->packet_num - (lmdp->packet_num + 1); 
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? FRAME_PERIOD_MS : fp->frame_rate==HZ_15? 66.66 : fp->frame_rate==HZ_10? 100 : 200;
    } else {
        // abrupt start of this frame AND abrupt end of the previous frame. Now we can't tell how many packets each frame lost
        // have assigned one missing packet to previous frame. So will assign the remaining packets to this frame. 
        fp->missing = cmdp->packet_num - (lmdp->packet_num +1) -1; 
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? FRAME_PERIOD_MS : fp->frame_rate==HZ_15? FRAME_PERIOD_MS*2: fp->frame_rate==HZ_10? 100 : 200;
    }
    fp->out_of_order = /* first_frame? 0: */ cmdp->rx_epoch_ms < lmdp->rx_epoch_ms; 
    fp->first_packet_num = fp->last_packet_num = cmdp->packet_num;
    fp->tx_epoch_ms_1st_packet = fp->tx_epoch_ms_last_packet = cmdp->tx_epoch_ms;
    fp->rx_epoch_ms_last_packet = fp->rx_epoch_ms_earliest_packet = fp->rx_epoch_ms_latest_packet = cmdp->rx_epoch_ms;
    fp->latest_retx = cmdp->retx; 
    fp->frame_rate = cmdp->frame_rate; 
	fp->display_period_ms = fp->frame_rate==HZ_30? FRAME_PERIOD_MS : fp->frame_rate==HZ_15 ? FRAME_PERIOD_MS*2 : fp->frame_rate==HZ_10? 100 : 200;
    fp->frame_resolution = cmdp->frame_resolution;
    fp->late = cmdp->rx_epoch_ms > (fp->camera_epoch_ms + maximum_acceptable_c2rx_latency);
    fp->packet_count = fp->latest_packet_count = 1; 
    fp->latest_packet_num = cmdp->packet_num; 
    fp->fast_channel_count = 0;
    if (first_frame) 
        fp->brm_changed = 0;
    if (fp->brm_changed)
        fp->brm_run_length = 1; 
} // end of init_frame

void print_command_line (FILE *fp) {
    
    // command line arguments
    fprintf (fp, "Command line arguments; ");
    fprintf (fp, "-l %d ", maximum_acceptable_c2rx_latency);
    fprintf (fp, "-ns%d ", new_sendertime_format);
    fprintf (fp, "-b %0.1f ", minimum_acceptable_bitrate);
    fprintf (fp, "-j %d ", rx_jitter_buffer_ms);
    fprintf (fp, "-f %d ", fast_channel_t2r);
    fprintf (fp, "-ff %d ", fast_frame_t2r);
    fprintf (fp, "-ml %d ", frame_size_modulation_latency);
    fprintf (fp, "-mt %d ", frame_size_modulation_threshold);
    fprintf (fp, "-a %s ", annotation_file_name);

    fprintf (fp, "\n"); 

    return;
} // end of print_command_line

void emit_frame_header (FILE *fp) {

    // command line
    // print_command_line (fp); 

    // frame stat header
    fprintf (fp, "F#, ");
    fprintf (fp, "CTS, ");
    fprintf (fp, "Late, ");
    fprintf (fp, "Miss, ");
    fprintf (fp, "CMbps, ");
    fprintf (fp, "anno, ");
    fprintf (fp, "0fst, ");
    fprintf (fp, "SzB, ");
    fprintf (fp, "EMbps,");
    fprintf (fp, "Lat, ");
    fprintf (fp, "Lon, ");
    fprintf (fp, "Spd, ");
    fprintf (fp, "Rpt, ");
    fprintf (fp, "skp, ");
    fprintf (fp, "c2d, ");
    fprintf (fp, "dlv, ulv, ");     // modulation down / up latency violation

    fprintf (fp, "SzP, ");
    fprintf (fp, "1st Pkt, ");
    fprintf (fp, "LPC, ");
    fprintf (fp, "LPN, ");
    fprintf (fp, "retx, ");
    fprintf (fp, "c2t, ");
    fprintf (fp, "t2r, ");
    fprintf (fp, "c2r, ");
    fprintf (fp, "ooo, ");
    fprintf (fp, "res, ");
    
    // time stamps at the end 
    fprintf (fp, "1st TX_TS, ");
    fprintf (fp, "Last TX_TS, ");
    fprintf (fp, "last Rx_TS, ");
    fprintf (fp, "earliest Rx_TS, ");
    fprintf (fp, "latest Rx_TS, ");
    fprintf (fp, "cdsp_TS, ");
    fprintf (fp, "ndsp_TS, ");
    fprintf (fp, "early, ");
    fprintf (fp, "Latest-earliest Rx, ");
    fprintf (fp, "Last-1st Tx, ");

    // bit-rate modulation
    fprintf (fp, "ebr, ");
    fprintf (fp, "estate, ");
    fprintf (fp, "esrl, ");
    fprintf (fp, "lqsrl,");
    fprintf (fp, "c0q, ");
    fprintf (fp, "c1q, ");
    fprintf (fp, "c2q, ");
    fprintf (fp, "br_epoch_ms, ");
    
    fprintf (fp, "\n"); 

} // emit_frame_header

void emit_packet_header (FILE *ps_fp) {

    // command line
    // print_command_line (ps_fp); 

    // frame meta data
    fprintf (ps_fp, "F#, ");
    fprintf (ps_fp, "P#, ");
	fprintf (ps_fp, "LPN, ");
    fprintf (ps_fp, "Late, ");
	fprintf (ps_fp, "Miss, ");
	fprintf (ps_fp, "CMbps, ");
	fprintf (ps_fp, "SzP, ");
	fprintf (ps_fp, "SzB, ");
    fprintf (ps_fp, "EMbps,");
	fprintf (ps_fp, "Ft2r, ");
	fprintf (ps_fp, "Fc2r, ");
	fprintf (ps_fp, "Rpt, ");
	fprintf (ps_fp, "skp, ");
	fprintf (ps_fp, "c2d, ");
    fprintf (ps_fp, "CTS, ");

    // Delivered packet meta data
    fprintf (ps_fp, "retx, ");
    fprintf (ps_fp, "ch, ");
    fprintf (ps_fp, "est,");
    fprintf (ps_fp, "MMbps, ");
    fprintf (ps_fp, "tx_TS, ");
    fprintf (ps_fp, "rx_TS, ");
    fprintf (ps_fp, "Pc2r, ");
    fprintf (ps_fp, "Pt2r, ");
    
    // service resumption indicator
	fprintf (ps_fp, "c2t, ");
    fprintf (ps_fp, "Res,");
    fprintf (ps_fp, "Res_ch,");

    // per carrier meta data
    int i; 
    for (i=0; i<3; i++) {
        fprintf (ps_fp, "C%d: c2r, c2v, t2r, r2t, est_t2r, ert, socc, MMbps, retx,", i); 
        fprintf (ps_fp, "tgap, ppkts, upkts, spkts, cont, Inc, rdbg, urpkt, oos_d, oos_o, cr, I, rb, bp_gap, x, y, rlen, flen, I,");
        fprintf (ps_fp, "tx_TS, rx_TS, r2t_TS, ert_TS, socc_TS, bp_pkt_TS, bp_pkt, bp_t2r,"); 
        fprintf (ps_fp, "Ravg, IS, qst, qsz, avg_ms, avg_pkt,");
    }

    // packet analytics 
    fprintf (ps_fp, "dtx, fch, eff, opt, "); 
    fprintf (ps_fp, "3fch, 2fch, 1fch, 0fch, cUse, ");

    fprintf (ps_fp, "\n");
    return;
} // emit_packet_header

void init_packet_stats (
    struct s_txlog *tdhead, int len_td, 
    struct s_cmetadata *cdhead, int len_cd, 
    struct s_cmetadata *csp, 
    struct s_carrier *cp) {

    cp->packet_num = -1;              // have not read any lines yet
    cp->socc = 0;                     // assume nothing in the buffer
    cp->t2r_ms = 30;                  // default value before the first packet is transmiited by this carrier
    cp->est_t2r_ms = 30;              // same as t2r default
    cp->tdhead = tdhead;              // tx data array
    cp->len_td = len_td;              // lenght of the tx data array
    cp->cdhead = cdhead; 
    cp->len_cd = len_cd;
    cp->csp = csp; 
    cp->last_run_length = 0;        // not started any transmission yet
    cp->run_length = 0;             // not started any transmission yet
    cp->start_of_run_flag = 0;        // no run finished yet
    cp->fast_run_length = 0;             // not started any transmission yet
    cp->last_fast_run_length = 0; 
    cp->prev_state_xp_TS = 0; 
    cp->prev_pkt_tx_epoch_ms = 0; 
    cp->prev_pkt_state_xp_TS = 0; 
    cp->prev_pkt_num = -1; 
    return;
} // init_packet_stats

void emit_frame_stats_for_per_packet_file (struct frame *fp, struct s_metadata *mdp, struct s_brmdata *brmdp) {

        // Frame metadata for reference (repeated for every packet)
        fprintf (ps_fp, "%u, ", fp->frame_count);
        fprintf (ps_fp, "%u, ", mdp->packet_num);
	    if (fp->latest_packet_num == mdp->packet_num) fprintf (ps_fp, "%u, ", fp->latest_packet_num); else fprintf (ps_fp, ", "); 
        fprintf (ps_fp, "%u, ", fp->late);
	    fprintf (ps_fp, "%u, ", fp->missing);
	    fprintf (ps_fp, "%.1f, ", fp->nm1_bit_rate);
	    fprintf (ps_fp, "%u, ", fp->packet_count);
	    fprintf (ps_fp, "%u, ", mdp->video_packet_len);
        fprintf (ps_fp, "%.1f,", (fp->size*8)/FRAME_PERIOD_MS/1000); 
	    // fprintf (ps_fp, "%.1f, ", fp->tx_epoch_ms_1st_packet - fp->camera_epoch_ms);
	    fprintf (ps_fp, "%.1f, ", fp->rx_epoch_ms_latest_packet - fp->tx_epoch_ms_1st_packet);
	    fprintf (ps_fp, "%.1f, ", fp->rx_epoch_ms_latest_packet - fp->camera_epoch_ms);
	    fprintf (ps_fp, "%d, ", fp->repeat_count);
	    fprintf (ps_fp, "%u, ", fp->skip_count);
	    fprintf (ps_fp, "%.1f, ", fp->c2d_frames);
        fprintf (ps_fp, "%.0lf, ", fp->camera_epoch_ms);

        // Delivered packet meta data
        fprintf (ps_fp, "%u, ", mdp->retx);
        fprintf (ps_fp, "%u, ", mdp->ch);
        if (len_brmd) fprintf (ps_fp, "%d,", brmdp->encoder_state); else fprintf (ps_fp, ",");
        if (mdp->kbps > 0) 
            fprintf (ps_fp, "%.1f, ", ((float) mdp->kbps)/1000);
        else fprintf (ps_fp, ","); 
        fprintf (ps_fp, "%.0lf, ", mdp->tx_epoch_ms);
        fprintf (ps_fp, "%.0lf, ", mdp->rx_epoch_ms);
        fprintf (ps_fp, "%.1f, ", mdp->rx_epoch_ms - fp->camera_epoch_ms);
        fprintf (ps_fp, "%.1f, ", mdp->rx_epoch_ms - mdp->tx_epoch_ms);

        return;
} // emit_frame_stats_for_per_packet_file

// tracks length in packets before a resuming channel goes out of service again
void run_length_state_machine (int channel, struct s_carrier *cp, 
    struct s_service *sd, int len_sd, struct frame *fp, double c2t, int dedup_used_this_channel) {

    // reset outputs from the previous packet (clock)
    // if a run finished with the previous packet, reset run state
    cp->start_of_run_flag = 0; 
    cp->indicator = 0; 

    // run length state machine - changes state only if the channel transferred this packet
    // retx packsts are ignored which is incorrect but very clumsy to handle in this program
    if (cp->tx && !cp->retx) {

        // find the closest state transition preceding this packet's tx
        struct s_service *state_xp = find_closest_sdp (cp->tx_epoch_ms, sd, len_sd);
        if ((state_xp->state == 0) && (cp->tx_epoch_ms <= state_xp->state_transition_epoch_ms))
            // tx occurred in same ms right before channel goes out of service
            // go back one more transition
            state_xp = find_closest_smaller_sdp (state_xp->state_transition_epoch_ms, sd, len_sd); 
        cp->state_xp_TS = state_xp->state_transition_epoch_ms;

        // if state transition occurred between this and previous packet this channel transmitted,  
        // then the previous run finished and a new run has begun with this packet
        if (cp->state_xp_TS != cp->prev_pkt_state_xp_TS) {
            cp->last_run_length = cp->run_length; 
            cp->run_length = 1; 
            cp->start_of_run_flag = 1; 
            cp->enable_indicator = 1; 
            // when the run starts, cp is the first packet that the channel is going to transmit
            // so find the closest packet whose camera_epoch_ms is less than the tx_epoch_ms of this packet
            struct s_metadata *txlastp = find_closest_mdp_by_camera_TS (cp->tx_epoch_ms-c2t, md, len_md);
            cp->pending_packet_count = txlastp->packet_num - cp->packet_num + 1; 
            cp->tgap = (int) (cp->tx_epoch_ms - (txlastp->camera_epoch_ms + c2t)); 
        } // state transition occurred between previous and this packet
        else { // else current run is continuing
            cp->run_length += cp->packet_num - cp->prev_pkt_num; 
        } // current run is continuinging
        
        cp->critical = 
            (cp->run_length <= cp->pending_packet_count) && dedup_used_this_channel? 
            cp->run_length : 0; 
        
        if (cp->enable_indicator) {
            if (cp->critical) { // channel supplied a critical packet from the pending packets
                cp->indicator = 1; 
                cp->enable_indicator = 0; 
            }
            if (cp->run_length > cp->pending_packet_count) { // none of the pending packets were used
                cp->indicator = 2; 
                cp->enable_indicator = 0; 
            }
        }  // if enable_indicator still armed
        
        // copy current pkt info now that done using it for next evaluation of the state
        cp->prev_pkt_tx_epoch_ms = cp->tx_epoch_ms; 
        cp->prev_pkt_state_xp_TS = cp->state_xp_TS;
        cp->prev_pkt_num =  cp->packet_num; 
    } // if the channel participated in this packet
    
    return; 
} // run_length_state_machine

// returns a 1 if the channel is in good shape near the specified epoch_ms
int channel_in_good_shape (
    double epoch_TS,
    char *channel, 
    struct s_service *sd, int len_sd, 
    struct s_latency *ls, int len_ld,
    struct s_txlog *td, int len_td) {

    struct s_service *sdp = find_closest_sdp (epoch_TS, sd, len_sd);
    struct s_latency *lsp = find_closest_lsp (epoch_TS, ls, len_ld); 
    struct s_txlog *tdp = find_closest_tdp (epoch_TS, td, len_td);
    double est_t2r = lsp->t2r_ms + (epoch_TS - lsp->bp_epoch_ms);
    
    int in_good_shape = 
        (sdp->state == 1);    // is in_service state
        /*
        &&        
        (sdp->zeroUplinkQueue || (tdp->occ <= 10)) // occupancy is in good place
        &&
        (est_t2r <= 80);
        */

    if (debug) {
        fprintf (dbg_fp, 
            "ch: %s, sd_TS: %0.lf, IS: %d, zUQ: %d, td_TS: %0.lf, occ: %d, ls_TS: %0.lf, est_t2r: %d, ", 
            channel, sdp->state_transition_epoch_ms,  sdp->state, sdp->zeroUplinkQueue, 
            tdp->epoch_ms, tdp->occ, lsp->bp_epoch_ms, (int) est_t2r);
    }

    return in_good_shape; 
} // channel_in_good_shape

// returns 1 if service should start from the begining of the pending packets queue, 
// 2 otherwise. Also sets the unused_pkts_at_start_of_run field of s_carrier
int resume_from_beginning (
    struct frame *fp, 
    int len_fd,
    struct s_carrier *cp, 
    struct s_service *sd, int len_sd, 
    struct s_txlog *td, int len_td,
    // x and y are the two channels other than the one resuming service
    struct s_carrier *cpx,
    struct s_service *sdx, int len_sdx, 
    struct s_latency *lsx, int len_ldx,
    struct s_txlog *tdx, int len_tdx,
    struct s_carrier *cpy,
    struct s_service *sdy, int len_sdy, 
    struct s_latency *lsy, int len_ldy,
    struct s_txlog *tdy, int len_tdy) {
    
    struct s_service *prev_state_xp = find_closest_smaller_sdp (cp->state_xp_TS, sd, len_sd); 
    cp->prev_state_xp_TS = prev_state_xp->state_transition_epoch_ms; 
    if (debug) fprintf (dbg_fp, "prev_state_x_TS: %0.lf, ", cp->prev_state_xp_TS);

    // oos_duration and socc_when_going_oos 
    cp->out_of_service_duration = cp->state_xp_TS - cp->prev_state_xp_TS; 

    int oos_iocc, oos_socc; double oos_socc_epoch; 
    struct s_txlog *oos_tdp = 
        find_occ_from_td (cp->prev_state_xp_TS, td, len_td, &oos_iocc, &oos_socc, &oos_socc_epoch); 
    cp->socc_when_going_oos = oos_socc; 

    // unused packet computation
    struct s_metadata *mdp = find_closest_mdp_by_rx_TS (cp->rx_epoch_ms, md, len_md);
    cp->unused_pkts_at_start_of_run = mdp->packet_num - cp->packet_num + 1; 

    // determine if other channels are in good shape or not
    cp->channel_x_in_good_shape = 
        channel_in_good_shape (cp->tx_epoch_ms, "x", sdx, len_sdx, lsx, len_ldx, tdx, len_tdx);
    cp->channel_y_in_good_shape =
       channel_in_good_shape (cp->tx_epoch_ms, "y", sdy, len_sdy, lsy, len_ldy, tdy, len_tdy);

    // algo 1 for computing resume_from_begining : did not work well
    // resume from the beining if this channel was out of service for less than a frame time
    // or other channels are not in good shape
    int from_beginning = 
        (cp->out_of_service_duration < FRAME_PERIOD_MS)
        ||
        !(cp->channel_x_in_good_shape || cp->channel_y_in_good_shape);   
    cp->reason_debug = 
        ((cp->out_of_service_duration < FRAME_PERIOD_MS) << 2)
        |
        (cp->channel_x_in_good_shape << 1)
        |
        (cp->channel_y_in_good_shape << 0);

    if (debug) {
        fprintf (dbg_fp, "rb: %d, reason: %d, OOS_ms: %d, ch_x_good: %d, ch_y_good: %d, oos_occ,",
            from_beginning, cp->reason_debug, (int) cp->out_of_service_duration, 
            cp->channel_x_in_good_shape, cp->channel_y_in_good_shape,
            cp->socc_when_going_oos); 
    }

    // algo 2 for determining resume_from_beginning (OVERIDES ALGO 1)
    if (cp->packet_num == 0) { // first packet (trivial case) so start from beginning
        cp->continuing_channel = 1; 
        cp->skip_pkts = 0; 
        cp->reason_debug = 0;
        return 1; 
    }

    // if (cp->packet_num == 6714)
        // printf ("reached 6714\n");

    // check if this channel backpropagated the last retired packet from the encoder queue
    struct s_cmetadata *cdp = find_packet_in_cd (cp->packet_num-1, cp->cdhead, cp->len_cd);
    struct s_cmetadata *cdpx = find_packet_in_cd (cp->packet_num-1, cpx->cdhead, cpx->len_cd);
    struct s_cmetadata *cdpy = find_packet_in_cd (cp->packet_num-1, cpy->cdhead, cpy->len_cd);
    if (// resuming channel is not a continuing channel if it did not transmit packet n-1 OR
        // did not tranmit fast enough relative to other channels
        (cdp == NULL) 
        // ||
        // (cdpx != NULL) && (cdpx->rx_epoch_ms < (cdp->rx_epoch_ms /* - 5 */))
        // ||
        // (cdpy != NULL) && (cdpy->rx_epoch_ms < (cdp->rx_epoch_ms /*- 5 */))
        )
        cp->continuing_channel = 0; 
    else // resuming channel was the first to retire the last packet 
        cp->continuing_channel = 1; 
    
    if (cp->continuing_channel) {
        cp->skip_pkts = 0; 
        cp->reason_debug = 1;
        return 1; 
    } // if resuming channel is a continuing channel
    
    // get here if another channel retired the last packet first

    // find weighted average packet size of the last 3 frames
    int average_frame_szP; 
    // float weight_1 = 1.0/3.0, weight_2 = 1.0/3.0, weight_3 = 1.0/3.0;
    float weight_1 = 0.5, weight_2 = 0.25, weight_3 = 0.25;
    // float weight_1 = 1.0, weight_2 = 0.0, weight_3 = 0.0;
    if (fp->frame_count < 3)
        average_frame_szP = fp->packet_count; 
    else
        average_frame_szP =  (int)
            fp->packet_count * weight_1 + (fp-1)->packet_count * weight_2
            + (fp-2)->packet_count * weight_3;

    // if the channel that retired the last packet first transmitted is out of service, 
    // determine the number of packdets it transmitted beyond this packet before
    // going out of service 
    struct s_carrier *faster_cp;
    struct s_service *sdp; 
    struct s_cmetadata *faster_cdp;
    if (cdpx==NULL 
        ||
        (cdpy != NULL) && (cdpx->rx_epoch_ms > cdpy->rx_epoch_ms)) {
        // channel y is faster
        sdp = find_closest_sdp (cp->tx_epoch_ms, sdy, len_sdy);
        faster_cp = cpy; 
        faster_cdp = cdpy; 
    } else {
        // channel x is faster
        sdp = find_closest_sdp (cp->tx_epoch_ms, sdx, len_sd);
        faster_cp = cpx; 
        faster_cdp = cdpx; 
    }

    float fraction_to_skip = 1.5; // 1.0; // 1.75; 
    if (sdp->state == 1) { // is in service
        cp->skip_pkts = fraction_to_skip * average_frame_szP; // weighted average of the last 3 frames;
        cp->reason_debug = 2; 
    } else { // not in service
        // find number of unretired packets that have been transmitted by this channel
        double oos_TS = sdp->state_transition_epoch_ms; 
        int unretired_pkts = 0; 
        faster_cdp++; // move past the packet_num-1
        while ((faster_cdp < (faster_cp->cdhead+faster_cp->len_cd)) && 
            (faster_cdp->tx_epoch_ms <= oos_TS)) {
            unretired_pkts++;
            faster_cdp++; 
        } // while there are more transmitted pkts before chnnel went oos
        cp->skip_pkts = MIN (unretired_pkts, fraction_to_skip * average_frame_szP);
        cp->reason_debug = 3; 
        cp->unretired_pkts_debug = unretired_pkts; 
    } // not in service 
    return 2; 
} // resume_from_beginning

void print_debug_part1 (int channel, struct s_carrier *cp) {
    if (debug) {
        fprintf (dbg_fp, "channel: %d, pkt_num: %d, tx_TS: %0.lf, state_x_TS: %0.lf, ",
            channel, cp->packet_num, cp->tx_epoch_ms, cp->state_xp_TS);

    }
} // print_debug_part1

// assumes called at the end of a frame after frame and session stats have been updated.
// emits packet stats and analytics output followed by frame stats and analytics
void carrier_metadata_clients (
    int print_header, struct s_session *ssp, struct frame *fp, 
    struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p ) {

    // check if MD_BUFFER is holding all the packets of the frame
	if (fp->packet_count > MD_BUFFER_SIZE) {
        FATAL("Frame has more packets than MD_BUFFER can hold. Increase MD_BUFFER_SIZE\n", "") 
	} 

    // for all packets in the frame
	// md_index point to the first line of the new frame when emit_frame_stats is called
	int i, starting_index;
	starting_index = (md_index + MD_BUFFER_SIZE - fp->packet_count) % MD_BUFFER_SIZE; 
    for (i=0; i < fp->packet_count; i++) {

        struct s_metadata *mdp = md + ((starting_index + i) % MD_BUFFER_SIZE);

        // Per carrier meta data
        initialize_cp_from_cd (mdp, c0p, 0); 
        initialize_cp_from_cd (mdp, c1p, 0); 
        initialize_cp_from_cd (mdp, c2p, 1); // 1 for t-mobile

        // calculate c2t of this packet. since atleat one channel is always in
        // service, assume that c2v of that channel is the correctc c2t.
        double c2t = 999.9; // absurdly large number
        if (c0p->tx) c2t = MIN(c2t, c0p->tx_epoch_ms - fp->camera_epoch_ms); 
        if (c1p->tx) c2t = MIN(c2t, c1p->tx_epoch_ms - fp->camera_epoch_ms); 
        if (c2p->tx) c2t = MIN(c2t, c2p->tx_epoch_ms - fp->camera_epoch_ms); 

        // run length state machine 
        run_length_state_machine (0, c0p, sd0, len_sd0, fp, c2t, mdp->ch==0);
        run_length_state_machine (1, c1p, sd1, len_sd1, fp, c2t, mdp->ch==1);
        run_length_state_machine (2, c2p, sd2, len_sd2, fp, c2t, mdp->ch==2);

        // determine where should a resuming channel start service from
        if (c0p->start_of_run_flag) {
            if (debug) print_debug_part1 (0, c0p); 
            // if (c0p->packet_num == 546)
                // printf ("GOT TO 546\n");
            c0p->resume_from_beginning = resume_from_beginning (
                fp, len_fd,
                c0p, sd0, len_sd0, td0, len_td0, 
                c1p, sd1, len_sd1, ls1, len_ld1, td1, len_td1, 
                c2p, sd2, len_sd2, ls2, len_ld2, td2, len_td2);
            if (debug) fprintf (dbg_fp, "\n"); 
            /*
            struct s_metadata *mdp = 
                find_closest_mdp_by_rx_TS (c0p->rx_epoch_ms, md, len_md);
            c0p->unused_pkts_at_start_of_run = mdp->packet_num - c0p->packet_num + 1; 
            */
        }
        if (c1p->start_of_run_flag) {
            if (debug) print_debug_part1 (1, c1p); 
            c1p->resume_from_beginning = resume_from_beginning (
                fp, len_fd,
                c1p, sd1, len_sd1, td1, len_td1,
                c0p, sd0, len_sd0, ls0, len_ld0, td0, len_td0, 
                c2p, sd2, len_sd2, ls2, len_ld2, td2, len_td2);
            if (debug) fprintf (dbg_fp, "\n"); 
            /*
            struct s_metadata *mdp = 
                find_closest_mdp_by_rx_TS (c1p->rx_epoch_ms, md, len_md);
            c1p->unused_pkts_at_start_of_run = mdp->packet_num - c1p->packet_num + 1; 
            */
               
        }
        if (c2p->start_of_run_flag) {
            if (debug) print_debug_part1 (2, c2p); 
            c2p->resume_from_beginning = resume_from_beginning (
                fp, len_fd,
                c2p, sd2, len_sd2, td2, len_td2, 
                c0p, sd0, len_sd0, ls0, len_ld0, td0, len_td0, 
                c1p, sd1, len_sd1, ls1, len_ld1, td1, len_td1);
            if (debug) fprintf (dbg_fp, "\n"); 
            /*
            struct s_metadata *mdp = 
                find_closest_mdp_by_rx_TS (c2p->rx_epoch_ms, md, len_md);
            c2p->unused_pkts_at_start_of_run = mdp->packet_num - c2p->packet_num + 1;
            */
        }

        // per carrier packet stats
        struct s_brmdata *brmdp;
        brmdp = mdp->ch==0? c0p->brmdp : mdp->ch==1? c1p->brmdp : c2p->brmdp;  
        mdp->kbps = mdp->ch==0? (len_td0? c0p->tdp->actual_rate : -1) : 
                    mdp->ch==1? (len_td1? c1p->tdp->actual_rate : -1) : 
                    (len_td2? c2p->tdp->actual_rate : -1);
        emit_frame_stats_for_per_packet_file (fp, mdp, brmdp); 
        emit_resumption_stats (c0p, c1p, c2p, c2t);
	    emit_packet_stats (c0p, mdp, 0, fp);
	    emit_packet_stats (c1p, mdp, 1, fp);
	    emit_packet_stats (c2p, mdp, 2, fp);

        // per packet analytics
        per_packet_analytics (ssp, fp, mdp, c0p, c1p, c2p); 
	
        fprintf (ps_fp, "\n");
    } // for every packet in the frame

    return;
} // carrier_metadata_clients

// prints out packet level analytics based on the carrier packet metadata
void per_packet_analytics (
    struct s_session *ssp, 
    struct frame *fp,
    struct s_metadata *mdp,
    struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p) {

		// dtx: difference in the tx times of the channels attempting this packet
		double latest_tx, earliest_tx, fastest_tx_to_rx;
	    float c2v_latency = MAX(c0p->tx*(c0p->vx_epoch_ms - fp->camera_epoch_ms), 
	        MAX(c1p->tx*(c1p->vx_epoch_ms - fp->camera_epoch_ms), c2p->tx*(c2p->vx_epoch_ms - fp->camera_epoch_ms))); 
		if (c0p->tx==0) { // c0 is not transmitting
		   latest_tx = MAX(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
		   earliest_tx = MIN(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
		} // c0 is not transmitting
		else if (c1p->tx==0) { // c1 is not transmitting
		   latest_tx = MAX(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
		   earliest_tx = MIN(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
		} // c1 is not transmitting
		else if (c2p->tx==0) { // c2 is not transmitting
		    latest_tx = MAX(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
		    earliest_tx = MIN(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
		} // c2 is not transmitting
        else { // all 3 channels are transmitting
		    latest_tx = MAX(MAX(c0p->tx_epoch_ms, c1p->tx_epoch_ms), c2p->tx_epoch_ms);
		    earliest_tx = MIN(MIN(c0p->tx_epoch_ms, c1p->tx_epoch_ms), c2p->tx_epoch_ms);
        } // all 3 channels transmitting this packet

		fprintf (ps_fp, "%0.1f, ", latest_tx-earliest_tx); 
	
	    // fch: fast channel availability
        // fastest tx_to_rx computed regardless of a channel was transmitting or not to capture the case where a fast
        // channel was avaialble but was not used
        if (c0p->tx==0) {
            if (c1p->tx==0) // only c2 transmitting 001
                fastest_tx_to_rx = c2p->t2r_ms;
            else if (c2p->tx==0) // only c1 transmitting 010
                fastest_tx_to_rx = c1p->t2r_ms;
            else // both c1 and c2 transmitting 011
                fastest_tx_to_rx = MIN(c1p->t2r_ms, c2p->t2r_ms); 
        } // c0 not transmitting;
        else if (c1p->tx==0) {
            if (c2p->tx==0) // only c0 transmitting 100
                fastest_tx_to_rx = c0p->t2r_ms;
            else // both c0 and c2 transmitting 101
                fastest_tx_to_rx = MIN(c0p->t2r_ms, c2p->t2r_ms); 
        }
        else if (c2p->tx==0)  
            // both c0 and c1 transmitting 110
                fastest_tx_to_rx = MIN(c0p->t2r_ms, c1p->t2r_ms); 
        else 
            // all 3 tranmsitting 111
            fastest_tx_to_rx = MIN(c0p->t2r_ms, MIN(c1p->t2r_ms, c2p->t2r_ms));

	    fprintf (ps_fp, "%0.1f, ", fastest_tx_to_rx);

        // eff: time wasted between encoding and transmission
	    fprintf (ps_fp, "%0.1f, ", (mdp->rx_epoch_ms - fp->camera_epoch_ms) - fastest_tx_to_rx - c2v_latency); 
	
	    // update carrier based session packet stats 
	    update_metric_stats (ssp->best_t2rp, 0, fastest_tx_to_rx, MAX_T2R_LATENCY, MIN_T2R_LATENCY);
        if (c0p->tx)
	        update_metric_stats (ssp->c0_est_t2rp, 1, c0p->est_t2r_ms, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);
        if (c1p->tx)
	        update_metric_stats (ssp->c1_est_t2rp, 1, c1p->est_t2r_ms, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);
        if (c2p->tx)
	    update_metric_stats (ssp->c2_est_t2rp, 1, c2p->est_t2r_ms, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);

        // opt: was fastest channel used to transfer this packet
        if ((mdp->rx_epoch_ms - mdp->tx_epoch_ms) < (fastest_tx_to_rx + 2))  // 2 is arbitrary grace duration
            fprintf (ps_fp, "1, "); 
        else
            fprintf (ps_fp, "0, "); 
        
        // fast channel availability
        int c0fast = c0p->t2r_ms < fast_channel_t2r ? 1 : 0; 
        int c1fast = c1p->t2r_ms < fast_channel_t2r ? 1 : 0; 
        int c2fast = c2p->t2r_ms < fast_channel_t2r ? 1 : 0; 

        fprintf (ps_fp, "%d, ", c0fast+c1fast+c2fast == 3? 1: 0);
        fprintf (ps_fp, "%d, ", c0fast+c1fast+c2fast == 2? 1: 0);
        fprintf (ps_fp, "%d, ", c0fast+c1fast+c2fast == 1? 1: 0);
        fprintf (ps_fp, "%d, ", c0fast+c1fast+c2fast == 0? 1: 0);
    
        // update fast_channel_count;
        fp->fast_channel_count += c0fast || c1fast || c2fast; 

        // channels used
        fprintf (ps_fp, "%d, ", c0p->tx + c1p->tx + c2p->tx); 

    return; 
} // per packet_analytics

// assumes called at the end of a frame after frame and session stats have been updated
void emit_frame_stats (int print_header, struct frame *p) {     // last is set 1 for the last frame of the session 

    static int fast_to_slow_edge_count = 0, slow_to_fast_edge_count = 0; 

    if ((p->frame_count % 1000) == 0)
        printf ("at frame %d\n", p->frame_count); 

    // for wats tool
    fprintf (fs_fp, "%u, ", p->frame_count);
    fprintf (fs_fp, "%.0lf, ", p->camera_epoch_ms);
    fprintf (fs_fp, "%u, ", p->late);
    fprintf (fs_fp, "%u, ", p->missing);
    fprintf (fs_fp, "%.1f, ", p->nm1_bit_rate);
    fprintf (fs_fp, "%u, ", p->has_annotation);
    fprintf (fs_fp, "%d, ", (p->fast_channel_count != p->packet_count)); 
    fprintf (fs_fp, "%u, ", p->size);
    fprintf (fs_fp, "%.1f,", (p->size*8)/FRAME_PERIOD_MS/1000); 
    fprintf (fs_fp, "%lf, ", p->coord.lat);
    fprintf (fs_fp, "%lf, ", p->coord.lon);
    fprintf (fs_fp, "%.1f, ", p->speed);
    fprintf (fs_fp, "%d, ", p->repeat_count);
    fprintf (fs_fp, "%u, ", p->skip_count);
    fprintf (fs_fp, "%.1f, ", p->c2d_frames);

    // dlv and ulv
    // frame size modulation analytics
    float frame_t2r = p->rx_epoch_ms_latest_packet - p->tx_epoch_ms_1st_packet;
    int fast_frame = frame_t2r < fast_frame_t2r;
    fast_to_slow_edge_count = fast_frame? 0: fast_to_slow_edge_count + 1;
    slow_to_fast_edge_count = fast_frame? slow_to_fast_edge_count + 1 : 0; 
    // dlv
    if (( fast_to_slow_edge_count > frame_size_modulation_latency)  // don't have fast channel for a while
        && (p->size > frame_size_modulation_threshold)) // but the frame size has not been modulated down
        fprintf (fs_fp, "%d, ", 1);
    else
        fprintf (fs_fp, "%d, ", 0); 
    // ulv
    if (( slow_to_fast_edge_count > frame_size_modulation_latency)  // don't have fast channel for a while
        && (p->size < frame_size_modulation_threshold)) // but the frame size has not been modulated up
        fprintf (fs_fp, "%d, ", 1);
    else
        fprintf (fs_fp, "%d, ", 0); 

    // other frame stats
    fprintf (fs_fp, "%u, ", p->packet_count);
    fprintf (fs_fp, "%u, ", p->first_packet_num);
    fprintf (fs_fp, "%u, ", p->latest_packet_count);
    fprintf (fs_fp, "%u, ", p->latest_packet_num);
    fprintf (fs_fp, "%u, ", p->latest_retx);
    fprintf (fs_fp, "%.1f, ", p->tx_epoch_ms_1st_packet - p->camera_epoch_ms);      // c2t
    fprintf (fs_fp, "%.1f, ", frame_t2r);                                           // t2r
    fprintf (fs_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->camera_epoch_ms);   // Fc2r
    fprintf (fs_fp, "%u, ", p->out_of_order);
    fprintf (fs_fp, "%u, ", p->frame_resolution);

    // time stamps
    fprintf (fs_fp, "%.0lf, ", p->tx_epoch_ms_1st_packet);
    fprintf (fs_fp, "%.0lf, ", p->tx_epoch_ms_last_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_last_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_earliest_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_latest_packet);
    fprintf (fs_fp, "%.0lf, ", p->cdisplay_epoch_ms);
    fprintf (fs_fp, "%.0lf, ", p->ndisplay_epoch_ms);
    fprintf (fs_fp, "%.01f, ", p->early_ms);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_latest_packet - p->rx_epoch_ms_earliest_packet);
    fprintf (fs_fp, "%.0lf, ", p->tx_epoch_ms_last_packet - p->tx_epoch_ms_1st_packet);

    // bit-rate modulation stats
    fprintf (fs_fp, "%d, ", p->brmdp->bit_rate);
    fprintf (fs_fp, "%d, ", p->brmdp->encoder_state);
    if (p->brm_changed) {
        fprintf (fs_fp, "%d, ", p->brm_run_length);
    } else
        fprintf (fs_fp, ","); 
    if (p->brm_changed && (p->brmdp->encoder_state == 0)) // previous state != 0 i.e low quality
        fprintf (fs_fp, "%d,", p->brm_run_length);
    else
        fprintf (fs_fp, ","); 
    fprintf (fs_fp, "%d, ", p->brmdp->channel_quality_state[0]);
    fprintf (fs_fp, "%d, ", p->brmdp->channel_quality_state[1]);
    fprintf (fs_fp, "%d, ", p->brmdp->channel_quality_state[2]);
    fprintf (fs_fp, "%.0lf, ", p->brmdp->epoch_ms);

    fprintf (fs_fp, "\n");
            
    return; 
} // end of emit_frame_stats

void skip_carrier_md_file_header (FILE *fp, char *cname) {
    char line[500];
	if (fgets (line, MAX_LINE_SIZE, fp) == NULL) {
	    printf ("skip_carrier_md_file_header: Empty %s csv file\n", cname);
	    my_exit(-1);
	}
    return;
} // skip_carrier_md_header 

// interpolate_occ returns interpolated value between the current and next value of occ from the tx log file
int interpolate_occ (double tx_epoch_ms, struct s_txlog *current, struct s_txlog *next) {

    if (current == next) 
        return (current->occ);
    
    float left_fraction = (next->epoch_ms - tx_epoch_ms) / (next->epoch_ms - current->epoch_ms);
        return ((left_fraction * current->occ) + ((1-left_fraction) * next->occ));

} // interpolate occ

// find_occ_from_td  returns the sampled occupancy at the closest time smaller than the specified tx_epoch_ms 
// and interpolated occupancy, interporated between the sampled occupancy above and the next (later) sample
// also returns the pointer to the td array element used to return the socc
struct s_txlog *find_occ_from_td (
    double tx_epoch_ms, struct s_txlog *head, int len_td, 
    int *iocc, int *socc, double *socc_epoch_ms) {

    struct s_txlog *left, *right, *current;    // current, left and right index of the search

    left = head; right = head + len_td - 1; 

    if (tx_epoch_ms < left->epoch_ms) // tx started before modem occupancy was read
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is smaller than first occupancy sample time\n", tx_epoch_ms)

    if (tx_epoch_ms > right->epoch_ms + 100) // tx was done significantly later than last occ sample
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is over 100ms largert than last occupancy sample time\n", tx_epoch_ms)

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (tx_epoch_ms > current->epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        }
        else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search

    // on exiting the while the left is smaller than tx_epoch_ms, the right can be bigger or equal 
    // so need to check if the right should be used
    if (tx_epoch_ms == right->epoch_ms) 
        current = right; 

    *socc = current->occ; 
    *iocc = interpolate_occ (tx_epoch_ms, current, MIN((current+1), (head+len_td-1)));
    *socc_epoch_ms = current->epoch_ms; 

    return current;
} // find_occ_from_td

// find_closest_tdp returns pointer to the td array element at closest time smaller than the 
// specified tx_epoch_ms 
struct s_txlog *find_closest_tdp (double tx_epoch_ms, struct s_txlog *head, int len_td) {

    struct s_txlog *left, *right, *current;    // current, left and right index of the search

    left = head; right = head + len_td - 1; 

    if (tx_epoch_ms < left->epoch_ms) // tx started before modem occupancy was read
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is smaller than first occupancy sample time\n", tx_epoch_ms)

    if (tx_epoch_ms > right->epoch_ms + 100) // tx was done significantly later than last occ sample
        FWARN(warn_fp, "find_occ_from_tdfile: Packet with tx_epoch_ms %0.lf is over 100ms largert than last occupancy sample time\n", tx_epoch_ms)

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (tx_epoch_ms > current->epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search

    // on exiting the while the left is smaller than tx_epoch_ms, the right can be bigger or equal 
    // so need to check if the right should be used
    if (tx_epoch_ms == right->epoch_ms) 
        current = right; 

    return current;
} // find_closest_tdp

// returns pointer to equal to or closest smaller brmdata to the specified epoch_ms
struct s_brmdata *find_closest_brmdp (double epoch_ms, struct s_brmdata *headp, int len) {

    struct  s_brmdata  *left, *right, *current;    // current, left and right index of the search

    left = headp; right = headp + len - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (epoch_ms > current->epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    if (right->epoch_ms == epoch_ms)
        current = right; 

    return current; 
} // find_closest_brmdp

// returns pointer to equal to or closest smaller mdp to the specified camera_epoch_ms
struct s_metadata *find_closest_mdp_by_camera_TS (double camera_epoch_ms, struct s_metadata *headp, int len) {

    struct  s_metadata  *left, *right, *current;    // current, left and right index of the search

    left = headp; right = headp + len - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (camera_epoch_ms > current->camera_epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than camera_epoch_ms
    if (right->camera_epoch_ms == camera_epoch_ms) {
        // move to the right most (latest) packet
        current = right; 
        while ((current+1 < headp+len-1) && ((current+1)->camera_epoch_ms==camera_epoch_ms))
            current++;
    }

    return current; 
} // find_closest_mdp_by_camera_TS

// returns pointer closest smaller mdp by tx_epoch to the specified TS
struct s_metadata *find_closest_mdp_by_rx_TS (double TS_ms, struct s_metadata *headp, int len) {

    struct  s_metadata  *left, *right, *current;    // current, left and right index of the search

    left = headp; right = headp + len - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (TS_ms > current->rx_epoch_ms) {
            left = current;
            current = right - (right - left)/2; 
        } else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    return current; 
} // find_closest_mdp_by_rx_TS

// returns pointer to equal to or avgqdata closest to the specified packet number with closest tx_epoc
struct s_avgqdata *find_avgqdp (int packet_num, double tx_epoch_ms, struct s_avgqdata *avgqdatap, int len_avgqd) {

    struct  s_avgqdata  *left, *right, *current;    // current, left and right index of the search

    left = avgqdatap; right = avgqdatap + len_avgqd - 1; 

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (packet_num > current->packet_num) {
            left = current;
            current = right - (right-left)/2;
        }
        else {
            right = current; 
            current = left + (right - left)/2; 
        }
    } // while there are more than 2 elements left to search
    
    // when the while exits, the current is equal to (if current = left edge) or less than epoch_ms
    if ((right->packet_num !=packet_num) && (left->packet_num != packet_num))
        // no match. should not really happen
        FWARN(warn_fp, "find_avgqdp: could not find packet number %d in avgq array\n", packet_num)

    if (right->packet_num == packet_num)
        left = right; 
    
    // now search to the right and see which element is closest is the specified tx_epoch_ms
    struct s_avgqdata *smallest = left; 
    while ((left != (avgqdatap+len_avgqd-1)) && ((++left)->packet_num == packet_num)) {
        if ((smallest->tx_epoch_ms - tx_epoch_ms) > (left->tx_epoch_ms - tx_epoch_ms)) 
            smallest = left; 
    }
    return smallest;

} // find_avgqdp

/*
#define FORWARD_PROP_DELAY 30
// find the t2r delay of the nearest packet revceived at least FORWARD_PROP_DELAY ago
// from the specified tx_epoch_ms
float bp_t2r_ms (struct s_carrier *cp, struct s_cmetadata *csp, int len_cs) {

    struct s_cmetadata *left, *right, *current;    // current, left and right index of the search

    left = csp; right = csp + len_cs - 1; 
    
    // adjust the tx_epoch_ms with the forward propagation delayo
    double tx_epoch_ms = cp->tx_epoch_ms - FORWARD_PROP_DELAY;

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (tx_epoch_ms > current->rx_epoch_ms)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search
    cp->bp_csp = current; 

    // find the t2r delay of the nearest packet revceived at least FORWARD_PROP_DELAY ago
    // from the specified tx_epoch_ms
    struct s_carrier bcp;
    bcp.vx_epoch_ms = current->vx_epoch_ms; 
    decode_sendtime (&(bcp.vx_epoch_ms), &(bcp.tx_epoch_ms), &(bcp.socc)); 
    float t2r = current->rx_epoch_ms - bcp.tx_epoch_ms; 

    return t2r; 
} // bp_t2r_ms
*/

// scans for the packet in specified mdp in the carrier meta data array and
// initialize cp with the relevant info
void initialize_cp_from_cd ( struct s_metadata *mdp, struct s_carrier *cp, int t_mobile) {

    struct s_cmetadata *cdp; 

    // if this carrier transmitted this meta data line, then print the carrier stats
	if (cdp = find_packet_in_cd (mdp->packet_num, cp->cdhead, cp->len_cd)) { 
        // this channel participated in this packet
        if (cp->len_avgqd = cdp->len_avgqd) cp->avgqdp = cdp->avgqdp;
        if (cp->len_ld = cdp->len_ld) {cp->ldp = cdp->ldp, cp->lsp = cdp->lsp;}
        if (len_brmd) cp->brmdp = cdp->brmdp;

        cp->packet_num = cdp->packet_num; 
        cp->vx_epoch_ms = cdp->vx_epoch_ms; 
        cp->tx_epoch_ms = cdp->tx_epoch_ms; 
        cp->socc = cdp->socc; 
        if (cp->len_td) cp->tdp = find_occ_from_td (cp->tx_epoch_ms, cp->tdhead, cp->len_td, 
            &(cp->iocc), &(cp->socc), &(cp->socc_epoch_ms));
        cp->rx_epoch_ms = cdp->rx_epoch_ms;
        cp->retx = cdp->retx;
        cp->t2r_ms = cp->rx_epoch_ms - cp->tx_epoch_ms;
        if (cp->len_ld) cp->r2t_ms = cp->ldp->bp_epoch_ms - cp->rx_epoch_ms;
        if (cp->len_ld) cp->est_t2r_ms = cp->lsp->t2r_ms + MAX(0, (cp->tx_epoch_ms - cp->lsp->bp_epoch_ms)); // max if bp_epoch > tx_epoch
        cp->ert_ms = 
            (t_mobile? 75 : 30) + 20 +                      // avg r2t + 3-sigma
            ((cp->socc < 10)? 30 : cp->est_t2r_ms) +        // avg t2r (3-sigma in guardband)
            60;                                             // guardband
        cp->ert_epoch_ms = cp->tx_epoch_ms + cp->ert_ms;
        cp->tx = 1; 
    } // if this carrier participated in transmitting this mdp 
	else {
        cp->tx = 0; 
        cp->packet_num = -1; 
        if (cp->len_td) // tx log file exists
            // if tx log file exists then get occ from there, even if the channel did not
            // transmit this packet since tx log is independent of transmit participation
            // **** need to change this when probe packet data is available
            cp->tdp = find_occ_from_td (mdp->tx_epoch_ms, cp->tdhead, cp->len_td, 
                &(cp->iocc), &(cp->socc), &(cp->socc_epoch_ms));
            // no new back propagated information available, so retain the previous value
    } // this carrier did not participate in transmitting this mdp

    return;
} // initialize_cp_from_cd

// outputs resumption time of any one of the 3 channels. If multiple channels are resuming
// at the same time then the smallest channel number is printed
void emit_resumption_stats (
    struct s_carrier *c0p, struct s_carrier *c1p, struct s_carrier *c2p, double c2t) {

        // c2t
        fprintf (ps_fp, "%d,", (int) c2t);

        // service resumption inidcator
        // somewhat incorrect if multiple channels are resuming at the same time
        // which does not happen very often
        if (c0p->tx && c0p->start_of_run_flag)  { // res
	        fprintf (ps_fp, "%0.1f,", c0p->tx_epoch_ms); 
            fprintf (ps_fp, "0,");
        }
        else if (c1p->tx && c1p->start_of_run_flag)  { // res
	        fprintf (ps_fp, "%0.1f,", c1p->tx_epoch_ms); 
            fprintf (ps_fp, "1,");
        }
        else if (c2p->tx && c2p->start_of_run_flag)  {// res
	        fprintf (ps_fp, "%0.1f,", c2p->tx_epoch_ms); 
            fprintf (ps_fp, "2,");
        }
        else fprintf (ps_fp, ",,");                                                              

        return;
} // emit_resumption_stats

// outputs per carrier stats and fills out some of the fields of s_carrier structure from cd array
void emit_packet_stats (struct s_carrier *cp, struct s_metadata *mdp, int carrier_num, struct frame *fp) {

	if (cp->tx) {
        // if this carrier transmitted this packet
	    fprintf (ps_fp, "%0.1f,", cp->rx_epoch_ms - fp->camera_epoch_ms);                       // c2r
	    fprintf (ps_fp, "%0.1f,", cp->vx_epoch_ms - fp->camera_epoch_ms);                       // c2v
	    fprintf (ps_fp, "%0.1f,", cp->t2r_ms);                                                  // t2r
	    if (cp->len_ld) fprintf (ps_fp, "%0.1f,", cp->r2t_ms); else fprintf (ps_fp, ",");       // r2t
	    if (cp->len_ld) fprintf (ps_fp, "%0.1f,", cp->est_t2r_ms);                              // est_t2r
        if (cp->len_ld) fprintf (ps_fp, "%.0f,", cp->ert_ms); else fprintf (ps_fp, ",");        // ert
        fprintf (ps_fp, "%d,", cp->socc);                                                       // socc
        if (cp->len_td) fprintf (ps_fp, "%.1f,", ((float) cp->tdp->actual_rate)/1000);          // MMbps
        else fprintf (ps_fp, ",");                                                              
        fprintf (ps_fp, "%d,", cp->retx);                                                       // retx
        if (cp->start_of_run_flag) { // channel entering service
            fprintf (ps_fp, "%d,", cp->tgap);                                                   // tgap
            fprintf(ps_fp, "%d,", cp->pending_packet_count);                                    // pptks
            fprintf(ps_fp, "%d,", cp->unused_pkts_at_start_of_run);                             // upkts
            fprintf(ps_fp, "%d,", cp->skip_pkts);                                               // spkts
            fprintf(ps_fp, "%d,", cp->continuing_channel);                                      // cont
            fprintf (ps_fp, "%d,", cp->unused_pkts_at_start_of_run - cp->skip_pkts);            // Inc
            fprintf (ps_fp, "%d,", cp->reason_debug); // rdbg
            if (cp->reason_debug==3)
                fprintf (ps_fp, "%d,", cp->unretired_pkts_debug); // urpkt
            else
                fprintf (ps_fp,","); // urpkt
            fprintf (ps_fp, "%d,", (int) cp->out_of_service_duration); // oos_d
            fprintf (ps_fp, "%d,", cp->socc_when_going_oos); // oos_o
        }
        else {
            fprintf (ps_fp,","); // pending packets time gap (tgap)
            fprintf (ps_fp,","); // pending packet count (ppkts)
            fprintf (ps_fp,","); // unused packets at the start of the run (upkts)
            fprintf (ps_fp,","); // spkts
            fprintf (ps_fp,","); // cont
            fprintf (ps_fp,","); // Inc
            fprintf (ps_fp,","); // rdbg
            fprintf (ps_fp,","); // urpkt
            fprintf (ps_fp,","); // oos_d
            fprintf (ps_fp,","); // oos_o
        }
        fprintf (ps_fp, "%d,", cp->critical); // cr
        fprintf (ps_fp, "%d,", cp->indicator); // I
        if (cp->start_of_run_flag) {
            fprintf (ps_fp, "%d,", cp->resume_from_beginning); //  rb
            if (cp->resume_from_beginning == 2)
                fprintf (ps_fp, "%d,", cp->bp_TS_gap); // bp_gap
            else
                fprintf (ps_fp, ","); 
            fprintf (ps_fp, "%d,", cp->channel_x_in_good_shape); // x
            fprintf (ps_fp, "%d,", cp->channel_y_in_good_shape); // y
        } else {
            fprintf (ps_fp,","); // rb
            fprintf (ps_fp,","); // bp_gap
            fprintf (ps_fp,","); // x
            fprintf (ps_fp,","); // y
        }

        if (cp->start_of_run_flag) {
            fprintf(ps_fp, "%d,", cp->last_run_length); // rlen
            fprintf(ps_fp, "%d,", cp->run_length); // flen
            fprintf (ps_fp, "%d,", cp->last_run_length - cp->last_fast_run_length); // I
        } else {
            fprintf (ps_fp,","); // rlen
            fprintf(ps_fp, "%d,", cp->run_length); // flen
            fprintf (ps_fp,","); // I 
        }

	    fprintf (ps_fp, "%.0lf,", cp->tx_epoch_ms);                                             // tx_TS
	    fprintf (ps_fp, "%.0lf,", cp->rx_epoch_ms);                                             // rx_TS
	    if (cp->len_ld) fprintf (ps_fp, "%.0lf,", cp->ldp->bp_epoch_ms); else fprintf (ps_fp, ",");// r2t_TS
	    if (cp->len_td) fprintf (ps_fp, "%.0lf,", cp->ert_epoch_ms); else fprintf (ps_fp, ","); //ert_TS
	    if (cp->len_td) fprintf (ps_fp, "%.0lf,", cp->socc_epoch_ms); else fprintf (ps_fp, ",");// socc_TS 
	    if (cp->len_ld) fprintf (ps_fp, "%.0lf,", cp->lsp->bp_epoch_ms); else fprintf (ps_fp, ",");// bp_pkt_TS 
	    if (cp->len_ld) fprintf (ps_fp, "%d,", cp->lsp->packet_num); else fprintf (ps_fp, ","); // bp_pkt
	    if (cp->len_ld) fprintf (ps_fp, "%d,", cp->lsp->t2r_ms); else fprintf (ps_fp, ",");     // bp_t2r

        // print average socc stats
        if (cp->len_avgqd) fprintf (ps_fp, "%0.1f,", cp->avgqdp->rolling_average); else fprintf (ps_fp, ","); 
        fprintf (ps_fp, "%d,", 1);  // in service
        if (cp->len_avgqd) fprintf (ps_fp, "%d,", cp->avgqdp->quality_state); else fprintf (ps_fp, ","); 
        if (cp->len_avgqd) fprintf (ps_fp, "%d,", cp->avgqdp->queue_size); else fprintf (ps_fp, ","); 
        if (cp->len_avgqd) fprintf (ps_fp, "%.0lf,", cp->avgqdp->tx_epoch_ms); else fprintf (ps_fp, ","); 
        if (cp->len_avgqd) fprintf (ps_fp, "%d,", cp->avgqdp->packet_num); else fprintf (ps_fp, ","); 
    } // if this carrier transmitted this meta data line, then print the carrier stats 

	else { // stay silent except indicate the channel quality occ and run lengths
	    fprintf (ps_fp, ",");       // c2r
	    fprintf (ps_fp, ",");       // c2v
        fprintf (ps_fp, ",");       // t2r
        fprintf (ps_fp, ",");       // r2t
        fprintf (ps_fp, ",");       // est_t2r
        fprintf (ps_fp, ",");       // ert
        fprintf (ps_fp, "%d,", cp->socc);
        if (cp->len_td) fprintf (ps_fp, "%.1f,", ((float) cp->tdp->actual_rate)/1000); else fprintf (ps_fp, ",");
        fprintf (ps_fp, ",");       // retx
        fprintf (ps_fp, ",");       // pending packets time gap (tgap)
        fprintf (ps_fp, ",");       // pending_packet_count (ppkts)
        fprintf (ps_fp, ",");       // unsed packets at the start of a run (upkts)
        fprintf (ps_fp, ",");       // spkts
        fprintf (ps_fp, ",");       // cont
        fprintf (ps_fp, ",");       // Inc(orrect)
        fprintf (ps_fp, ",");       // rdbg
        fprintf (ps_fp, ",");       // urpkt
        fprintf (ps_fp, ",");       // oos_d
        fprintf (ps_fp, ",");       // oos_o
        fprintf (ps_fp, ",");       // cr
        fprintf (ps_fp, ",");       // indicator
        fprintf (ps_fp, ",");       // rb
        fprintf (ps_fp, ",");       // bp_gap
        fprintf (ps_fp, ",");       // x
        fprintf (ps_fp, ",");       // y
        if (cp->start_of_run_flag) {
            fprintf(ps_fp, "%d,", cp->last_run_length); // rlen
            fprintf(ps_fp, "%d,", cp->run_length); // flen
            fprintf (ps_fp, "%d,", cp->last_run_length - cp->last_fast_run_length); // I
        } else {
            fprintf (ps_fp,","); // rlen
            fprintf(ps_fp, "%d,", cp->run_length); // flen
            fprintf (ps_fp,","); // I 
        }
        fprintf (ps_fp, ",");       // tx_TS
        fprintf (ps_fp, ",");       // rx_TS
        fprintf (ps_fp, ",");       // r2t_TS
        fprintf (ps_fp, ",");       // ert_TS
	    if (cp->len_td) fprintf (ps_fp, "%.0lf,", cp->socc_epoch_ms); else fprintf (ps_fp, ","); 
        fprintf (ps_fp, ",");       // bp_pkt_TS
        fprintf (ps_fp, ",");       // bp_pkt
        fprintf (ps_fp, ",");       // bp_t2r

        fprintf (ps_fp, ","); 
        fprintf (ps_fp, "%d,", 0);  // not in serivce
        fprintf (ps_fp, ","); 
        fprintf (ps_fp, ","); 
        fprintf (ps_fp, ","); 
        fprintf (ps_fp, ","); 
    } // this carrier did not transmit the packet

    return;
} // emit_packet_stats

// Accumulates stats at the end of every frame. 
// assumes that it is called only at the end of a frame
void update_session_frame_stats (struct s_session *ssp, struct frame *p) { 
    float   latency;                        // camera timestamp to the last rx packet latency of the current frame

    // annotation stat
    p->has_annotation = check_annotation (p->frame_count); 

    // packet count stats
    update_metric_stats (ssp->pcp, p->packet_count, p->packet_count, MAX_PACKETS_IN_A_FRAME, MIN_PACKETS_IN_A_FRAME);

    // frame latency stats
    latency = p->rx_epoch_ms_latest_packet - p->camera_epoch_ms; 
    if (latency < 0) {
        printf ("Negative transit latency %.1f for frame %u starting at %.0lf\n", latency, p->frame_count, p->tx_epoch_ms_1st_packet); 
        my_exit (-1);
    }
    update_metric_stats (ssp->lp, 0, latency, MAX_TRANSIT_LATENCY_OF_A_FRAME, MIN_TRANSIT_LATENCY_OF_A_FRAME);

    // frame byte count stats
    if (p->size <= 0) {
        printf ("Invalid frame size %u for frame %u starting at %.0lf\n", p->size, p->frame_count, p->tx_epoch_ms_1st_packet);
        my_exit (-1);
    }
    update_metric_stats (ssp->bcp, p->size, p->size, MAX_BYTES_IN_A_FRAME, MIN_BYTES_IN_A_FRAME);

    // late and incomplete frames stats
    update_metric_stats (ssp->latep, p->late > 0, p->late, MAX_LATE_PACKETS_IN_A_FRAME, MIN_LATE_PACKETS_IN_A_FRAME);
    update_metric_stats (ssp->ip, p->missing > 0, p->missing, MAX_MISSING_PACKETS_IN_A_FRAME, MIN_MISSING_PACKETS_IN_A_FRAME); 
    update_metric_stats (ssp->op, p->out_of_order > 0, p->out_of_order, MAX_OOO_PACKETS_IN_A_FRAME, MIN_OOO_PACKETS_IN_A_FRAME); 
    
    // bit rate stat
    if (p->nm1_bit_rate<=0) {
        printf ("Invalid bit rate %.1f for frame %u starting at %.0lf\n", p->nm1_bit_rate, p->frame_count, p->tx_epoch_ms_1st_packet); 
        my_exit (-1); 
    }
    update_metric_stats (ssp->brp, p->nm1_bit_rate < minimum_acceptable_bitrate, p->nm1_bit_rate, MAX_BIT_RATE_OF_A_FRAME, MIN_BIT_RATE_OF_A_FRAME); 
    
    // camera timestamp
    if (p->frame_count > 1) // skip first frame because the TS for n-1 frame is undefined 
        update_metric_stats (ssp->ctsp, 1, p->camera_epoch_ms - p->nm1_camera_epoch_ms, 60, 30); 

    // c2d latency
    update_metric_stats (ssp->c2dp, 1, p->c2d_frames, 10, 2);

    // c2d latency
    update_metric_stats (ssp->rptp, 1, p->repeat_count, 10, 0);

} // update_session_frame_stats

// Computes the mean/variance of the specified metric. 
void compute_metric_stats (struct stats *p, unsigned count) {
    p->mean /= count;              // compute EX
    p->var /= count;               // compute E[X^2]
    p->var -= p->mean * p->mean;         // E[X^2] - EX^2
} // compute stats

void emit_session_stats (struct s_session *ssp, struct frame *fp) {
    char buffer[500];
    compute_metric_stats (ssp->pcp, fp->frame_count); 
    compute_metric_stats (ssp->lp, fp->frame_count); 
    compute_metric_stats (ssp->bcp, fp->frame_count); 
    compute_metric_stats (ssp->ip, fp->frame_count); 
    compute_metric_stats (ssp->op, fp->frame_count); 
    compute_metric_stats (ssp->latep, fp->frame_count); 
    compute_metric_stats (ssp->brp, fp->frame_count); 
    compute_metric_stats (ssp->ctsp, fp->frame_count); 
    compute_metric_stats (ssp->c2dp, fp->frame_count); 
    compute_metric_stats (ssp->rptp, fp->frame_count); 
    compute_metric_stats (ssp->c2vp, ssp->pcp->count); 
    compute_metric_stats (ssp->v2tp, ssp->pcp->count); 
    compute_metric_stats (ssp->best_t2rp, ssp->pcp->count); 
    compute_metric_stats (ssp->c0_est_t2rp, ssp->c0_est_t2rp->count); 
    compute_metric_stats (ssp->c1_est_t2rp, ssp->c1_est_t2rp->count); 
    compute_metric_stats (ssp->c2_est_t2rp, ssp->c2_est_t2rp->count); 
    compute_metric_stats (ssp->c2rp, ssp->pcp->count); 

    fprintf (ss_fp, "Total number of frames in the session, %u\n", fp->frame_count); 
    emit_metric_stats ("Frames with late packets", "Late Packets distribution", ssp->latep, 1, MAX_LATE_PACKETS_IN_A_FRAME, MIN_LATE_PACKETS_IN_A_FRAME);
    emit_metric_stats ("Frame Latency", "Frame Latency", ssp->lp, 0, MAX_TRANSIT_LATENCY_OF_A_FRAME, MIN_TRANSIT_LATENCY_OF_A_FRAME);
    emit_metric_stats ("Frames with missing packets", "Missing Packets distribution",  ssp->ip, 1, MAX_MISSING_PACKETS_IN_A_FRAME, MIN_MISSING_PACKETS_IN_A_FRAME);
    emit_metric_stats ("Frames with out of order packets", "OOO Packets", ssp->op, 1, MAX_OOO_PACKETS_IN_A_FRAME, MIN_OOO_PACKETS_IN_A_FRAME);
    sprintf (buffer, "Frames with bit rate below %.1fMbps", minimum_acceptable_bitrate);
    emit_metric_stats (buffer, "Bit-rate", ssp->brp, 1, MAX_BIT_RATE_OF_A_FRAME, MIN_BIT_RATE_OF_A_FRAME); 
    emit_metric_stats ("Camera time stamp", "Camera time stamp", ssp->ctsp, 1, 60, 30); 
    emit_metric_stats ("Camera to display latency", "Camera to display latency", ssp->c2dp, 1, 10, 2); 
    emit_metric_stats ("Repeated frames", "Repeated frames", ssp->rptp, 1, 10, 0); 
    emit_metric_stats ("C->V latency", "C->V latency", ssp->c2vp, 0, MAX_C2V_LATENCY, MIN_C2V_LATENCY); 
    emit_metric_stats ("V->T latency", "V->T latency", ssp->v2tp, 0, 50, 0); 
    emit_metric_stats ("Best TX->RX latency", "Best TX->RX latency", ssp->best_t2rp, 0, MAX_T2R_LATENCY, MIN_T2R_LATENCY); 
    emit_metric_stats ("C0 est_t2r", "C0 est_t2r", ssp->c0_est_t2rp, 1, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);
    emit_metric_stats ("C1 est_t2r", "C1 est_t2r", ssp->c1_est_t2rp, 1, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);
    emit_metric_stats ("C2 est_t2r", "C2 est_t2r", ssp->c2_est_t2rp, 1, MAX_EST_T2R_LATENCY, MIN_EST_T2R_LATENCY);
    emit_metric_stats ("C->Rx latency", "C->Rx latency", ssp->c2rp, 0, MAX_C2R_LATENCY, MIN_C2R_LATENCY); 
    emit_metric_stats ("Packets in frame", "Packets in frame", ssp->pcp, 1, MAX_PACKETS_IN_A_FRAME, MIN_PACKETS_IN_A_FRAME); 
    emit_metric_stats ("Bytes in frame", "Bytes in frame", ssp->bcp, 1, MAX_BYTES_IN_A_FRAME, MIN_BYTES_IN_A_FRAME);

} // end of emit_session_stats

// emits the stats for the specified metric
void emit_metric_stats (
    char            *p1,            // name of the metric 
    char            *p2,            // name of the metric
    struct stats    *s,             // pointer to where the stats are stored
    int             print_count,    // count not printed if print_count = 0
    double          range_max,      // min and max range this metric can take in a frame
    double          range_min) {   

    int         index; 
    double      bin_size = (range_max - range_min)/ NUMBER_OF_BINS;

    fprintf (ss_fp, "%s\n", p1);
    if (print_count) fprintf (ss_fp, "Total, %u\n", s->count);
    fprintf (ss_fp, "Mean, %.1f\n", s->mean);
    fprintf (ss_fp, "std. dev., %.1f\n", sqrt(s->var));
    fprintf (ss_fp, "Min, %.1f\n", s->min);
    fprintf (ss_fp, "Max, %.1f\n", s->max);

    // first line of the latency histogram; blank first cell, then bin names
    fprintf (ss_fp, " "); 
    for (index=0; index < NUMBER_OF_BINS; index++)
        fprintf (ss_fp, ", %.1f-%.1f", range_min + index*bin_size, range_min + (index+1)*bin_size);
    fprintf (ss_fp, "\n"); 
    // second lline of the latency histogram; 
    fprintf (ss_fp, "%s distribution", p2);
    for (index=0; index < NUMBER_OF_BINS; index++)
        fprintf (ss_fp, ", %.1f", s->distr[index]);
    fprintf (ss_fp, "\n");
    return; 
} // end of emit_metric_stats

void skip_combined_md_file_header (FILE *fp) {
    char line[500];
    // skip header
    if (fgets (line, MAX_LINE_SIZE, fp) == NULL) {
	    printf ("skip_combined_file_header: Empty combined csv file\n");
	    my_exit(-1);
    }
    return;
} // skip_combined_md_file_header

// reads next line of the meta data file. returns 0 if reached end of file
int read_md (int skip_header, FILE *fp, struct s_metadata *p) {
    char    mdline[MAX_LINE_SIZE], *mdlp = mdline; 

    // read next line
    if (fgets (mdlp, MAX_LINE_SIZE, md_fp) != NULL) {

        // parse the line
        if (sscanf (mdlp, "%u, %lf, %lf, %u, %u, %u, %u, %u, %u, %lf, %u, %u", 
            &p->packet_num, 
            &p->vx_epoch_ms,
            &p->rx_epoch_ms,
            &p->video_packet_len,
            &p->frame_start, 
            &p->rolling_frame_number,
            &p->frame_rate,
            &p->frame_resolution,
            &p->frame_end,
            &p->camera_epoch_ms,
            &p->retx, 
            &p->ch) != NUM_OF_MD_FIELDS) {
            printf ("Insufficient number of fields in line: %s\n", mdlp);
            my_exit(1);
        } // scan did not succeed

        else { // successful scan
            decode_sendtime (&p->vx_epoch_ms, &p->tx_epoch_ms, &p->socc);
            if (p->camera_epoch_ms == 0)
                p->camera_epoch_ms = (p-1)->camera_epoch_ms; 
            return 1;
        } // successful scan

    } // if were able to read a line from the file
    
    // get here at the end of the file
    return 0;
} // end of read_md

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (double *vx_epoch_ms, double *tx_epoch_ms, int *socc) {
            double real_vx_epoch_ms;
            unsigned tx_minus_vx; 
            
            // calculate tx-vx and modem occupancy
            if (new_sendertime_format == 1) {
                real_vx_epoch_ms = /* starts at bit 8 */ trunc (*vx_epoch_ms/256);
                *socc = 31; // not available
                tx_minus_vx =  /* lower 8 bits */ *vx_epoch_ms - real_vx_epoch_ms*256;
            } // format 1:only tx-vx available
            else if (new_sendertime_format == 2) {
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
} // end of fix_vx_tx_epoch_ms

// called after the last packet of the frame is received. Updates stats for the current frame
// calculates and updates bit_rate. If end of the file then checks the last frame for missing packets
void update_bit_rate (struct frame *fp, struct s_metadata *lmdp, struct s_metadata *cmdp) {
    double transit_time;

    //bit-rate. For the first frame, bit-rate is approximate bit rate of the. For subsequent frames bit-rate is 
    // the bit-rate of the previous frame
    if (fp->frame_count==1) {
        // first frame of the session. so we don't know the earliest arriving packet of the previous frame
        // so we will use the time between earliest arriving packet and last packet as the transit time
        transit_time = lmdp->rx_epoch_ms - fp->rx_epoch_ms_earliest_packet; 
        fp->nm1_size = fp->size; // since nm1_size would have been invalid in frame_init
    } else 
        // use the time elapsed between earliest packet of the previous vs this frame
        transit_time = fp->rx_epoch_ms_earliest_packet - fp->nm1_rx_epoch_ms_earliest_packet;

    if (transit_time > 0) // update bit-rate only if it is computable
        fp->nm1_bit_rate = (fp->nm1_size * 8) / 1000 / transit_time;

} // end of update_bit_rate

// called after receiving the last packet of the frame and updates brm stats
void update_brm (struct frame *fp) {

    struct s_brmdata *brmdp = find_closest_brmdp (fp->tx_epoch_ms_1st_packet, brmdata, len_brmd); 

    if (fp->frame_count==1) // first frame
        fp->brm_changed = 1; 
    else
        fp->brm_changed = (brmdp->encoder_state != fp->brmdp->encoder_state);

    if (!fp->brm_changed)
        fp->brm_run_length++;   // will be set to 1 in init_frame if brm_changed == 1

    fp->brmdp = brmdp; 
    return; 
} // update_brm


// initializes session stats
void init_session_stats (struct s_session *p) {
    p->pcp = &(p->pc); init_metric_stats (p->pcp);
    p->lp = &(p->l); init_metric_stats (p->lp);
    p->bcp = &(p->bc); init_metric_stats (p->bcp);
    p->ip = &(p->i); init_metric_stats (p->ip);
    p->op = &(p->o); init_metric_stats (p->op);
    p->latep = &(p->late); init_metric_stats (p->latep);
    p->brp = &(p->br); init_metric_stats (p->brp);
    p->ctsp = &(p->cts); init_metric_stats (p->ctsp);
    p->c2dp = &(p->c2d); init_metric_stats (p->c2dp);
    p->rptp = &(p->rpt); init_metric_stats (p->rptp);
    p->c2vp = &(p->c2v); init_metric_stats (p->c2vp);
    p->v2tp = &(p->v2t); init_metric_stats (p->v2tp);
    p->c2rp = &(p->c2r); init_metric_stats (p->c2rp);
    p->best_t2rp = &(p->best_t2r); init_metric_stats (p->best_t2rp);
    p->c0_est_t2rp = &(p->c0_est_t2r); init_metric_stats (p->c0_est_t2rp);
    p->c1_est_t2rp = &(p->c1_est_t2r); init_metric_stats (p->c1_est_t2rp);
    p->c2_est_t2rp = &(p->c2_est_t2r); init_metric_stats (p->c2_est_t2rp);
} // end of init_session_stats

// initializes the session stat structures for the specified metrics
void init_metric_stats (struct stats *p) {
    int i; 
    p->count = p->mean = p->var = p->min = p->max = 0;
    for (i=0; i<NUMBER_OF_BINS; i++)
        p->distr[i] = 0; 
} // end of init_metric_stats

// Collects data for calculating mean/variance/distribution for the specified metric
void update_metric_stats (
    struct stats *p,                        // metric being updated
    unsigned count,                         // number of frames this metric occurred in the session
    double value,                            // value of the metric in the frame
    double range_max,                        // max value this mnetric can take in a frame
    double range_min) {                      // min value this metric can take in a frame

    int index; 
    double bin_size = (range_max - range_min)/NUMBER_OF_BINS;

    if (bin_size < 0) {
        printf("Invalid bin size: max=%.2f min=%.2f", range_max, range_min);
        my_exit (-1); 
    }
    p->count += count;
    p->mean += value;                       // storing sum (X) till the end
    p->var += value * value;                // storing sum (X^2) till the end
    p->max = MAX(p->max, value);
    p->min = p->min==0?                     // first time, so can't do min
        value : MIN(p->min, value); 
    index = MIN(MAX(trunc((value-range_min) / bin_size), 0), NUMBER_OF_BINS-1);
    p->distr[index]++;

    return;
} // end of update stats