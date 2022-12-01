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

struct s_carrier {
        unsigned packet_num;                // packet number read from this line of the carrier meta_data finle 
        double vx_epoch_ms; 
        double tx_epoch_ms; 
        double rx_epoch_ms;
        int modem_occ; 
        unsigned packet_len;
        unsigned frame_start;
        unsigned frame_num;
        unsigned frame_rate;
        unsigned frame_res;
        unsigned frame_end;
        double camera_epoch_ms;
        unsigned retx;
        unsigned check_packet_num;

        int match;                              // set to 1 if this cahnle matches the packet num to be transmitted
};

#define		FATAL(STR, ARG) {printf (STR, ARG); exit(-1);}
#define		FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (int format, double *vx_epoch_ms, double *tx_epoch_ms, int *modem_occ) {
            double real_vx_epoch_ms;
            unsigned tx_minus_vx; 
            
            // calculate tx-vx and modem occupancy
            if (format == 1) {
                real_vx_epoch_ms = /* starts at bit 8 */ trunc (*vx_epoch_ms/256);
                *modem_occ = 31; // not available
                tx_minus_vx =  /* lower 8 bits */ *vx_epoch_ms - real_vx_epoch_ms*256;
            } // format 1:only tx-vx available
            else if (format == 2) {
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
} // end of decode_sendtime

FILE *open_file (char *file_namep, char *modep) {
    FILE *fp;
    
    if ((fp = fopen (file_namep, modep)) == NULL)
        FATAL ("Could not open file %s\n", file_namep)
    
    return fp; 
} // end of open_file

// reads and parses a meta data line from the specified file. returns 1 if end of file reached
int read_line (int read_header, FILE *fp, struct s_carrier *mdp) {

    char mdline[MAX_MD_LINE_SIZE], *mdlinep = mdline; 
    // packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolut, ion	 frame_end	 camera_timestamp	 retx	 chPacketNum

    if (fgets (mdline, MAX_MD_LINE_SIZE, fp) == NULL)
        // reached end of the file
        return 1;

    if (read_header)
        return 0;
    
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
        &mdp->check_packet_num) !=12)
        FATAL ("could not find all the fields in the metadata line %s\n", mdlinep)

        decode_sendtime (2, /*assume format 2 */ &mdp->vx_epoch_ms, &mdp->tx_epoch_ms, &mdp->modem_occ);

        return 0;

} // end of read_line

// checks if the paramters of the two carriers are consistent 
void check_consistency (unsigned packet_num, struct s_carrier *mdp0, struct s_carrier *mdp1) {
    if (mdp0->match==0 || mdp1->match==0)
        // nothing to check as one of the carriers is not involved in this transmission
        return; 
    
    if (mdp0->packet_len != mdp1->packet_len)
        FATAL ("Packet len field of Packet num %u inconsistent\n", packet_num)

    if (mdp0->frame_start != mdp1->frame_start)
        FATAL ("frame_start field of Packet num %u inconsistent\n", packet_num)

    if (mdp0->frame_num != mdp1->frame_num)
        FATAL ("frame_num field of Packet num %u inconsistent\n", packet_num)

    if (mdp0->frame_rate != mdp1->frame_rate)
        FATAL ("frame_rate field of Packet rate %u inconsistent\n", packet_num)

    if (mdp0->frame_res != mdp1->frame_res)
        FATAL ("frame_res field of Packet rate %u inconsistent\n", packet_num)

    if (mdp0->frame_end != mdp1->frame_end)
        FATAL ("frame_end field of Packet rate %u inconsistent\n", packet_num)

    return;
} // end of check_consistency

// prints out the common stuff
void emit_common (int print_header, unsigned packet_num, struct s_carrier *mdp, FILE *fp) {

    if (print_header) fprintf (fp, "F#, ");    else fprintf (fp, "%u, ", mdp->frame_num);
    if (print_header) fprintf (fp, "P#, ");    else fprintf (fp, "%u, ", packet_num);
    if (print_header) fprintf (fp, "P_len, "); else fprintf (fp, "%u, ", mdp->packet_len);
    if (print_header) fprintf (fp, "F_S, ");   else fprintf (fp, "%u, ", mdp->frame_start);
    if (print_header) fprintf (fp, "F_E, ");   else fprintf (fp, "%u, ", mdp->frame_end);
    if (print_header) fprintf (fp, "Rate, ");  else fprintf (fp, "%u, ", mdp->frame_rate);
    if (print_header) fprintf (fp, "Res, ");   else fprintf (fp, "%u, ", mdp->frame_res);

    return;
} // end of emit_common

