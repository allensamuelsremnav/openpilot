// vq filter*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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
#define     MAX_MD_LINE_SIZE    500 
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

struct meta_data {
    int         packet_num;                 // incrementing number starting from 0
    double      vx_epoch_ms;                // time since epoch in ms
    double      tx_epoch_ms;                // time since epoch in ms
    double      rx_epoch_ms;                // time sicne epoch in ms
    int         modem_occ;                  // 31 if no information avaialble from the mdoem
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

struct frame {
    unsigned    size;                       // size in bytes
    unsigned    late;                       // number of late packets in this frame
    unsigned    missing;                    // number of missing packets in this frames
    unsigned    out_of_order;               // number of out of order packets in this frame
    int         first_packet_num;           // first packet number of the frame
    int         last_packet_num;            // last packet number of the frame
    unsigned    packet_count;               // number of packets in this frame
    double      tx_epoch_ms_1st_packet;     // tx timestamp of the first packet of the frame
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
    int         repeat_count;               // number of times this frame caused a previous frame to be repeated
    int         skip_count;                 // if 1 then this frame will cause previous frame to be skipped
    float       c2d_frames;                 // camera to display latency in units of frame time
}; 

struct stats {
    unsigned    count;
    double      mean; 
    double      var;
    double      min;
    double      max;
    double      distr[NUMBER_OF_BINS]; 
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
        char line[MAX_MD_LINE_SIZE];        // line to read the carrier data in 
        char *lp;                           // pointer to line
        int tx;                             // set to 1 if this carrier tranmitted the packet being considered
        int packet_num;                     // packet number read from this line of the carrier meta_data finle 
        double vx_epoch_ms; 
        double tx_epoch_ms; 
        double rx_epoch_ms;
        float last_rx_m_tx;                 // tx-rx of the last transmitted packet
        int modem_occ;                      // modem buffer occupancy. 31 if not available from the modem
};

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
void decode_sendtime (double *vx_epoch_ms, double *tx_epoch_ms, int *modem_occ);

// outputs per carrier stats
void emit_carrier_stat (int print_header, struct s_carrier *cp, FILE *c_fp, char *cname, struct meta_data *mdp, struct frame *fp);

void emit_packet_header (FILE *fp);

void emit_frame_header (FILE *fp);

void skip_combined_md_file_header (FILE *fp);

void skip_carrier_md_file_header (FILE *fp, char *cname);

void skip_combined_md_file_header (FILE *fp);

/*
void red (FILE *fp) {
  fprintf(fp, "\033[1;31m");
}

void yellow () {
  printf("\033[1;33m");
}

void reset (FILE *fp) {
  fprintf(fp, "\033[0m");
}
*/

// globals
char    *usage = "Usage: vqfilter -l <ddd> -b <dd.d> -md|mdc <input prefex> [-j <dd>] [-v] [-ns1|ns2] [-a <file name>] [-help] -out <output prefix> ";
FILE    *md_fp = NULL;                          // meta data file name
FILE    *an_fp = NULL;                          // annotation file name
FILE    *fs_fp = NULL;                          // frame statistics output file
FILE    *ss_fp = NULL;                          // statistics output file name
FILE    *lf_fp = NULL;                          // late frame output file name
FILE    *c0_fp = NULL;                          // carrier 0 meta data file
FILE    *c1_fp = NULL;                          // carrier 0 meta data file
FILE    *c2_fp = NULL;                          // carrier 0 meta data file
char    annotation_file_name[200];              // annotation file name
struct  session_stats sstat, *ssp=&sstat;       // session stats 
float   minimum_acceptable_bitrate = -1;        // used for stats generation only
unsigned maximum_acceptable_c2rx_latency = -1;  // packets considered late if the latency exceeds this 
struct  meta_data md[MD_BUFFER_SIZE];           // buffer to store meta data lines*/
int     md_index;                               // current meta data buffer pointer
unsigned anlist[MAX_NUM_OF_ANNOTATIONS][2];     // mmanual annotations
int     len_anlist=0;                           // length of the annotation list
int     have_carrier_metadata = 0;              // set to 1 if per carrier meta data is available
int     new_sendertime_format =0;               // set to 1 if using embedded sender time format
int     verbose;                                // 1 or 2 for the new sender time formats
int     rx_jitter_buffer_ms = 10;               // buffer to mitigate skip/repeats due to frame arrival jitter

int main (int argc, char* argv[]) {
    unsigned    waiting_for_first_frame;        // set to 1 till first clean frame start encountered
    struct      meta_data *cmdp, *lmdp;         // last and current meta data lines
    struct      frame cf, *cfp = &cf;          // current frame

    //  read command line arguments
    while (*++argv != NULL) {
        char buffer[100], *bp=buffer; 

        // deduped combined meta data file only
        if (strcmp (*argv, "-md") == MATCH) {
            sprintf (bp, "%s.csv", *++argv); 
            if ((md_fp = fopen (bp, "r")) == NULL) {
                printf ("Could not open meta data file %s\n", bp);
                print_usage (); 
                exit (-1); 
            }

        // deduped combined meta data file and per carrier meta data avaiable
        } else if (strcmp (*argv, "-mdc") == MATCH) {
            have_carrier_metadata = 1;
            sprintf (bp, "%s.csv", *++argv); 
            if ((md_fp = fopen (bp, "r")) == NULL) {
                printf ("Could not open meta data file %s\n", bp);
                print_usage (); 
                exit (-1); 
            }
            // carrier 0 meta data files
            sprintf (bp, "%s_ch0.csv", *argv); 
            if ((c0_fp = fopen (bp, "r")) == NULL) {
                printf ("Could not open carrier 0 meta data file %s\n", bp );
                print_usage (); 
                exit (-1); 
            }
            // carrier 1 meta data files
            sprintf (bp, "%s_ch1.csv", *argv); 
            if ((c1_fp = fopen (bp, "r")) == NULL) {
                printf ("Could not open carrier 1 meta data file %s\n", bp );
                print_usage (); 
                exit (-1); 
            }
            // carrier 2 meta data files
            sprintf (bp, "%s_ch2.csv", *argv); 
            if ((c2_fp = fopen (bp, "r")) == NULL) {
                printf ("Could not open carrier 0 meta data file %s\n", bp );
                print_usage (); 
                exit (-1); 
            }

        // new sender time format
        } else if (strcmp (*argv, "-ns1") == MATCH) {
            new_sendertime_format = 1; 

        // new sender time format
        } else if (strcmp (*argv, "-ns2") == MATCH) {
            new_sendertime_format = 2; 

        // verbose
        } else if (strcmp (*argv, "-v") == MATCH) {
            verbose = 1; 

        // annotation file
        } else if (strcmp (*argv, "-a") == MATCH) {
            strcpy(annotation_file_name, *++argv);
            if ((an_fp = fopen (annotation_file_name, "r")) == NULL) {
                printf ("Could not open annotation file %s\n", annotation_file_name);
                print_usage (); 
                exit (-1); 
            } else // read the annotation file
                read_annotation_file (); 

        // frame and session stat output files
        } else if (strcmp (*argv, "-out") == MATCH) {

            // frame stats output file
            sprintf (bp, "%s_frame_stats.csv",*++argv); 
            if ((fs_fp = fopen (bp, "w")) == NULL) {
                printf ("Could not open frame stats output file %s\n", bp);
                print_usage (); 
                exit (-1); 
            }
            
            // session stats output file
            sprintf (bp, "%s_session_stats.csv",*argv); 
            if ((ss_fp = fopen (bp, "w")) == NULL) {
                printf ("Could not open statistics output file %s\n", bp);
                print_usage (); 
                exit (-1); 
            }

            // late frames output file
            sprintf (bp, "%s_late_frames.csv",*argv); 
            if ((lf_fp = fopen (bp, "w")) == NULL) {
                printf ("Could not open late frame output file %s\n", bp);
                print_usage (); 
                exit (-1); 
            }

        // maximum_acceptable_c2rx_latency
        } else if (strcmp (*argv, "-l") == MATCH) {
            if (sscanf (*++argv, "%u", &maximum_acceptable_c2rx_latency) != 1) {
                printf ("missing maximum acceptable camera->rx latency specification\n");
                print_usage (); 
                exit (-1); 
            }

        // minimum_acceptable_bitrate
        } else if (strcmp (*argv, "-b") == MATCH) {
            if (sscanf (*++argv, "%f", &minimum_acceptable_bitrate) != 1) {
                printf ("missing minimum acceptable bit rate specification\n");
                printf ("%s\n", usage); 
                exit (-1); 
            }
        
        // rx arrival jitter buffer
        } else if (strcmp (*argv, "-j") == MATCH) {
            if (sscanf (*++argv, "%d", &rx_jitter_buffer_ms) != 1) {
                printf ("missing frame arrival jitter buffer length specification \n");
                printf ("%s\n", usage); 
                exit (-1); 
            }

        // help/usage
        } else if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            printf ("%s\n", usage);
            exit (-1); 

        // invalid argument
        } else {
            printf ("Invalid argument %s\n", *argv);
            print_usage (); 
            exit (-1); 
        }
    } // while there are more arguments to process

