#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define		FATAL(STR, ARG) {printf (STR, ARG); my_exit(-1);}
#define		WARN(STR, ARG) {if (!silent) printf (STR, ARG);}
#define		FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}
#define     MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define     MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define     MATCH   0
#define     HZ_30   0
#define     HZ_15   1
#define     HZ_10   2
#define     HZ_5    3
#define     RES_HD  0
#define     RES_SD  1
#define     MD_BUFFER_SIZE 35
#define     MAX_LINE_SIZE    1000 
#define     NUM_OF_MD_FIELDS    12
#define     NUM_OF_PARAMS 3
#define     NUMBER_OF_BINS      20
#define     LATENCY_BIN_SIZE    10
#define     BIT_RATE_BIN_SIZE   0.5
#define     MAX_PACKETS_IN_A_FRAME  30
#define     MIN_PACKETS_IN_A_FRAME  1
#define     MAX_TRANSIT_LATENCY_OF_A_FRAME  500
#define     MIN_TRANSIT_LATENCY_OF_A_FRAME  100
#define     MAX_BYTES_IN_A_FRAME    25000
#define     MIN_BYTES_IN_A_FRAME    2000
#define     MAX_LATE_PACKETS_IN_A_FRAME     20
#define     MIN_LATE_PACKETS_IN_A_FRAME     0
#define     MAX_MISSING_PACKETS_IN_A_FRAME  20
#define     MIN_MISSING_PACKETS_IN_A_FRAME  0 
#define     MAX_OOO_PACKETS_IN_A_FRAME  20
#define     MIN_OOO_PACKETS_IN_A_FRAME  0 
#define     MAX_BIT_RATE_OF_A_FRAME 10
#define     MIN_BIT_RATE_OF_A_FRAME 0
#define     MAX_NUM_OF_ANNOTATIONS 100
#define     MAX_C2V_LATENCY 200
#define     MIN_C2V_LATENCY 20
#define     MAX_C2R_LATENCY 200
#define     MIN_C2R_LATENCY 20
#define     MAX_T2R_LATENCY 100
#define     MIN_T2R_LATENCY 10
#define     FRAME_PERIOD_MS 33.364
#define 	MAX_GPS			25000				// maximum entries in the gps file. fatal if there are more.
#define     TX_BUFFER_SIZE (20*60*1000)

struct meta_data {
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
};

struct s_coord {
	double	lon;
	double	lat;
}; 

struct frame {
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

	struct s_coord	coord;						// interpolated coordinates based on gps file
	float			speed; 						// interpolated speed of this frame

}; 

struct stats {
    unsigned    count;
    double      mean; 
    double      var;
    double      min;
    double      max;
    double      distr[NUMBER_OF_BINS]; 
};

struct s_txlog {
// uplink_queue. ch: 2, timestamp: 1672344732193, queue_size: 23, elapsed_time_since_last_queue_update: 29, actual_rate: 1545, stop_sending_flag: 0, zeroUplinkQueue_flag: 0, lateFlag: 1
    int channel;                    // channel number 0, 1 or 2
    double epoch_ms;                // time modem occ was sampled
    int occ;                        // occupancy 0-30
    int time_since_last_update;     // of occupancy for the same channel
    int actual_rate;
};
                    
struct session_stats {
    unsigned        frame_count;            // total fames in the session
    struct stats    pc, *pcp;               // packet count stats: count=total packets in the session, rest per frame
    struct stats    l, *lp;                 // latecncy stats: count=n/a, rest per frame
    struct stats    bc, *bcp;               // byte count stats: count = total bytes in the session, rest per frame
    struct stats    i, *ip;                 // Frames with missing packets stats: count = number of incomplete frames in the session; 
    struct stats    o, *op;                 // Frames with out of order packets stats: count = number of OOO frames in the session; 
    struct stats    late, *latep;           // late frame stats; count = number of late frames in the session; 
    struct stats    br, *brp;               // frame bit-rate stats: count = number of frames below too_low_bit_rate parameter
    struct stats    cts, *ctsp;             // camera time-stamp
    // packet level stats
    struct stats    c2v, *c2vp;             // camera to video latency
    struct stats    v2t, *v2tp;             // best encoder to transmit for a packet
    struct stats    c2r, *c2rp;             // camera to receiver latency
    struct stats    best_t2r, *best_t2rp;   // best tx to rx delay for a packet
};

struct s_carrier {
        char line[MAX_LINE_SIZE];        // line to read the carrier data in 
        char *lp;                           // pointer to line
        int tx;                             // set to 1 if this carrier tranmitted the packet being considered
        int packet_num;                     // packet number read from this line of the carrier meta_data finle 
        double vx_epoch_ms; 
        double tx_epoch_ms; 
        double rx_epoch_ms;
        float t2r;                          // tx-rx of the last transmitted packet
        int socc;                           // sampled occupancy either from the tx log or the rx metadata file
        int iocc;                           // interpolated occupancy from tx log
        double socc_epoch_ms;               // time when the occupacny for this packet was sampled

        struct s_txlog *tdp;                // pointer to the tx data array
        int len_td;                         // number of entries in the td array

        FILE *fp;                           // carrier metadata file pointer
        char name[100];                     // carrier name
};

struct s_gps {
	double 			epoch_ms; 					// time stamp
	int 			mode;						// quality indicator
	struct s_coord	coord; 						// lon, lat of the gps point (at 1Hz)
	float			speed; 						// vehicle speed 
	int				count; 						// number of frames that mapped into this gps coord
}; 

// frees up storage before exiting
int my_exit (int n);

// reads a new meta data line. returns 0 if reached end of the file
int read_md (
    int skip_header,                        // if 1 then skip the header lines
    FILE *fp, 
    struct meta_data *p);    

// initializes the frame structure for a new frame
void init_frame (
    struct frame *fp,
    struct meta_data *lmdp,                 // last packet's meta data
    struct meta_data *cmdp,                 // current packet's meta data; current may be NULL at EOF
    int    first_frame);                    // 1 if first frame

