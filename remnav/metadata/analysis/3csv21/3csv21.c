#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define MATCH 0
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define HZ_30   0
#define HZ_15   1
#define HZ_10   2
#define HZ_5    3
#define RES_HD  0
#define RES_SD  1
#define MAX_MD_LINE_SIZE 500 
#define TX_BUFFER_SIZE (20*60*1000)
#define MD_BUFFER_SIZE (20*60*1000)
#define MAX_TD_LINE_SIZE 1000

struct s_txlog {
// uplink_queue. ch: 2, timestamp: 1672344732193, queue_size: 23, elapsed_time_since_last_queue_update: 29, actual_rate: 1545, stop_sending_flag: 0, zeroUplinkQueue_flag: 0, lateFlag: 1
    int channel;                    // channel number 0, 1 or 2
    double epoch_ms;                // time modem occ was sampled
    int occ;                        // occupancy 0-30
    int time_since_last_update;     // of occupancy for the same channel
    int actual_rate;
};

struct s_carrier {
    int packet_num;                 // packet number read from this line of the carrier meta_data finle 
    double vx_epoch_ms; 
    double tx_epoch_ms;
    double rx_epoch_ms;
    int packet_len;
    int frame_start;
    int frame_num;
    int frame_rate;
    int frame_res;
    int frame_end;
    double camera_epoch_ms;
    int retx;
    int check_packet_num;

    int socc;                           // sampled occupancy either from the tx log or the rx metadata file
    int iocc;                           // interpolated occupancy from tx log
    double socc_epoch_ms;               // time when the occupacny for this packet was sampled

//     struct s_txlog *tdp;                 // pointer to the td array
//     int len_td;                         // lenth of the tx log array. 0 implies tx log was not specified
    int match;                          // set to 1 if this channel matches the packet num to be transmitted
};

// globals
struct s_txlog *td0 = NULL, *td1 = NULL, *td2 = NULL;  // transmit log file stored in this array
int len_td0 = 0, len_td1 = 0, len_td2 = 0; // len of the transmit data logfile

struct s_carrier *c0_md = NULL, *c1_md = NULL, *c2_md = NULL; 
int len_md0 = 0, len_md1 = 0, len_md2 = 0;
 
int silent = 0;                        // suppresses warnings if set to a 1
FILE *warn_fp = NULL;                  // combined output file 

#define		FATAL(STR, ARG) {printf (STR, ARG); my_exit(-1);}
#define		FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}

// free up storage before exiting
int my_exit (int n) {

    if (td0 != NULL) free (td0); 
    if (td1 != NULL) free (td1); 
    if (td2 != NULL) free (td2); 
    exit (n);

} // my_exit

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (int format, double *vx_epoch_ms, double *tx_epoch_ms, int *occ) {
            double real_vx_epoch_ms;
            unsigned tx_minus_vx; 
            
            // calculate tx-vx and modem occupancy
            if (format == 1) {
                real_vx_epoch_ms = /* starts at bit 8 */ trunc (*vx_epoch_ms/256);
                *occ = 31; // not available
                tx_minus_vx =  /* lower 8 bits */ *vx_epoch_ms - real_vx_epoch_ms*256;
            } // format 1:only tx-vx available
            else if (format == 2) {
                real_vx_epoch_ms = /*starts at bit 9 */ trunc (*vx_epoch_ms/512);
                *occ = /* bits 8:4 */ trunc ((*vx_epoch_ms - real_vx_epoch_ms*512)/16); 
                tx_minus_vx =  /* bits 3:0 */ (*vx_epoch_ms - real_vx_epoch_ms*512) - (*occ *16); 
            } // format 2: tx-vx and modem occ available
            else {
                *occ = 31; // not available
                tx_minus_vx = 0; 
            } // format 0: tx-vx and modem occ not available
                
            *tx_epoch_ms = real_vx_epoch_ms + tx_minus_vx; 
            *vx_epoch_ms = real_vx_epoch_ms; 

            return;
} // end of decode_sendtime