    // check if any missing arguments
    if (md_fp == NULL)
        printf ("Metadata file was not specified\n");
    if ((fs_fp == NULL) || (ss_fp == NULL) || (lf_fp == NULL))
        printf ("output file prefix was not speciied\n");
    if (minimum_acceptable_bitrate <0)
        printf ("Invalid or missing minimum bit rate specification\n");
    if (maximum_acceptable_c2rx_latency < 0) 
        printf ("Invalid or missing maximumum camera to rx latency specificaiton \n"); 
    if (md_fp==NULL || fs_fp==NULL || ss_fp==NULL || lf_fp==NULL || (minimum_acceptable_bitrate < 0) || 
        (maximum_acceptable_c2rx_latency < 0)) {
        printf ("%s\n", usage);
        exit (-1);
    }

    // control initialization
    waiting_for_first_frame = 1; 

    // frame structures intializetion
    lmdp = md; 
    lmdp->rx_epoch_ms = -1; // to out-of-order calculations for the first frame are correct.

    cmdp = lmdp+1; 
    md_index = 1; 

    // sesstion stat structures intialization
    init_session_stats (ssp);

    // skip/print headers
    skip_combined_md_file_header (md_fp);
    skip_carrier_md_file_header (c0_fp, "C0");
    skip_carrier_md_file_header (c1_fp, "C0");
    skip_carrier_md_file_header (c2_fp, "C0");
    emit_frame_header (fs_fp);
    emit_packet_header (lf_fp);