// updates per packet stats of the frame
void update_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct meta_data *lmdp,                 // last packet's meta data
    struct meta_data *cmdp);                // current packet's meta data; current may be NULL at EOF

// calculates and update bit-rate of n-1 frame
void update_bit_rate (                      // updates stats of the last frame
    struct frame *fp,
    struct meta_data *lmdp,                 // last packet's meta data
    struct meta_data *cmdp);                 // current packet's meta data; current may be NULL at EOF

// assumes called at the end of a frame after frame and session stats have been updated
void emit_frame_stats (
    int print_header,                       // set to 1 if only header is to be emitted
    struct frame *p,                        // current frame pointer
    int last);                              // set to 1 when called with the last frame of the session

// initializes the session stat structures for the specified metrics
void init_metric_stats (
    struct stats *p);

// initializes session stats
void init_session_stats (
    struct session_stats *p);

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
void update_session_stats (                 // updates stats for the specified frame
    struct frame *p);

// updates session per packet stats - called after every meta data line read
void update_session_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct meta_data *mdp); 

// emits the stats for the specified metric
void emit_metric_stats (
    char            *p1,                     // name of the metric
    char            *p2,                     // name of the metric
    struct stats    *s,                     // pointer to where the stats are stored
    int             print_count,            // count not printed if print_count = 0
    double          range_max,              // min and max range this metric can take in a frame
    double          range_min);

// outputs session  stats
void emit_session_stats (void);

// prints program usage
void print_usage (void);

// returns 1 if successfully read the annotation file
int read_annotation_file (void); 

// returns 1 if the current frame count is marked in the annotation file
int check_annotation (unsigned frame_num); 

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (double *vx_epoch_ms, double *tx_epoch_ms, int *socc);

// outputs per carrier stats
void emit_carrier_stat (
    int print_header, 
    struct s_carrier *cp, 
    struct meta_data *mdp, 
    struct frame *fp);

void emit_packet_header (FILE *fp);

void emit_frame_header (FILE *fp);

void skip_combined_md_file_header (FILE *fp);

void skip_carrier_md_file_header (FILE *fp, char *cname);

void skip_combined_md_file_header (FILE *fp);

void print_command_line (FILE *fp);

// returns number of enteries found in the gps file
int read_gps (FILE *);							// reads and error checks the gps file

// returns number of entries in the transmit log datat file
int read_td (FILE *);

// returns 1 if able to find coordinates for the frame; else 0
int get_gps_coord (struct frame *);

FILE *open_file (char *filep, char *modep);

// globals - basically command line inputs and storage
int     silent = 0;                             // if 1 then suppresses warning messages
char    ipath[500], *ipathp = ipath;            // input files directory
char    opath[500], *opathp = opath;            // output files directory
char    tx_pre[500], *tx_prep = tx_pre;         // transmit log prefix not including the .log extension
char    rx_pre[500], *rx_prep = rx_pre;         // receive side meta data prefix not including the .csv extension
FILE    *md_fp = NULL;                          // meta data file name
FILE    *an_fp = NULL;                          // annotation file name
FILE    *fs_fp = NULL;                          // frame statistics output file
FILE    *ss_fp = NULL;                          // statistics output file name
FILE    *lf_fp = NULL;                          // late frame output file name
FILE    *c0_fp = NULL;                          // carrier 0 meta data file
FILE    *c1_fp = NULL;                          // carrier 0 meta data file
FILE    *c2_fp = NULL;                          // carrier 0 meta data file
FILE    *gps_fp = NULL;                         // gps data file
FILE    *td_fp = NULL;                          // transmit log data file
FILE    *warn_fp = NULL;                        // warning outputs go to this file
char    annotation_file_name[500];              // annotation file name
struct  session_stats sstat, *ssp=&sstat;       // session stats 
float   minimum_acceptable_bitrate = 0.5;       // used for stats generation only
unsigned maximum_acceptable_c2rx_latency = 110; // frames considered late if the latency exceeds this 
struct  meta_data md[MD_BUFFER_SIZE];           // buffer to store meta data lines*/
int     md_index;                               // current meta data buffer pointer
unsigned anlist[MAX_NUM_OF_ANNOTATIONS][2];     // mmanual annotations
int     len_anlist=0;                           // length of the annotation list
int     have_carrier_metadata = 0;              // set to 1 if per carrier meta data is available
int     have_tx_log = 0;                        // set to 1 if transmit log is available
int     new_sendertime_format = 2;              // set to 1 if using embedded sender time format
int     verbose = 1;                            // 1 or 2 for the new sender time formats
int     rx_jitter_buffer_ms = 10;               // buffer to mitigate skip/repeats due to frame arrival jitter
int     fast_channel_t2r = 40;                  // channels with latency lower than this are considered fast
int     fast_frame_t2r = 60;                    // frames with latency lower than this are considered fast
int     frame_size_modulation_latency = 3;      // number of frames the size is expected to be modulated up or down
int     frame_size_modulation_threshold = 6000; // size in bytes the frame size is expected to be modulated below or aboveo
struct  s_gps gps[MAX_GPS]; int	len_gps; 		// gps data array number of valid entries in gps arraynt     
char    comamnd_line[5000]; 
struct  s_txlog *td0=NULL, *td1=NULL, *td2=NULL;// transmit log file stored in this array
int     len_td0=0, len_td1=0, len_td2=0;        // len of the transmit data logfile