FILE *open_file (char *file_namep, char *modep) {
    FILE *fp;
    
    if ((fp = fopen (file_namep, modep)) == NULL)
        FATAL ("Could not open file %s\n", file_namep)
    
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
void find_occ_from_td (int packet_num, double tx_epoch_ms, struct s_txlog *tdp, int len_tdfile, int *iocc, int *socc, double *socc_epoch_ms) {
    struct s_txlog *left, *right, *current;    // current, left and right index of the search

    left = tdp; right = tdp + len_tdfile -1; 

    if (tx_epoch_ms < left->epoch_ms) // tx started before modem occupancy was read
        FATAL("find_occ_from_td: Packet %d tx_epoch_ms is smaller than first occupancy sample time\n", packet_num)

    if (tx_epoch_ms > right->epoch_ms + 100) // tx was done significantly later than last occ sample
        FATAL("find_occ_from_td: Packet %d tx_epoch_ms is over 100ms largert than last occupancy sample time\n", packet_num)

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
} // find_occ_from_td

// reads and parses a meta data line from the specified file. returns 0 if end of file reached
// tx log file if exists must be read prior to reading the carrier meta data files so that
// occupancy can be correctly initialized 
int read_md_line (int read_header, FILE *fp, struct s_txlog *tdp, int len_td, struct s_carrier *mdp) {

    char mdline[MAX_MD_LINE_SIZE], *mdlinep = mdline; 

    if (fgets (mdline, MAX_MD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 0;

    if (read_header)
        return 1;
    
    // parse the line
    // packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolut, ion	 frame_end	 camera_timestamp	 retx	 chPacketNum
    if (sscanf (mdlinep, "%d, %lf, %lf, %d, %d, %d, %d, %d, %d, %lf, %d, %d", 
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
        &mdp->check_packet_num) !=12)
        FATAL ("could not find all the fields in the metadata line %s\n", mdlinep)

        decode_sendtime (2, /*assume format 2 */ &mdp->vx_epoch_ms, &mdp->tx_epoch_ms, &mdp->socc);

        // if tx log is available then get occupancy from it
        if (len_td) // array is not empty
            find_occ_from_td (mdp->packet_num, mdp->tx_epoch_ms, tdp, len_td, &(mdp->iocc), &(mdp->socc), &(mdp->socc_epoch_ms));

        return 1;

} // end of read_md_line

// checks if the paramters of the two carriers are consistent 
void check_consistency (int packet_num, struct s_carrier *mdp0, struct s_carrier *mdp1) {

    if (mdp0->match==0 || mdp1->match==0)
        // nothing to check as one of the carriers is not involved in this transmission
        return; 
    
    if (mdp0->packet_len != mdp1->packet_len)
        FATAL ("Packet len field of Packet num %d inconsistent\n", packet_num)

    if (mdp0->frame_start != mdp1->frame_start)
        FATAL ("frame_start field of Packet num %d inconsistent\n", packet_num)

    if (mdp0->frame_num != mdp1->frame_num)
        FATAL ("frame_num field of Packet num %d inconsistent\n", packet_num)

    if (mdp0->frame_rate != mdp1->frame_rate)
        FATAL ("frame_rate field of Packet rate %d inconsistent\n", packet_num)

    if (mdp0->frame_res != mdp1->frame_res)
        FATAL ("frame_res field of Packet rate %d inconsistent\n", packet_num)

    if (mdp0->frame_end != mdp1->frame_end)
        FATAL ("frame_end field of Packet rate %d inconsistent\n", packet_num)

    return;
} // end of check_consistency

// prints out the common stuff
void emit_common (int print_header, int packet_num, struct s_carrier *mdp, FILE *fp) {

    if (print_header) fprintf (fp, "F#, ");    else fprintf (fp, "%d, ", mdp->frame_num);
    if (print_header) fprintf (fp, "P#, ");    else fprintf (fp, "%d, ", packet_num);
    if (print_header) fprintf (fp, "P_len, "); else fprintf (fp, "%d, ", mdp->packet_len);
    if (print_header) fprintf (fp, "F_S, ");   else fprintf (fp, "%d, ", mdp->frame_start);
    if (print_header) fprintf (fp, "F_E, ");   else fprintf (fp, "%d, ", mdp->frame_end);
    if (print_header) fprintf (fp, "Rate, ");  else fprintf (fp, "%d, ", mdp->frame_rate);
    if (print_header) fprintf (fp, "Res, ");   else fprintf (fp, "%d, ", mdp->frame_res);

    return;
} // end of emit_common

void emit_per_carrier (int print_header, int len_td, struct s_carrier *mdp, FILE *fp, char *Cx) {

    if (print_header) fprintf (fp, "%s, ", Cx);   else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, ", "); 
    if (print_header) fprintf (fp, "Cx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->camera_epoch_ms);
    if (print_header) fprintf (fp, "vx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->vx_epoch_ms);
    if (print_header) fprintf (fp, "tx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->tx_epoch_ms);
    if (print_header) fprintf (fp, "rx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->rx_epoch_ms);
    if (print_header) fprintf (fp, "socc, ");     else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%d, ", mdp->socc);
    if (len_td) {
        if (print_header) fprintf (fp, "iocc, "); else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%d, ", mdp->iocc);
        if (print_header) fprintf (fp, "socc_epoch_ms, "); else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->socc_epoch_ms);
    }
    if (print_header) fprintf (fp, "retx, ");     else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%d, ", mdp->retx);
    if (print_header) fprintf (fp, "chkP, ");     else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%d, ", mdp->check_packet_num);

}  // emit_per_carrier

void emit_combined (
    int print_header,
    int packet_num, 
    struct s_carrier *c0_mdp, struct s_carrier *c1_mdp, struct s_carrier *c2_mdp,
    FILE *fp) {


    if (print_header == 1) {
        emit_common (1, packet_num, c0_mdp, fp); 
        emit_per_carrier (1, len_td0, c0_mdp, fp, "c0");
        emit_per_carrier (1, len_td1, c1_mdp, fp, "c1");
        emit_per_carrier (1, len_td2, c2_mdp, fp, "c2");
        fprintf (fp, "\n");
        return;
    }
    
    emit_common (0, packet_num, 
        // get common info from a channel involved in transmission
        c0_mdp->match? c0_mdp : c1_mdp->match? c1_mdp : c2_mdp, 
        fp);

    emit_per_carrier (0, len_td0, c0_mdp, fp, "c0");
    emit_per_carrier (0, len_td1, c1_mdp, fp, "c1");
    emit_per_carrier (0, len_td2, c2_mdp, fp, "c2");
    fprintf (fp, "\n");

    return;
} // emit_combined

// prints program usage
void print_usage (void) {
    char *usage = "Usage: 3csv21 -ipath <dir> -opath <dir> -no_tx|tx_pre <prefix> -rx_pre <prefix>"; 
    printf ("%s\n", usage);
    printf ("\t -ipath: input path directory. last ipath name applies to next input file\n"); 
    printf ("\t -opath: out path directory \n"); 
    printf ("\t -no_tx|tx_pre: tranmsmit side log filename without the extension .log. Use -no_tx if no tx side file.\n"); 
    printf ("\t -rx_pre: receive side meta data filename without the extension .csv\n"); 

    return;
}

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

// sorts td array by tx timestamp
void sort_td_by_tx_TS (struct s_txlog *tdp, int len) {

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
} // end of sort_td_by_tx_TS

// sorts md array by packet number
void sort_md_by_pkt_num (struct s_carrier *mdp, int len) {

    int i, j; 

    for (i=1; i<len; i++) {
        j = i; 
        while (j != 0) {
            if ((mdp+j)->packet_num < (mdp+j-1)->packet_num) {
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
    } // for elements in the log data array

    return;
} // end of sort_td_by_tx_TS

// returns number of entries in the carrirer meta data file
int read_md (char *prefix) {

    // allocate storage for tx log
    c0_md = (struct s_carrier *) malloc (sizeof (struct s_carrier) * MD_BUFFER_SIZE);
    c1_md = (struct s_carrier *) malloc (sizeof (struct s_carrier) * MD_BUFFER_SIZE);
    c2_md = (struct s_carrier *) malloc (sizeof (struct s_carrier) * MD_BUFFER_SIZE);

    if (c0_md==NULL || c1_md==NULL || c2_md==NULL)
        FATAL("Could not allocate storage to read the tx log file in an array%s\n", "")

    int i; 
    for (i=0; i<3; i++) {
        char buffer[1000], *bp=buffer; 
        struct s_carrier *mdp; 
        struct s_txlog *tdp; 
	    int *len_md, *len_td;
        FILE *fp; 

        mdp = i==0? c0_md : i==1? c1_md : c2_md; 
        tdp = i==0? td0 : i==1? td1 : td2; 
        len_md = i==0? &len_md0 : i==1? &len_md1 : &len_md2; 
        len_td = i==0? &len_td0 : i==1? &len_td1 : &len_td2; 

        
        sprintf (bp, "%s_ch%d.csv", prefix, i);
        fp = open_file (bp, "r"); 

        // skip header
        read_md_line (1, fp, tdp, *len_td, mdp);

		// read tx log file into array and sort it
		while (read_md_line (0, fp, tdp, *len_td, mdp)) {
		    (*len_md)++;
		    if (*len_md == MD_BUFFER_SIZE)
		        FATAL ("MD data array is not large enough for ch%d. Increase TX_BUFFER_SIZE\n", i)
		    mdp++;
		} // while there are more lines to be read

		if (*len_md == 0)
		    FATAL("Ch%d meta data file is empty\n", i)

		// sort by packet number
        mdp = i==0? c0_md : i==1? c1_md : c2_md; 
        sort_md_by_pkt_num (mdp, *len_md); 
    } // for each carrier

} // read_md

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
        tdp = i==0? td0 : i==1? td1 : td2; 
        sort_td_by_tx_TS (tdp, *len_td); 

        // reset the file pointer to the start of the file for the next channel
        fseek (fp, 0, SEEK_SET);

    } // for each carrier

} // read_td

int main (int argc, char* argv[]) {
    char buffer[1000], *bp=buffer; 
    char ipath[500], *ipathp = ipath; 
    char opath[500], *opathp = opath; 
    char tx_prefix[500], *tx_prefixp = tx_prefix; 
    char rx_prefix[500], *rx_prefixp = rx_prefix; 
    int tx_specified = 0; 
    FILE *td_fp = NULL;                     // transmit log file
    FILE *c0_fp = NULL;                     // carrier 0 meta data file
    FILE *c1_fp = NULL;                     // carrier 0 meta data file
    FILE *c2_fp = NULL;                     // carrier 0 meta data file
    FILE *out_fp = NULL;                    // combined output file 
    int c0_md_index = 0, c1_md_index = 0, c2_md_index = 0; 
    struct s_carrier *c0_mdp, *c1_mdp, *c2_mdp; 
    int packet_num = 0; 

    //  read command line arguments
    while (*++argv != NULL) {

        if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            exit (-1); 
        }

        // input / output file locations and file prefix
        else if (strcmp (*argv, "-ipath") == MATCH) {
            strcpy (ipathp, *++argv); 
        }

        else if (strcmp (*argv, "-opath") == MATCH) {
            strcpy (opathp, *++argv); 
        }

        else if (strcmp (*argv, "-tx_pre") == MATCH) {
            strcpy (tx_prefix, *++argv); 
            tx_specified = 1; 
        }

        else if (strcmp (*argv, "-no_tx") == MATCH) {
            tx_specified = 0; 
        }

        else if (strcmp (*argv, "-rx_pre") == MATCH) {
            strcpy (rx_prefix, *++argv); 
        }

        else {
            FATAL("Invalid option %s\n", *argv)
        }
    } // while there are more arguments to process

    // open files
    // combined output file 
	sprintf (bp, "%s%s_3csv21.csv", opath, rx_prefix); 
    out_fp = open_file (bp, "w");

    // warning message file
	sprintf (bp, "%s%s_warnings_3csv21.txt", opath, rx_prefix);
    warn_fp = open_file (bp, "w");

    // transmit log file. Must be ready before reading carrier meta data files
    if (tx_specified) {
         sprintf (bp, "%s%s.log", ipathp, tx_prefix); 
        if ((td_fp = open_file (bp, "r")) != NULL)
            read_td(td_fp);
    } 
    // else len_td = 0 and tdp = NULL; 

	// open the 3 meta input data files. Must be read after read tx log file if specified.
	sprintf (bp, "%s%s", ipath, rx_prefix);
    read_md (bp); 

    /*
	sprintf (bp, "%s%s_ch0.csv", ipath, rx_prefix);
    len_md0 = read_md(c0_fp); 
	sprintf (bp, "%s%s_ch1.csv", ipath, rx_prefix);
	c1_fp = open_file (bp, "r");
    len_md1 = read_md(c1_fp); 
	sprintf (bp, "%s%s_ch2.csv", ipath, rx_prefix);
	c2_fp = open_file (bp, "r");
    len_md2 = read_md(c2_fp); 
    */

    /*
    int i; 
    for (i=0; i < len_md0; i++) {
        (c0_md+i)->tdp = td0; (c0_md+i)->len_td = len_td0;
    }
    for (i=0; i < len_md1; i++) {
        (c1_md+i)->tdp = td1; (c1_md+i)->len_td = len_td1;
    }
    for (i=0; i < len_md2; i++) {
        (c2_md+i)->tdp = td2; (c2_md+i)->len_td = len_td2;
    }
    */
	
    // print header
    emit_combined (1, packet_num, c0_md, c1_md, c2_md, out_fp); 
    
    // while loop is entered with the line to be processed already read or EOF read for a carrier
    // and the pkt_num is the next packet to be transmitted
    packet_num = 0; 
    c0_md_index = 0; c0_mdp = c0_md + c0_md_index; 
    c1_md_index = 0; c1_mdp = c1_md + c1_md_index; 
    c2_md_index = 0; c2_mdp = c2_md + c2_md_index; 
    while ( (c0_md_index < len_md0) && (c1_md_index < len_md1)  && (c2_md_index < len_md2)) {
        int retransmission = 0; 
        /*
        retransmission 
        */
        // first check if a channel wants to retransmit last packet
        if (c0_mdp->match = c0_mdp->packet_num == (packet_num-1)) 
            check_consistency (c0_mdp->packet_num, c0_mdp, (c0_mdp-1)); 
        if (c1_mdp->match = c1_mdp->packet_num == (packet_num-1)) 
            check_consistency (c1_mdp->packet_num, c1_mdp, (c1_mdp-1));
        if (c2_mdp->match = c2_mdp->packet_num == (packet_num-1)) 
            check_consistency (c2_mdp->packet_num, c2_mdp, (c2_mdp-1)); 
        switch (c0_mdp->match + c1_mdp->match + c2_mdp->match) {
            case 0: { // no one wants to retransmit
                break; 
            } // case 0
            case 1: case 2: case 3: { // one or more channels want to re-transmit this packet
                FWARN (warn_fp, "Packet num %d re-transmitted by one or more carrier\n", packet_num-1)
                retransmission = 1; 
                goto EMIT_OUTPUT;
            } // end of case1, 2 and 3
        } // end of switch (c0_match + c1_match + c2_match)

        /*
        not a retransmission 
        */
        // check that the files are sorted by incresaing packet number
        if ((packet_num > c0_mdp->packet_num) || (packet_num > c1_mdp->packet_num) || (packet_num > c2_mdp->packet_num)) {
            printf ("packet num %d, c0_mdp->packet_num %d\n", packet_num, c0_mdp->packet_num);
            printf ("packet num %d, c1_mdp->packet_num %d\n", packet_num, c1_mdp->packet_num);
            printf ("packet num %d, c2_mdp->packet_num %d\n", packet_num, c2_mdp->packet_num);
            FATAL ("The input files MUST be sorted in increasing packet number. Check around packet %d\n", packet_num)
        }

        // now check which channels will transmit this packet
        c0_mdp->match = c0_mdp->packet_num == packet_num; 
        c1_mdp->match = c1_mdp->packet_num == packet_num; 
        c2_mdp->match = c2_mdp->packet_num == packet_num; 

        switch (c0_mdp->match + c1_mdp->match + c2_mdp->match) {
            case 0: { // this packet not transmitted by any channel
                // skip this packet 
                FWARN (warn_fp, "Packet num %d NOT transmitted by ANY carrier\n", packet_num)
                packet_num++;
                continue; 
            } // case 0
            case 1: { // only one channel wants to transmit this packet
                FWARN (warn_fp, "Packet num %d transmitted by one carrier ONLY\n", packet_num)
                break; 
            } // case 1
            /* modified to accommpdate 3-q algo where 1, 2 or 3 channels may participate in tranmitting a packet
            case 2: { // normal case: each packet shoueld be transmitted by two channels 
                break;
            } // case 2
            case 3: { // should not happen
                FWARN (warn_fp, "Packet num %d transmitted by MORE than 2 channels\n", packet_num)
                break; 
            } // case 3
            */
            default: break; 
        } // end of switch (c0_match + c1_match + c2_match)

        // check consistency of common fields
        check_consistency (packet_num, c0_mdp, c1_mdp);
        check_consistency (packet_num, c0_mdp, c2_mdp); 
        check_consistency (packet_num, c1_mdp, c2_mdp); 

        EMIT_OUTPUT:
        emit_combined (0, retransmission? packet_num-1: packet_num, c0_mdp, c1_mdp, c2_mdp, out_fp); 

        // read next lines for the channels that participated in this transaction
        if (c0_mdp->match) {
            c0_md_index++; c0_mdp = c0_md + c0_md_index; 
        }
        if (c1_mdp->match) {
            c1_md_index++; c1_mdp = c1_md + c1_md_index; 
        }
        if (c2_mdp->match) {
            c2_md_index++; c2_mdp = c2_md + c2_md_index; 
        }

        if (retransmission)
            ; // have not processed the current packet number so don't increment it
        else
            packet_num++;
        
        if ((packet_num % 10000) == 0)
            printf ("packet # %d\n", packet_num);

    } // while there are more lines to be read

    my_exit (0); 
} // end of main