    // while there are more lines to read from the meta data file
    while (read_md (0, md_fp, cmdp) != 0) { 

        //  if first frame then skip till a clean frame start 
        if (waiting_for_first_frame) {
            if (cmdp->frame_start) {
                // found a clean start
                init_frame (cfp, lmdp, cmdp, 1); // first frame set to 1
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
        // lmdp has the last packet of the current frame. cmdp first of the new frame
        else {

            // first finish processing the last frame

            // check if the last frame ended with packets missing
            if (!lmdp->frame_end) // frame ended abruptly
                if (cmdp->frame_start) // next frame started cleanly so all the missing packets belong to this frame
                    cfp->missing += cmdp->packet_num - (lmdp->packet_num+1);
                else 
                    cfp->missing += 1; // not the correct number of missing packets but can't do better

            // check if the last frame arrived too late and would have caused repeats at the display
		    if (ssp->frame_count < 1) { // we just reached the end of first frame
		        // set up display clock
                // by definition the first frame does not have repeat/skip
		        cfp->cdisplay_epoch_ms = cfp->rx_epoch_ms_latest_packet + rx_jitter_buffer_ms;
		        cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms;
		        cfp->repeat_count = 0; // nothing repeated yet
                cfp->skip_count = 0; 
		    } // set up display clock

		    else { // check if this frame caused a repeat or skip
                if (cfp->rx_epoch_ms_latest_packet < cfp->cdisplay_epoch_ms) {
                    // multiple frames arriving within in the current display period
                    // do not advance the dipslay clock but reset repeat_count since late frames can
                    // advance the display clock by arbitrarly large amount and next frame may arrive 
                    // earlier than this simulated display
                    cfp->repeat_count = 0;
                    cfp->skip_count = 1; 
                } // of frame arrived early and will cause previous frame(s) not yet displayed to be skipped
                /* no need to separate timely arrival case from late
                else if (cfp->rx_epoch_ms_latest_packet < cfp->ndisplay_epoch_ms) {
                    // frame arrived in time. Nothing will need to be repeated
                    // advance display clock
                    cfp->repeat_count = 0; 
                    cfp->skip_count = 0; 
                    cfp->cdisplay_epoch_ms += cfp->display_period_ms; 
                    cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms; 
                } 
                */
                else {
                    // frame arrived in time or late. Calculate number of repeats
                    // advance display clock
                    double delta = cfp->rx_epoch_ms_latest_packet - cfp->ndisplay_epoch_ms; 
                    cfp->repeat_count = ceil (delta/cfp->display_period_ms); 
                    cfp-> skip_count = 0; 
                    cfp->cdisplay_epoch_ms += (cfp->repeat_count+1) * cfp->display_period_ms; 
                    cfp->ndisplay_epoch_ms = cfp->cdisplay_epoch_ms + cfp->display_period_ms;
                } // frame arrived in time or late
		    } // check if the last frame that arrived caused repeats
                    
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
            update_session_packet_stats (cfp, cmdp);
        } // start of a new frame

        md_index = (md_index + 1) % MD_BUFFER_SIZE; 
        lmdp = cmdp;
        cmdp = &md[md_index]; 
    } // while there are more lines to be read from the meta data file

    if (waiting_for_first_frame) { // something horribly wrong
        printf ("ERROR: no frames found in this metadata file\n");
        exit (-1); 
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

    exit (0); 
} // end of main

// returns 1 if successfully read the annotation file
int read_annotation_file (void) {
    char    line[100]; 

    // skip header
    if (fgets (line, 100, an_fp) == NULL) {
        printf ("Empty annotation file\n");
        exit (-1); 
    }

    // read annotation line
    while (fgets (line, 100, an_fp) != NULL) {
        if (sscanf (line, "%u, %u", &anlist[len_anlist][0], &anlist[len_anlist][1]) == 2) {
            if (anlist[len_anlist][1] < anlist[len_anlist][0]) {
                printf ("Invalid annotation. Start frame is bigger than then End\n");
                exit (-1);
            }
        } else if (sscanf (line, "%u", &anlist[len_anlist][0]) == 1)
            anlist[len_anlist][1] = anlist[len_anlist][0];
        else {
            printf ("Invalid annnotation format %s\n", line); 
            exit(-1); 
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
    printf ("%s\n", usage);
    printf ("\t-l <ddd> is the maximumum acceptable camera to rx latency in ms\n");
    printf ("\t-b <dd.d> is the minimum acceptable bit rate in Mbps\n");
    printf ("\t-md <file name> is the metadata csv file to be read. see VQ filter POR for format\n");
    printf ("\t-pre <file name prefix> is the output file prefix used to create frame and session stat output files\n");
}

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
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? 33.33 : fp->frame_rate==HZ_15? 66.66 : fp->frame_rate==HZ_10? 100 : 200;
    } else {
        // abrupt start of this frame AND abrupt end of the previous frame. Now we can't tell how many packets each frame lost
        // have assigned one missing packet to previous frame. So will assign the remaining packets to this frame. 
        fp->missing = cmdp->packet_num - (lmdp->packet_num +1) -1; 
        fp->camera_epoch_ms += fp->frame_rate==HZ_30? 33.33 : fp->frame_rate==HZ_15? 66.66 : fp->frame_rate==HZ_10? 100 : 200;
    }
    fp->out_of_order = /* first_frame? 0: */ cmdp->rx_epoch_ms < lmdp->rx_epoch_ms; 
    fp->first_packet_num = fp->last_packet_num = cmdp->packet_num;
    fp->tx_epoch_ms_1st_packet = cmdp->tx_epoch_ms;
    fp->rx_epoch_ms_last_packet = fp->rx_epoch_ms_earliest_packet = fp->rx_epoch_ms_latest_packet = cmdp->rx_epoch_ms;
    fp->latest_retx = cmdp->retx; 
    fp->frame_rate = cmdp->frame_rate; 
	fp->display_period_ms = 1000 / (double) ((fp->frame_rate==HZ_30? 30 : fp->frame_rate==HZ_15 ? 15 : fp->frame_rate==HZ_10? 10 : 5));
    fp->frame_resolution = cmdp->frame_resolution;
    fp->late = cmdp->rx_epoch_ms > (fp->camera_epoch_ms + maximum_acceptable_c2rx_latency);
    fp->packet_count = fp->latest_packet_count = 1; 
    fp->latest_packet_num = cmdp->packet_num; 

} // end of init_frame

void emit_frame_header (FILE *fp) {

    // command line arguments
    // red (fp);
    fprintf (fp, "Command line arguments; ");
    fprintf (fp, "-l %d ", maximum_acceptable_c2rx_latency);
    fprintf (fp, "-ns%d ", new_sendertime_format);
    fprintf (fp, "-b %0.1f ", minimum_acceptable_bitrate);
    fprintf (fp, "-j %d ", rx_jitter_buffer_ms);
    fprintf (fp, "-a %s ", annotation_file_name);
    fprintf (fp, "\n"); 
    // reset (fp); 

    fprintf (fp, "F#, ");
    fprintf (fp, "Late, ");
    fprintf (fp, "Miss, ");
    fprintf (fp, "Mbps, ");
    fprintf (fp, "anno, ");
    fprintf (fp, "1st Pkt, ");
    fprintf (fp, "SzP, ");
    fprintf (fp, "SzB, ");
    fprintf (fp, "LPC, ");
    fprintf (fp, "LPN, ");
    fprintf (fp, "retx, ");
    fprintf (fp, "c2t, ");
    fprintf (fp, "t2r, ");
    fprintf (fp, "c2r, ");
    fprintf (fp, "ooo, ");
    fprintf (fp, "Rpt, ");
    fprintf (fp, "skp, ");
    fprintf (fp, "c2d, ");
    fprintf (fp, "CTS, ");
    fprintf (fp, "1st TX_TS, ");
    fprintf (fp, "last Rx_TS, ");
    fprintf (fp, "earliest Rx_TS, ");
    fprintf (fp, "latest Rx_TS, ");
    fprintf (fp, "cdsp_TS, ");
    fprintf (fp, "ndsp_TS, ");
    
    fprintf (fp, "\n"); 

} // emit_frame_header

void emit_packet_header (FILE *lf_fp) {

        fprintf (lf_fp, "F#, ");
        fprintf (lf_fp, "P#, ");
        fprintf (lf_fp, "Late, ");
	    fprintf (lf_fp, "Miss, ");
	    fprintf (lf_fp, "Mbps, ");
	    fprintf (lf_fp, "SzP, ");
	    fprintf (lf_fp, "SzB, ");
	    fprintf (lf_fp, "LPN, ");
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
        fprintf (lf_fp, "Pc2R, ");
        
        // per carrier meta data
        fprintf (lf_fp, "C0: c2v, v2t, t2r, c2r, occ, tx_TS, "); 
        fprintf (lf_fp, "C1: c2v, v2t, t2r, c2r, occ, tx_TS, "); 
        fprintf (lf_fp, "C2: c2v, v2t, t2r, c2r, occ, tx_TS, "); 

    fprintf (lf_fp, "\n");
    return;
} // emit_packet_header

// assumes called at the end of a frame after frame and session stats have been updated
void emit_frame_stats (int print_header, struct frame *p, int last) {     // last is set 1 for the last frame of the se//ssion 
    static struct s_carrier c0, c1, c2; 
    struct s_carrier *c0p=&c0, *c1p=&c1, *c2p=&c2; 
    
    // initialization
    if (ssp->frame_count == 1) {
        c0p->packet_num = -1; // have not read any lines yet
        c0p->last_rx_m_tx = 30; // default value before the first packet is transmiited by this carrier
        c1p->packet_num = -1; // have not read any lines yet
        c1p->last_rx_m_tx = 30; // default value before the first packet is transmiited by this carrier
        c2p->packet_num = -1; // have not read any lines yet
        c2p->last_rx_m_tx = 30; // default value before the first packet is transmiited by this carrier
    }

    if (verbose) {
        if ((ssp->frame_count % 1000) == 0)
            printf ("at frame %d\n", ssp->frame_count); 
    }

    // frame stats 
    fprintf (fs_fp, "%u, ", ssp->frame_count);
    fprintf (fs_fp, "%u, ", p->late);
    fprintf (fs_fp, "%u, ", p->missing);
    fprintf (fs_fp, "%.1f, ", p->nm1_bit_rate);
    fprintf (fs_fp, "%u, ", p->has_annotation);
    fprintf (fs_fp, "%u, ", p->first_packet_num);
    fprintf (fs_fp, "%u, ", p->packet_count);
    fprintf (fs_fp, "%u, ", p->size);
    fprintf (fs_fp, "%u, ", p->latest_packet_count);
    fprintf (fs_fp, "%u, ", p->latest_packet_num);
    fprintf (fs_fp, "%u, ", p->latest_retx);
    fprintf (fs_fp, "%.1f, ", p->tx_epoch_ms_1st_packet - p->camera_epoch_ms);
    fprintf (fs_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->tx_epoch_ms_1st_packet);
    fprintf (fs_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->camera_epoch_ms);
    fprintf (fs_fp, "%u, ", p->out_of_order);
    fprintf (fs_fp, "%u, ", p->repeat_count);
    fprintf (fs_fp, "%u, ", p->skip_count);
    fprintf (fs_fp, "%.1f, ", p->c2d_frames);
    fprintf (fs_fp, "%.0lf, ", p->camera_epoch_ms);

    fprintf (fs_fp, "%.0lf, ", p->tx_epoch_ms_1st_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_last_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_earliest_packet);
    fprintf (fs_fp, "%.0lf, ", p->rx_epoch_ms_latest_packet);
    fprintf (fs_fp, "%.0lf, ", p->cdisplay_epoch_ms);
    fprintf (fs_fp, "%.0lf, ", p->ndisplay_epoch_ms);
    fprintf (fs_fp, "\n");

    // packet stats

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

        // Frame metadata
        fprintf (lf_fp, "%u, ", ssp->frame_count);
        fprintf (lf_fp, "%u, ", mdp->packet_num);
        fprintf (lf_fp, "%u, ", p->late);
	    fprintf (lf_fp, "%u, ", p->missing);
	    fprintf (lf_fp, "%.1f, ", p->nm1_bit_rate);
	    fprintf (lf_fp, "%u, ", p->packet_count);
	    fprintf (lf_fp, "%u, ", p->size);
	    fprintf (lf_fp, "%u, ", p->latest_packet_num);
	    fprintf (lf_fp, "%.1f, ", p->tx_epoch_ms_1st_packet - p->camera_epoch_ms);
	    fprintf (lf_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->tx_epoch_ms_1st_packet);
	    fprintf (lf_fp, "%.1f, ", p->rx_epoch_ms_latest_packet - p->camera_epoch_ms);
	    fprintf (lf_fp, "%u, ", p->repeat_count);
	    fprintf (lf_fp, "%u, ", p->skip_count);
	    fprintf (lf_fp, "%.1f, ", p->c2d_frames);
        fprintf (lf_fp, "%.0lf, ", p->camera_epoch_ms);

        // Delivered packet meta data
        fprintf (lf_fp, "%u, ", mdp->retx);
        fprintf (lf_fp, "%u, ", mdp->ch);
        fprintf (lf_fp, "%.0lf, ", mdp->tx_epoch_ms);
        fprintf (lf_fp, "%.0lf, ", mdp->rx_epoch_ms);
        fprintf (lf_fp, "%.1f, ", mdp->rx_epoch_ms - p->camera_epoch_ms);

        // Per carrier meta data
        if (have_carrier_metadata) {

	        emit_carrier_stat (print_header, c0p, c0_fp, "c0", mdp, p);
	        emit_carrier_stat (print_header, c1p, c1_fp, "c1", mdp, p);
	        emit_carrier_stat (print_header, c2p, c2_fp, "c2", mdp, p);
	
		    // compute difference in the tx times of the channels attempting this packet
		    double latest_tx, earliest_tx;
		    if (c0p->tx==0) { // c0 is not transmitting
		       latest_tx = MAX(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
		       earliest_tx = MIN(c1p->tx_epoch_ms, c2p->tx_epoch_ms);
		    } // c0 is not transmitting
		    else if (c1p->tx==0) { // c1 is not transmitting
		       latest_tx = MAX(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
		       earliest_tx = MIN(c0p->tx_epoch_ms, c2p->tx_epoch_ms);
		    } // c1 is not transmitting
		    else { // c2 is not transmitting
		        latest_tx = MAX(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
		        earliest_tx = MIN(c0p->tx_epoch_ms, c1p->tx_epoch_ms);
		    } // c2 is not transmitting
		    fprintf (lf_fp, "%0.1f, ", latest_tx-earliest_tx); 
	
	        // fast channel availabiliyt and efficiency
	        float fastest_tx_to_rx = MIN(c0p->last_rx_m_tx, MIN(c1p->last_rx_m_tx, c2p->last_rx_m_tx));
	        float c2v_latency = MAX(c0p->tx*(c0p->vx_epoch_ms - p->camera_epoch_ms), 
	            MAX(c1p->tx*(c1p->vx_epoch_ms - p->camera_epoch_ms), c2p->tx*(c2p->vx_epoch_ms - p->camera_epoch_ms))); 
	        fprintf (lf_fp, "%0.1f, ", fastest_tx_to_rx);
	        fprintf (lf_fp, "%0.1f, ", (mdp->rx_epoch_ms-p->camera_epoch_ms) - fastest_tx_to_rx - c2v_latency); 
	
	        // update session packet stats (wrong place for this code. 
	        // update_metric_stats (ssp->c2vp, 0, c2v_latency, MAX_C2V_LATENCY, MIN_C2V_LATENCY);
	        update_metric_stats (ssp->best_t2rp, 0, fastest_tx_to_rx, MAX_T2R_LATENCY, MIN_T2R_LATENCY);

        } // have carrier meta data 

        fprintf (lf_fp, "\n");
        
    } // for every packet in the frame
            
    return; 
} // end of emit_frame_stats

void skip_carrier_md_file_header (FILE *fp, char *cname) {
    char line[500];
	if (fgets (line, MAX_MD_LINE_SIZE, fp) == NULL) {
	    printf ("Empty %s csv file\n", cname);
	    exit(-1);
	}
    return;
} // skip_carrier_md_header 

int read_carrier_md (int skip_header, FILE *fp, char *line, struct s_carrier *cp, char *cname) {

	if (fgets(line, MAX_MD_LINE_SIZE, fp) != NULL) {
	    if (sscanf (line, "%u, %lf, %lf,", &cp->packet_num, &cp->vx_epoch_ms, &cp->rx_epoch_ms) !=3) {
	        printf ("could not find all the fields in the %s meta data file %s\n", cname, line); 
	        exit (-1);
	    } // if scan failed
        else // scan passed
            return 1;
    } // fgets got a line
    else // reached end of file
        return 0;
} // read_carrier_md

// outputs per carrier stats
void emit_carrier_stat (int print_header, struct s_carrier *cp, FILE *c_fp, char *cname, struct meta_data *mdp, struct frame *fp) {

	while (mdp->packet_num > cp->packet_num) {
        if (read_carrier_md (0, c_fp, cp->line, cp, cname) == 0)
            // reached end of file
            break;
	} // while have not reached and gone past the current packet

    // if this carrier transmitted this meta data line, then print the carrier stats
	if (cp->packet_num == mdp->packet_num) {
        cp->tx = 1; 
        decode_sendtime (&cp->vx_epoch_ms, &cp->tx_epoch_ms, &cp->modem_occ);
	    fprintf (lf_fp, "%0.1f, ", cp->vx_epoch_ms - fp->camera_epoch_ms);
	    fprintf (lf_fp, "%0.1f, ", cp->tx_epoch_ms - cp->vx_epoch_ms);
	    fprintf (lf_fp, "%0.1f, ", cp->rx_epoch_ms - cp->tx_epoch_ms);
	    fprintf (lf_fp, "%0.1f, ", cp->rx_epoch_ms - fp->camera_epoch_ms);
	    if (cp->modem_occ == 31) /* no info */ fprintf (lf_fp, ", "); else fprintf (lf_fp, "%d, ", cp->modem_occ);
	    fprintf (lf_fp, "%.0lf, ", cp->tx_epoch_ms);

        cp->last_rx_m_tx = cp->rx_epoch_ms - cp->tx_epoch_ms; 

    } // if this carrier transmitted this meta data line, then print the carrier stats 
	else {// stay silent except indicate the channel quality through t2r
        cp->tx = 0; 
	    fprintf (lf_fp, ", , %0.1f, , , , ", cp->last_rx_m_tx);
    }

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
        exit (-1);
    }
    update_metric_stats (ssp->lp, 0, latency, MAX_TRANSIT_LATENCY_OF_A_FRAME, MIN_TRANSIT_LATENCY_OF_A_FRAME);

    // frame byte count stats
    if (p->size <= 0) {
        printf ("Invalid frame size %u for frame %u starting at %.0lf\n", p->size, ssp->frame_count, p->tx_epoch_ms_1st_packet);
        exit (-1);
    }
    update_metric_stats (ssp->bcp, p->size, p->size, MAX_BYTES_IN_A_FRAME, MIN_BYTES_IN_A_FRAME);

    // late and incomplete frames stats
    update_metric_stats (ssp->latep, p->late > 0, p->late, MAX_LATE_PACKETS_IN_A_FRAME, MIN_LATE_PACKETS_IN_A_FRAME);
    update_metric_stats (ssp->ip, p->missing > 0, p->missing, MAX_MISSING_PACKETS_IN_A_FRAME, MIN_MISSING_PACKETS_IN_A_FRAME); 
    update_metric_stats (ssp->op, p->out_of_order > 0, p->out_of_order, MAX_OOO_PACKETS_IN_A_FRAME, MIN_OOO_PACKETS_IN_A_FRAME); 
    
    // bit rate stat
    if (p->nm1_bit_rate<=0) {
        printf ("Invalid bit rate %.1f for frame %u starting at %.0lf\n", p->nm1_bit_rate, ssp->frame_count, p->tx_epoch_ms_1st_packet); 
        exit (-1); 
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
        fprintf (ss_fp, ", %.1f-%.1f", index*bin_size, (index+1)*bin_size);
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
    if (fgets (line, MAX_MD_LINE_SIZE, fp) == NULL) {
	    printf ("Empty combined csv file\n");
	    exit(-1);
    }
    return;
} // skip_combined_md_file_header

// reads next line of the meta data file. returns 0 if reached end of file
int read_md (int skip_header, FILE *fp, struct meta_data *p) {
    char    mdline[MAX_MD_LINE_SIZE], *mdlp = mdline; 

    // read next line
    if (fgets (mdlp, MAX_MD_LINE_SIZE, md_fp) != NULL) {

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
            exit(1);
        } // scan did not succeed

        else { // successful scan
            decode_sendtime (&p->vx_epoch_ms, &p->tx_epoch_ms, &p->modem_occ);
            return 1;
        } // successful scan

    } // if were able to read a line from the file
    
    // get here at the end of the file
    return 0;
} // end of read_md

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (double *vx_epoch_ms, double *tx_epoch_ms, int *modem_occ) {
            double real_vx_epoch_ms;
            unsigned tx_minus_vx; 
            
            // calculate tx-vx and modem occupancy
            if (new_sendertime_format == 1) {
                real_vx_epoch_ms = /* starts at bit 8 */ trunc (*vx_epoch_ms/256);
                *modem_occ = 31; // not available
                tx_minus_vx =  /* lower 8 bits */ *vx_epoch_ms - real_vx_epoch_ms*256;
            } // format 1:only tx-vx available
            else if (new_sendertime_format == 2) {
                real_vx_epoch_ms = /*starts at bit 9 */ trunc (*vx_epoch_ms/512);
                *modem_occ = /* bits 8:4 */ trunc ((*vx_epoch_ms - real_vx_epoch_ms*512)/16); 
                tx_minus_vx =  /* bits 3:0 */ (*vx_epoch_ms - real_vx_epoch_ms*512) - (*modem_occ *16); 
            } // format 2: tx-vx and modem occ available
            else {
                *modem_occ = 31; // not available
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
        exit (-1); 
    }
    p->count += count;
    p->mean += value;                       // storing sum (X) till the end
    p->var += value * value;                // storing sum (X^2) till the end
    p->max = MAX(p->max, value);
    p->min = p->min==0?                     // first time, so can't do min
        value : MIN(p->min, value); 
    index = MIN((unsigned) value / bin_size, NUMBER_OF_BINS-1);
    p->distr[index]++;

    return;

} // end of update stats