int main (int argc, char* argv[]) {
    unsigned    waiting_for_first_frame;        // set to 1 till first clean frame start encountered
    struct      meta_data *cmdp, *lmdp;         // last and current meta data lines
    struct      frame cf, *cfp = &cf;           // current frame
    char buffer[1000], *bp=buffer;              // temp_buffer

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

        // receive dedup meta data file prefix
        else if (strcmp (*argv, "-rx_pre") == MATCH) {
            strcpy (rx_prep, *++argv); 
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

        // invalid argument
        else {
            printf ("Invalid argument %s\n", *argv);
            print_usage (); 
            my_exit (-1); 
        }
    } // while there are more arguments to process

    // open remaining input and output files
    // transmit log file
    len_td0 = len_td1 = len_td2 = 0; 
    if (have_tx_log) {
         sprintf (bp, "%s%s.log", ipathp, tx_prep); 
        if ((td_fp = open_file (bp, "r")) != NULL) {
            read_td(td_fp);
        }
    }

    // dedup metadata file
    sprintf (bp, "%s%s.csv", ipathp, rx_prep);
    md_fp = open_file (bp, "r");
        
    // per carrier meta data files
    if (have_carrier_metadata) {

        sprintf (bp, "%s%s_ch0.csv", ipathp, rx_prep); 
        c0_fp = open_file (bp, "r");

        sprintf (bp, "%s%s_ch1.csv", ipathp, rx_prep);
        c1_fp = open_file (bp, "r");

        sprintf (bp, "%s%s_ch2.csv", ipathp, rx_prep);
        c2_fp = open_file (bp, "r");

    } // per carrier meta data files

    // output files
    sprintf (bp, "%s%s_packet_vqfilter.csv",opath, rx_prep); 
    lf_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_frame_vqfilter.csv", opath, rx_prep);
    fs_fp = open_file (bp, "w");
            
    sprintf (bp, "%s%s_session_vqfilter.csv", opath, rx_prep);
    ss_fp = open_file (bp, "w");

    // check if any missing arguments
    if (md_fp==NULL || fs_fp==NULL || ss_fp==NULL || lf_fp==NULL) {
        print_usage (); 
        my_exit (-1);
    }

    // control initialization
    waiting_for_first_frame = 1; 

    // frame structures intializetion
    lmdp = md; 
    lmdp->rx_epoch_ms = -1; // so out-of-order calculations for the first frame are correct.

    cmdp = lmdp+1; 
    md_index = 1; 

    // sesstion stat structures intialization
    init_session_stats (ssp);

    // skip/print headers
    skip_combined_md_file_header (md_fp);
    if (have_carrier_metadata) {
        skip_carrier_md_file_header (c0_fp, "C0");
        skip_carrier_md_file_header (c1_fp, "C1");
        skip_carrier_md_file_header (c2_fp, "C2");
    }
    emit_frame_header (fs_fp);
    emit_packet_header (lf_fp);
    print_command_line (ss_fp);

    // while there are more lines to read from the meta data file
    while (read_md (0, md_fp, cmdp) != 0) { 

        //  if first frame then skip till a clean frame start 
        if (waiting_for_first_frame) {
            if (cmdp->frame_start) {
                // found a clean start
                init_frame (cfp, lmdp, cmdp, 1); // first frame set to 1
                get_gps_coord (cfp); 
                update_session_packet_stats (cfp, cmdp);
                waiting_for_first_frame = 0;
            } else 
                // skip packets till we find a clean start 
                ; 
        } 

        // else if conntinuation of the current frame
        else if (cmdp->rolling_frame_number == lmdp->rolling_frame_number) {

            // update per packet stats of the current frame
            update_packet_stats (cfp, lmdp, cmdp);
            update_session_packet_stats (cfp, cmdp);
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
		    if (ssp->frame_count < 1) { // we just reached the end of first frame
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

            // update session stat so the frame number etc. is correct
            update_session_stats (cfp);

            // emit frame stats 
            emit_frame_stats (0, cfp, 0);

            // now process the meta data line of the new frame
            init_frame (cfp, lmdp, cmdp, 0);    
            get_gps_coord (cfp); 
            update_session_packet_stats (cfp, cmdp);
        } // start of a new frame

        md_index = (md_index + 1) % MD_BUFFER_SIZE; 
        lmdp = cmdp;
        cmdp = &md[md_index]; 
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
    update_session_stats (cfp);

    // end of the file so emit both last frame and session stats
    emit_frame_stats(0, cfp, 1);
    emit_session_stats();

    my_exit (0); 
} // end of main

// free up storage before exiting
int my_exit (int n) {

    if (td0 != NULL) free (td0); 
    if (td1 != NULL) free (td1); 
    if (td2 != NULL) free (td2); 
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

#define MAX_TD_LINE_SIZE 1000
// reads and parses a tx log file. Returns 0 if end of file reached
int read_td_line (FILE *fp, int ch, struct s_txlog *tdp) {

    char tdline[MAX_TD_LINE_SIZE], *tdlinep = tdline; 
    char dummy_string[100];

    if (fgets (tdline, MAX_TD_LINE_SIZE, fp) == NULL)
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

        if (fgets (tdline, MAX_TD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    } // while not successfully scanned a transmit log line

    return 1;
} // end of read_td_line

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

// returns number of entries in the transmit log datat file. Modifiels global td array
int read_td (FILE *fp) {

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
		while (read_td_line (fp, i, tdp)) {
		    (*len_td)++;
		    if (*len_td == TX_BUFFER_SIZE)
		        FATAL ("TX data array is not large enough to read the tx log file. Increase TX_BUFFER_SIZE%S\n", "")
		    tdp++;
		} // while there are more lines to be read

		if (*len_td == 0)
		    FATAL("Meta data file is empty%s", "\n")

		// sort by timestamp
        sort_td (tdp, *len_td); 

        // reset the file pointer to the start of the file for the next channel
        fseek (fp, 0, SEEK_SET);
    } // for each carrier

} // read_td

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
    char    *usage2 = "[-a <file name>] [-gps <prefex>] [-tx_pre <prefix>] [-mdc] -ipath -opath -rx_pre <prefix> ";
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
    printf ("\t-mdc shouild be used if per channel meta data is also avalible. Name of the per channel file should be <rx_prefix>_ch0/1/2\n");
    printf ("\t-ipath input directory\n");
    printf ("\t-opath output directory\n");
    printf ("\t-rx_pre prefix (without .csv) of the receive side dedup meta data file without the .csv. Output files take the same prefix.");
    return; 
} // print_usage

// updates session per packet stats - called after every meta data line read
void update_session_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct meta_data *mdp) {               // current packet's meta data; current may be NULL at EOF
    update_metric_stats (ssp->c2vp, 0, mdp->vx_epoch_ms - fp->camera_epoch_ms, MAX_C2V_LATENCY, MIN_C2V_LATENCY);
    update_metric_stats (ssp->v2tp, 0, mdp->tx_epoch_ms - mdp->vx_epoch_ms, 50, 0);
    update_metric_stats (ssp->c2rp, 0, mdp->rx_epoch_ms - fp->camera_epoch_ms, MAX_C2R_LATENCY, MIN_C2R_LATENCY);
} // end of update_session_packet_stats

// updates per packet stats of the frame
void update_packet_stats (
    struct frame *fp,                       // current frame pointer
    struct meta_data *lmdp,                 // last packet's meta data
    struct meta_data *cmdp) {               // current packet's meta data; current may be NULL at EOF

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

} // end of update_packet_stats

// initializes the frame structure for a new frame
void init_frame (
    struct frame *fp,                       // current frame pointer
    struct meta_data *lmdp,                 // last packet's meta data
    struct meta_data *cmdp,                 // current packet's meta data; current may be NULL at EOF
    int	   first_frame) {                   // 1 if it is the first frame. NOT USED

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
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? 33.364 : fp->frame_rate==HZ_15? 66.66 : fp->frame_rate==HZ_10? 100 : 200;
    } else {
        // abrupt start of this frame AND abrupt end of the previous frame. Now we can't tell how many packets each frame lost
        // have assigned one missing packet to previous frame. So will assign the remaining packets to this frame. 
        fp->missing = cmdp->packet_num - (lmdp->packet_num +1) -1; 
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? 33.364 : fp->frame_rate==HZ_15? 33.364*2: fp->frame_rate==HZ_10? 100 : 200;
    }
    fp->out_of_order = /* first_frame? 0: */ cmdp->rx_epoch_ms < lmdp->rx_epoch_ms; 
    fp->first_packet_num = fp->last_packet_num = cmdp->packet_num;
    fp->tx_epoch_ms_1st_packet = fp->tx_epoch_ms_last_packet = cmdp->tx_epoch_ms;
    fp->rx_epoch_ms_last_packet = fp->rx_epoch_ms_earliest_packet = fp->rx_epoch_ms_latest_packet = cmdp->rx_epoch_ms;
    fp->latest_retx = cmdp->retx; 
    fp->frame_rate = cmdp->frame_rate; 
	fp->display_period_ms = fp->frame_rate==HZ_30? 33.364 : fp->frame_rate==HZ_15 ? 33.364*2 : fp->frame_rate==HZ_10? 100 : 200;
    fp->frame_resolution = cmdp->frame_resolution;
    fp->late = cmdp->rx_epoch_ms > (fp->camera_epoch_ms + maximum_acceptable_c2rx_latency);
    fp->packet_count = fp->latest_packet_count = 1; 
    fp->latest_packet_num = cmdp->packet_num; 
    fp->fast_channel_count = 0;

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
    print_command_line (fp); 

    // frame stat header
    fprintf (fp, "F#, ");
    fprintf (fp, "CTS, ");
    fprintf (fp, "Late, ");
    fprintf (fp, "Miss, ");
    fprintf (fp, "Mbps, ");
    fprintf (fp, "anno, ");
    fprintf (fp, "0fst, ");
    fprintf (fp, "SzB, ");
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
    
    fprintf (fp, "\n"); 

} // emit_frame_header