void emit_per_carrier (int print_header, struct s_carrier *mdp, FILE *fp, char *Cx) {

    if (print_header) fprintf (fp, "%s, ", Cx);   else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, ", "); 
    if (print_header) fprintf (fp, "Cx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->camera_epoch_ms);
    if (print_header) fprintf (fp, "vx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->vx_epoch_ms);
    if (print_header) fprintf (fp, "tx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->tx_epoch_ms);
    if (print_header) fprintf (fp, "rx_TS, ");    else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%lf, ", mdp->rx_epoch_ms);
    if (print_header) fprintf (fp, "occ, ");      else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%u, ", mdp->modem_occ);
    if (print_header) fprintf (fp, "retx, ");     else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%u, ", mdp->retx);
    if (print_header) fprintf (fp, "chkP, ");     else if (!mdp->match)  fprintf (fp, ", ");    else fprintf (fp, "%u, ", mdp->check_packet_num);

}  // emit_per_carrier

void emit_combined (
    int print_header,
    unsigned packet_num, 
    struct s_carrier *c0_mdp, struct s_carrier *c1_mdp, struct s_carrier *c2_mdp,
    FILE *fp) {


    if (print_header == 1) {
        emit_common (1, packet_num, c0_mdp, fp); 
        emit_per_carrier (1, c0_mdp, fp, "c0");
        emit_per_carrier (1, c1_mdp, fp, "c1");
        emit_per_carrier (1, c2_mdp, fp, "c2");
        fprintf (fp, "\n");
        return;
    }
    
    // check consistency of common fields
    check_consistency (packet_num, c0_mdp, c1_mdp);
    check_consistency (packet_num, c0_mdp, c2_mdp); 
    check_consistency (packet_num, c1_mdp, c2_mdp); 

    emit_common (0, packet_num, 
        // get common info from a channel involved in transmission
        c0_mdp->match? c0_mdp : c1_mdp->match? c1_mdp : c2_mdp, 
        fp);

    emit_per_carrier (0, c0_mdp, fp, "c0");
    emit_per_carrier (0, c1_mdp, fp, "c1");
    emit_per_carrier (0, c2_mdp, fp, "c2");
    fprintf (fp, "\n");

    return;
} // emit_combined

// prints program usage
void print_usage (void) {
    char *usage = "Usage: 3csv21 <prefix>"; 
    printf ("%s\n", usage);
}

int main (int argc, char* argv[]) {
    FILE *c0_fp = NULL;                     // carrier 0 meta data file
    FILE *c1_fp = NULL;                     // carrier 0 meta data file
    FILE *c2_fp = NULL;                     // carrier 0 meta data file
    FILE *out_fp = NULL;                    // combined output file 
    FILE *warn_fp = NULL;                   // combined output file 
    int silent = 0;                         // if 1 then warnigns are not printed
    int c0_eof, c1_eof, c2_eof;             // set to 1 if end of file readched
    struct s_carrier c0_md, *c0_mdp = &c0_md, c1_md, *c1_mdp = &c1_md, c2_md, *c2_mdp = &c2_md;
    unsigned packet_num = 0; 

    //  read command line arguments
    while (*++argv != NULL) {
        char buffer[100], *bp=buffer; 

        // help/usage
        if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            exit (-1); 
        }

        // output file prefix
        else if (strcmp (*argv, "-out") == MATCH) {
            // combined output file 
	        sprintf (bp, "%s.csv", *++argv); 
            if ((out_fp = fopen (bp, "w")) == NULL) {
                FATAL ("could not open the output file %s", bp)
            }
            // warning message file
	        sprintf (bp, "%s_warnings.txt", *argv); 
            if ((warn_fp = fopen (bp, "w")) == NULL) {
                FATAL ("could not open the output file %s", bp)
            }
        }
        
        // input file prefix
        else { // the argument must be prefix
	        // open the 3 meta input data files
	        sprintf (bp, "%s_ch0.csv", *argv); 
	        c0_fp = open_file (bp, "r");
	        sprintf (bp, "%s_ch1.csv", *argv); 
	        c1_fp = open_file (bp, "r");
	        sprintf (bp, "%s_ch2.csv", *argv); 
	        c2_fp = open_file (bp, "r");
        }
	
    } // while there are more arguments to process

    // check if any missing arguments
    if ((c0_fp == NULL) || (c1_fp == NULL) || (c2_fp == NULL) || (out_fp == NULL))
        FATAL ("could not open input or output files", "")

    // skip header. print header
    c0_eof = read_line (1, c0_fp, c0_mdp);
    c1_eof = read_line (1, c1_fp, c1_mdp);  
    c2_eof = read_line (1, c2_fp, c2_mdp);
    emit_combined (1, packet_num, c0_mdp, c1_mdp, c2_mdp, out_fp); 
    
    // while there is a line to be read from any of the input files

    c0_eof = read_line (0, c0_fp, c0_mdp);
    c1_eof = read_line (0, c1_fp, c1_mdp);  
    c2_eof = read_line (0, c2_fp, c2_mdp);

    // while loop is entered with the line to be processed already read or EOF read for a carrier
    // and the pkt_num is the next packet to be transmitted
    while ( !(c0_eof || c1_eof || c2_eof) ) {
        int retransmission = 0; 

        // first check if a channel wants to retransmit last packet
        c0_mdp->match = c0_mdp->packet_num == (packet_num-1); 
        c1_mdp->match = c1_mdp->packet_num == (packet_num-1); 
        c2_mdp->match = c2_mdp->packet_num == (packet_num-1); 
        switch (c0_mdp->match + c1_mdp->match + c2_mdp->match) {
            case 0: { // normal case: packet should not be transmitted twice
                break; 
            } // case 0
            case 1: case 2: case 3: { // one channel wants to re-transmit this packet
                FWARN (warn_fp, "Packet num %u re-transmitted by one or more carrier\n", packet_num-1)
                retransmission = 1; 
                goto EMIT_OUTPUT;
                ; 
            } // end of case1, 2 and 3
        } // end of switch (c0_match + c1_match + c2_match)

        // check that the files are sorted by incresaing packet number
        if ((packet_num > c0_mdp->packet_num) || (packet_num > c1_mdp->packet_num) || (packet_num > c2_mdp->packet_num))
            FATAL ("The input files MUST be sorted in increasing packet number. Check around packet %d\n", packet_num)

        // now check which channels will transmit this packet
        c0_mdp->match = c0_mdp->packet_num == packet_num; 
        c1_mdp->match = c1_mdp->packet_num == packet_num; 
        c2_mdp->match = c2_mdp->packet_num == packet_num; 

        switch (c0_mdp->match + c1_mdp->match + c2_mdp->match) {
            case 0: { // thsi packet not transmitted by any channel
                // skip this packet 
                FWARN (warn_fp, "Packet num %u NOT transmitted by ANY carrier\n", packet_num)
                packet_num++;
                continue; 
            } // case 0
            case 1: { // only one channel wants to transmit this packet
                FWARN (warn_fp, "Packet num %u transmitted by one carrier ONLY\n", packet_num)
                break; 
            } // case 1
            case 2: { // normal case: each packet shoueld be transmitted by two channels 
                break;
            } // case 2
            case 3: { // should not happen
                FWARN (warn_fp, "Packet num %u transmitted by MORE than 2 channels\n", packet_num)
                break; 
            } // case 3
        } // end of switch (c0_match + c1_match + c2_match)

        EMIT_OUTPUT:
        emit_combined (0, retransmission? packet_num-1: packet_num, c0_mdp, c1_mdp, c2_mdp, out_fp); 

        // read next lines for the channels that participated in this transaction
        if (c0_mdp->match) c0_eof = read_line (0, c0_fp, c0_mdp);
        if (c1_mdp->match) c1_eof = read_line (0, c1_fp, c1_mdp);
        if (c2_mdp->match) c2_eof = read_line (0, c2_fp, c2_mdp);

        if (retransmission)
            ; // have not process the current packet number so don't increment it
        else
            packet_num++;
        
        if ((packet_num % 10000) == 0)
            printf ("packet # %d\n", packet_num);

    } // while there are more lines to be read

    exit (0); 
} // end of main