void emit_packet_header (FILE *lf_fp) {

    // command line
    print_command_line (lf_fp); 

    fprintf (lf_fp, "F#, ");
    fprintf (lf_fp, "P#, ");
	fprintf (lf_fp, "LPN, ");
    fprintf (lf_fp, "Late, ");
	fprintf (lf_fp, "Miss, ");
	fprintf (lf_fp, "Mbps, ");
	fprintf (lf_fp, "SzP, ");
	fprintf (lf_fp, "SzB, ");
	fprintf (lf_fp, "Fc2t, ");
	fprintf (lf_fp, "Ft2r, ");
	fprintf (lf_fp, "Fc2r, ");
	fprintf (lf_fp, "Rpt, ");
	fprintf (lf_fp, "skp, ");
	fprintf (lf_fp, "c2d, ");
    fprintf (lf_fp, "CTS, ");

    // Delivered packet meta data
    fprintf (lf_fp, "retx, ");
    fprintf (lf_fp, "ch, ");
    fprintf (lf_fp, "tx_TS, ");
    fprintf (lf_fp, "rx_TS, ");
    fprintf (lf_fp, "Pt2r, ");
    fprintf (lf_fp, "Pc2r, ");
    
    // per carrier meta data
    fprintf (lf_fp, "C0: c2v, v2t, t2r, c2r, socc, iocc, socc_TS, tx_TS, "); 
    fprintf (lf_fp, "C1: c2v, v2t, t2r, c2r, socc, iocc, socc_TS, tx_TS, "); 
    fprintf (lf_fp, "C2: c2v, v2t, t2r, c2r, socc, iocc, socc_TS, tx_TS, "); 

    // packet analytics 
    fprintf (lf_fp, "dtx, fch, eff, opt, "); 
    fprintf (lf_fp, "3fch, 2fch, 1fch, 0fch, cUse, ");

    fprintf (lf_fp, "\n");
    return;
} // emit_packet_header

// assumes called at the end of a frame after frame and session stats have been updated
void emit_frame_stats (int print_header, struct frame *p, int last) {     // last is set 1 for the last frame of the se//ssion 
    static struct s_carrier c0, c1, c2; 
    struct s_carrier *c0p=&c0, *c1p=&c1, *c2p=&c2; 
    static int fast_to_slow_edge_count = 0, slow_to_fast_edge_count = 0; 

    // initialization
    if (ssp->frame_count == 1) {
        c0p->packet_num = -1;               // have not read any lines yet
        c0p->socc =0;                       // assume nothing in the buffer
        c0p->t2r = 30;                      // default value before the first packet is transmiited by this carrier
        c0p->tdp = td0;                     // tx data array
        c0p->len_td = len_td0;              // lenght of the tx data array
        c0p->fp = c0_fp;
        sprintf (c0p->name, "ch0"); 

        c1p->packet_num = -1;               // have not read any lines yet
        c1p->socc =0;                       // assume nothing in the buffer
        c1p->t2r = 30;                      // default value before the first packet is transmiited by this carrier
        c1p->tdp = td1;                     // tx data array
        c1p->len_td = len_td1;              // lenght of the tx data array
        c1p->fp = c1_fp;
        sprintf (c1p->name, "ch1"); 

        c2p->packet_num = -1;               // have not read any lines yet
        c2p->socc =0;                       // assume nothing in the buffer
        c2p->t2r = 30;                      // default value before the first packet is transmiited by this carrier
        c2p->tdp = td2;                     // tx data array
        c2p->len_td = len_td2;              // lenght of the tx data array
        c2p->fp = c2_fp;
        sprintf (c2p->name, "ch2"); 
    }

    if ((ssp->frame_count % 1000) == 0)
        printf ("at frame %d\n", ssp->frame_count); 

    //
    // packet stats
    //

	// md_index point to the first line of the new frame when emit_frame_stats is called
	int i, starting_index, current_index; 
	if (p->packet_count > MD_BUFFER_SIZE) {
	    printf ("Warning: Frame has more packets than MD_BUFFER can hold. Increase MD_BUFFER_SIZE\n"); 
	    starting_index = (md_index + 1) % MD_BUFFER_SIZE; 
	} 
    else 
	    starting_index = (md_index + MD_BUFFER_SIZE - p->packet_count) % MD_BUFFER_SIZE; 

    // for all packets in the frame
    for (i=0; i < p->packet_count; i++) {
        current_index = (starting_index + i) % MD_BUFFER_SIZE;
        struct meta_data *mdp = &md[current_index]; 

        // Frame metadata for reference (repeated for every packet)
        fprintf (lf_fp, "%u, ", ssp->frame_count);
        fprintf (lf_fp, "%u, ", mdp->packet_num);
	    if (p->latest_packet_num == mdp->packet_num) fprintf (lf_fp, "%u, ", p->latest_packet_num); else fprintf (lf_fp, ", "); 
        fprintf (lf_fp, "%u, ", p->late);
	    fprintf (lf_fp, "%u, ", p->missing);
	    fprintf (lf_fp, "%.1f, ", p->nm1_bit_rate);
	    fprintf (lf_fp, "%u, ", p->packet_count);
	    fprintf (lf_fp, "%u, ", p->size);
	    fprintf (lf_fp, "%.1f, ", p->tx_epoch_ms_1st_packet - p->camera_epoch_ms);
	    fprintf (lf_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->tx_epoch_ms_1st_packet);
	    fprintf (lf_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->camera_epoch_ms);
	    fprintf (lf_fp, "%d, ", p->repeat_count);
	    fprintf (lf_fp, "%u, ", p->skip_count);
	    fprintf (lf_fp, "%.1f, ", p->c2d_frames);
        fprintf (lf_fp, "%.0lf, ", p->camera_epoch_ms);

        // Delivered packet meta data
        fprintf (lf_fp, "%u, ", mdp->retx);
        fprintf (lf_fp, "%u, ", mdp->ch);
        fprintf (lf_fp, "%.0lf, ", mdp->tx_epoch_ms);
        fprintf (lf_fp, "%.0lf, ", mdp->rx_epoch_ms);
        fprintf (lf_fp, "%.1f, ", mdp->rx_epoch_ms - mdp->tx_epoch_ms);
        fprintf (lf_fp, "%.1f, ", mdp->rx_epoch_ms - p->camera_epoch_ms);

        // Per carrier meta data
        if (have_carrier_metadata) {

	        emit_carrier_stat (print_header, c0p, mdp, p);
	        emit_carrier_stat (print_header, c1p, mdp, p);
	        emit_carrier_stat (print_header, c2p, mdp, p);
	
            //
            // packet analytics
            //

		    double latest_tx, earliest_tx, fastest_tx_to_rx;
	        float c2v_latency = MAX(c0p->tx*(c0p->vx_epoch_ms - p->camera_epoch_ms), 
	            MAX(c1p->tx*(c1p->vx_epoch_ms - p->camera_epoch_ms), c2p->tx*(c2p->vx_epoch_ms - p->camera_epoch_ms))); 
		    if (c0p->tx==0) { // c0 is not transmitting
		       latest_tx = MAX(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
		       earliest_tx = MIN(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
               // fastest_tx_to_rx = MIN(c1p->t2r, c2p->t2r);
		    } // c0 is not transmitting
		    else if (c1p->tx==0) { // c1 is not transmitting
		       latest_tx = MAX(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
		       earliest_tx = MIN(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
               // fastest_tx_to_rx = MIN(c0p->t2r, c2p->t2r);
		    } // c1 is not transmitting
		    else if (c2p->tx==0) { // c2 is not transmitting
		        latest_tx = MAX(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
		        earliest_tx = MIN(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
                // fastest_tx_to_rx = MIN(c0p->t2r, c1p->t2r);
		    } // c2 is not transmitting
            else { // all 3 channels are transmitting
		        latest_tx = MAX(MAX(c0p->tx_epoch_ms, c1p->tx_epoch_ms), c2p->tx_epoch_ms);
		        earliest_tx = MIN(MIN(c0p->tx_epoch_ms, c1p->tx_epoch_ms), c2p->tx_epoch_ms);
                // fastest_tx_to_rx = MIN(c0p->t2r, MIN(c1p->t2r, c2p->t2r));
            } // all 3 channels transmitting this packet

		    // dtx: difference in the tx times of the channels attempting this packet
		    fprintf (lf_fp, "%0.1f, ", latest_tx-earliest_tx); 
	
	        // fch: fast channel availability
            // fastest tx_to_rx computed regardless of a channel was transmitting or not to capture the case where a fast
            // channel was avaialble but was not used
            fastest_tx_to_rx = MIN(c0p->t2r, MIN(c1p->t2r, c2p->t2r));
	        fprintf (lf_fp, "%0.1f, ", fastest_tx_to_rx);

            // eff: time wasted between encoding and transmission
	        fprintf (lf_fp, "%0.1f, ", (mdp->rx_epoch_ms-p->camera_epoch_ms) - fastest_tx_to_rx - c2v_latency); 
	
	        // update session packet stats (wrong place for this code)
	        // update_metric_stats (ssp->c2vp, 0, c2v_latency, MAX_C2V_LATENCY, MIN_C2V_LATENCY);
	        update_metric_stats (ssp->best_t2rp, 0, fastest_tx_to_rx, MAX_T2R_LATENCY, MIN_T2R_LATENCY);

            // opt: was fastest channel used to transfer this packet
            if ((mdp->rx_epoch_ms - mdp->tx_epoch_ms) < (fastest_tx_to_rx + 2))  // 2 is arbitrary grace duration
                fprintf (lf_fp, "1, "); 
            else
                fprintf (lf_fp, "0, "); 
            
            // fast channel availability
            int c0fast = c0p->t2r < fast_channel_t2r ? 1 : 0; 
            int c1fast = c1p->t2r < fast_channel_t2r ? 1 : 0; 
            int c2fast = c2p->t2r < fast_channel_t2r ? 1 : 0; 

            fprintf (lf_fp, "%d, ", c0fast+c1fast+c2fast == 3? 1: 0);
            fprintf (lf_fp, "%d, ", c0fast+c1fast+c2fast == 2? 1: 0);
            fprintf (lf_fp, "%d, ", c0fast+c1fast+c2fast == 1? 1: 0);
            fprintf (lf_fp, "%d, ", c0fast+c1fast+c2fast == 0? 1: 0);
        
            // update fast_channel_count;
            p->fast_channel_count += c0fast || c1fast || c2fast; 

            // channels used
            fprintf (lf_fp, "%d, ", c0p->tx + c1p->tx + c2p->tx); 

        } // have carrier meta data 
        fprintf (lf_fp, "\n");

    } // for every packet in the frame

    //
    // frame stats 
    //
    // for wats tool
    fprintf (fs_fp, "%u, ", ssp->frame_count);
    fprintf (fs_fp, "%.0lf, ", p->camera_epoch_ms);
    fprintf (fs_fp, "%u, ", p->late);
    fprintf (fs_fp, "%u, ", p->missing);
    fprintf (fs_fp, "%.1f, ", p->nm1_bit_rate);
    fprintf (fs_fp, "%u, ", p->has_annotation);
    fprintf (fs_fp, "%d, ", (p->fast_channel_count != p->packet_count)); 
    fprintf (fs_fp, "%u, ", p->size);
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

int read_carrier_md (int skip_header, FILE *fp, char *line, struct s_carrier *cp, char *cname) {

	if (fgets(line, MAX_LINE_SIZE, fp) != NULL) {
	    if (sscanf (line, "%u, %lf, %lf,", &cp->packet_num, &cp->vx_epoch_ms, &cp->rx_epoch_ms) !=3) {
	        printf ("could not find all the fields in the %s meta data file line: %s\n", cname, line); 
            return 0; // consider it end of file
	    } // if scan failed
        else // scan passed
            return 1;
    } // fgets got a line
    else // reached end of file
        return 0;
} // read_carrier_md

// interpolate_occ returns interpolated value between the current and next value of occ from the tx log file
int interpolate_occ (double tx_epoch_ms, struct s_txlog *current, struct s_txlog *next) {

    if (current == next) 
        return (current->occ);
    
    float left_fraction = (next->epoch_ms - tx_epoch_ms) / (next->epoch_ms - current->epoch_ms);
        return ((left_fraction * current->occ) + ((1-left_fraction) * next->occ));

} // interpolate occ

// find_occ_frim_tdfile  returns the sampled occupancy at the closest time smaller than the specified tx_epoch_ms and that time i
// and // interpolated occupancy, interporated between the sampled occupancy above and the next (later) sample
void find_occ_from_tdfile (int packet_num, double tx_epoch_ms, struct s_txlog *tdp, int len_tdfile, int *iocc, int *socc, double *socc_epoch_ms) {
    struct s_txlog *left, *right, *current;    // current, left and right index of the search

    left = tdp; right = tdp + len_tdfile -1; 

    if (tx_epoch_ms < left->epoch_ms) // tx started before modem occupancy was read
        FATAL("find_occ_from_tdfile: Packet %d tx_epoch_ms is smaller than first occupancy sample time\n", packet_num)

    if (tx_epoch_ms > right->epoch_ms + 100) // tx was done significantly later than last occ sample
        FATAL("find_occ_from_tdfile: Packet %d tx_epoch_ms is over 100ms largert than last occupancy sample time\n", packet_num)

    current = left + (right - left)/2; 
    while (current != left) { // there are more than 2 elements to search
        if (tx_epoch_ms > current->epoch_ms)
            left = current;
        else
            right = current; 
        current = left + (right - left)/2; 
    } // while there are more than 2 elements left to search

    // on exiting the while the left is smaller than tx_epoch_ms, the right can be bigger or equal 
    // so need to check if the right should be used
    if (tx_epoch_ms == right->epoch_ms) 
        current = right; 

    *socc = current->occ; 
    *iocc = interpolate_occ (tx_epoch_ms, current, MIN((current+1), (tdp+len_tdfile+1)));
    *socc_epoch_ms = current->epoch_ms; 

    return;
} // find_occ_from_tdfile

// outputs per carrier stats
void emit_carrier_stat (
    int print_header, 
    struct s_carrier *cp, 
    struct meta_data *mdp, 
    struct frame *fp) {

	while (mdp->packet_num > cp->packet_num) { // step over retransmitted previous packets
        int last_packet_num = cp->packet_num; 
        if (read_carrier_md (0, cp->fp, cp->line, cp, cp->name) == 0) {
            // reached end of file or invalid formatted line
            cp->packet_num = -1;  // so that it does not match the mdp->packet number and participate in the transaction
            break;
        } // if not able to read a line
        else { // do sanity check
            if (last_packet_num > cp->packet_num) { // check that channel meta data file is sorted by packet number
                printf ("Meta data file for carrier %s is not sorted. Check line: %s", cp->name, cp->line);
                my_exit (-1); 
            } // channel meta data file is not sorte by packet number
        } // end of sanity check
	} // while have not reached end of file or gone past the current packet

    // **** WHILE exits on reaching the first packet that matches mdp packet num assuming that the first line has the earliest
    // rx_timestamp, If there are additional retransmitted packets they are ignored and disccarded during next packet read. 
    // This is a bug and needs to be fixed. the code should search for the earliest rather than assuming.

    // if this carrier transmitted this meta data line, then print the carrier stats
	if (cp->packet_num == mdp->packet_num) {
        int last_reported_socc = cp->socc; 

        cp->tx = 1; 
        decode_sendtime (&cp->vx_epoch_ms, &cp->tx_epoch_ms, &cp->socc);
	    if (cp->socc == 31) /* no info */ cp->socc = last_reported_socc; 

        // if tx log exists then overwrite occupancy from tx log file
        if (cp->len_td) { // log file exists
            find_occ_from_tdfile (mdp->packet_num, cp->tx_epoch_ms, cp->tdp, cp->len_td, &(cp->iocc), &(cp->socc), &(cp->socc_epoch_ms));
        }

        cp->t2r = cp->rx_epoch_ms - cp->tx_epoch_ms; 

	    fprintf (lf_fp, "%0.1f, ", cp->vx_epoch_ms - fp->camera_epoch_ms);
	    fprintf (lf_fp, "%0.1f, ", cp->tx_epoch_ms - cp->vx_epoch_ms);
	    fprintf (lf_fp, "%0.1f, ", cp->t2r);
	    fprintf (lf_fp, "%0.1f, ", cp->rx_epoch_ms - fp->camera_epoch_ms);
        fprintf (lf_fp, "%d, ", cp->socc);
        if (cp->len_td) fprintf (lf_fp, "%d, ", cp->iocc); else fprintf (lf_fp, ", "); 
	    if (cp->len_td) fprintf (lf_fp, "%.0lf, ", cp->socc_epoch_ms); else fprintf (lf_fp, ", "); 
	    fprintf (lf_fp, "%.0lf, ", cp->tx_epoch_ms);

    } // if this carrier transmitted this meta data line, then print the carrier stats 

	else {// stay silent except indicate the channel quality through t2r and occ
        cp->tx = 0; 
        if (cp->len_td) {
            // find socc using mdp->tx_epoch_ms since cp->tx_epoch_ms is not avaialble as the channel did not transmit
            find_occ_from_tdfile (mdp->packet_num, mdp->tx_epoch_ms, cp->tdp, cp->len_td, &(cp->iocc), &(cp->socc), &(cp->socc_epoch_ms));
	        fprintf (lf_fp, ", , %0.1f, ,%d ,%d ,%0.lf , , ", cp->t2r, cp->socc, cp->iocc, cp->socc_epoch_ms);
        }
        else
	        fprintf (lf_fp, ", , %0.1f, , , , , , ", cp->t2r);
    }

    return;

} // emit_carrier_stat

// Accumulates stats at the end of every frame. 
// assumes that it is called only at the end of a frame
void update_session_stats (struct frame *p) { 
    float   latency;                        // camera timestamp to the last rx packet latency of the current frame

    // frame numbers start at 1 so increment before doing anything else.
    ssp->frame_count++; 

    // annotation stat
    p->has_annotation = check_annotation (ssp->frame_count); 

    // packet count stats
    update_metric_stats (ssp->pcp, p->packet_count, p->packet_count, MAX_PACKETS_IN_A_FRAME, MIN_PACKETS_IN_A_FRAME);

    // frame latency stats
    latency = p->rx_epoch_ms_latest_packet - p->camera_epoch_ms; 
    if (latency < 0) {
        printf ("Negative transit latency %.1f for frame %u starting at %.0lf\n", latency, ssp->frame_count, p->tx_epoch_ms_1st_packet); 
        my_exit (-1);
    }
    update_metric_stats (ssp->lp, 0, latency, MAX_TRANSIT_LATENCY_OF_A_FRAME, MIN_TRANSIT_LATENCY_OF_A_FRAME);

    // frame byte count stats
    if (p->size <= 0) {
        printf ("Invalid frame size %u for frame %u starting at %.0lf\n", p->size, ssp->frame_count, p->tx_epoch_ms_1st_packet);
        my_exit (-1);
    }
    update_metric_stats (ssp->bcp, p->size, p->size, MAX_BYTES_IN_A_FRAME, MIN_BYTES_IN_A_FRAME);

    // late and incomplete frames stats
    update_metric_stats (ssp->latep, p->late > 0, p->late, MAX_LATE_PACKETS_IN_A_FRAME, MIN_LATE_PACKETS_IN_A_FRAME);
    update_metric_stats (ssp->ip, p->missing > 0, p->missing, MAX_MISSING_PACKETS_IN_A_FRAME, MIN_MISSING_PACKETS_IN_A_FRAME); 
    update_metric_stats (ssp->op, p->out_of_order > 0, p->out_of_order, MAX_OOO_PACKETS_IN_A_FRAME, MIN_OOO_PACKETS_IN_A_FRAME); 
    
    // bit rate stat
    if (p->nm1_bit_rate<=0) {
        printf ("Invalid bit rate %.1f for frame %u starting at %.0lf\n", p->nm1_bit_rate, ssp->frame_count, p->tx_epoch_ms_1st_packet); 
        my_exit (-1); 
    }
    update_metric_stats (ssp->brp, p->nm1_bit_rate < minimum_acceptable_bitrate, p->nm1_bit_rate, MAX_BIT_RATE_OF_A_FRAME, MIN_BIT_RATE_OF_A_FRAME); 
    
    // camera timestamp
    if (ssp->frame_count > 1) // skip first frame because the TS for n-1 frame is undefined 
        update_metric_stats (ssp->ctsp, 1, p->camera_epoch_ms - p->nm1_camera_epoch_ms, 60, 30); 

} // update_session_stats

// Computes the mean/variance of the specified metric. 
void compute_metric_stats (struct stats *p, unsigned count) {
    p->mean /= count;              // compute EX
    p->var /= count;               // compute E[X^2]
    p->var -= p->mean * p->mean;         // E[X^2] - EX^2
} // compute stats

void emit_session_stats (void) {
    char buffer[500];
    compute_metric_stats (ssp->pcp, ssp->frame_count); 
    compute_metric_stats (ssp->lp, ssp->frame_count); 
    compute_metric_stats (ssp->bcp, ssp->frame_count); 
    compute_metric_stats (ssp->ip, ssp->frame_count); 
    compute_metric_stats (ssp->op, ssp->frame_count); 
    compute_metric_stats (ssp->latep, ssp->frame_count); 
    compute_metric_stats (ssp->brp, ssp->frame_count); 
    compute_metric_stats (ssp->ctsp, ssp->frame_count); 
    compute_metric_stats (ssp->c2vp, ssp->pcp->count); 
    compute_metric_stats (ssp->v2tp, ssp->pcp->count); 
    compute_metric_stats (ssp->best_t2rp, ssp->pcp->count); 
    compute_metric_stats (ssp->c2rp, ssp->pcp->count); 

    fprintf (ss_fp, "Total number of frames in the session, %u\n", ssp->frame_count); 
    emit_metric_stats ("Frames with late packets", "Late Packets distribution", ssp->latep, 1, MAX_LATE_PACKETS_IN_A_FRAME, MIN_LATE_PACKETS_IN_A_FRAME);
    emit_metric_stats ("Frame Latency", "Frame Latency", ssp->lp, 0, MAX_TRANSIT_LATENCY_OF_A_FRAME, MIN_TRANSIT_LATENCY_OF_A_FRAME);
    emit_metric_stats ("Frames with missing packets", "Missing Packets distribution",  ssp->ip, 1, MAX_MISSING_PACKETS_IN_A_FRAME, MIN_MISSING_PACKETS_IN_A_FRAME);
    emit_metric_stats ("Frames with out of order packets", "OOO Packets", ssp->op, 1, MAX_OOO_PACKETS_IN_A_FRAME, MIN_OOO_PACKETS_IN_A_FRAME);
    sprintf (buffer, "Frames with bit rate below %.1fMbps", minimum_acceptable_bitrate);
    emit_metric_stats (buffer, "Bit-rate", ssp->brp, 1, MAX_BIT_RATE_OF_A_FRAME, MIN_BIT_RATE_OF_A_FRAME); 
    emit_metric_stats ("Camera time stamp", "Camera time stamp", ssp->ctsp, 1, 60, 30); 
    emit_metric_stats ("C->V latency", "C->V latency", ssp->c2vp, 0, MAX_C2V_LATENCY, MIN_C2V_LATENCY); 
    emit_metric_stats ("V->T latency", "V->T latency", ssp->v2tp, 0, 50, 0); 
    emit_metric_stats ("Best TX->RX latency", "Best TX->RX latency", ssp->best_t2rp, 0, MAX_T2R_LATENCY, MIN_T2R_LATENCY); 
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
int read_md (int skip_header, FILE *fp, struct meta_data *p) {
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
void update_bit_rate (struct frame *fp, struct meta_data *lmdp, struct meta_data *cmdp) {
    double transit_time;

    //bit-rate. For the first frame, bit-rate is approximate bit rate of the. For subsequent frames bit-rate is 
    // the bit-rate of the previous frame
    if (ssp->frame_count==0) {
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

// initializes session stats
void init_session_stats (struct session_stats *p) {
    p->frame_count = 0;
    p->pcp = &(p->pc); init_metric_stats (p->pcp);
    p->lp = &(p->l); init_metric_stats (p->lp);
    p->bcp = &(p->bc); init_metric_stats (p->bcp);
    p->ip = &(p->i); init_metric_stats (p->ip);
    p->op = &(p->o); init_metric_stats (p->op);
    p->latep = &(p->late); init_metric_stats (p->latep);
    p->brp = &(p->br); init_metric_stats (p->brp);
    p->ctsp = &(p->cts); init_metric_stats (p->ctsp);
    p->c2vp = &(p->c2v); init_metric_stats (p->c2vp);
    p->v2tp = &(p->v2t); init_metric_stats (p->v2tp);
    p->c2rp = &(p->c2r); init_metric_stats (p->c2rp);
    p->best_t2rp = &(p->best_t2r); init_metric_stats (p->best_t2rp);
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
