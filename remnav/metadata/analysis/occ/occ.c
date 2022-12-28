#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define MATCH 0
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MAX_MD_LINE_SIZE 500
#define MD_BUFFER_SIZE 2
#define MAX_SPIKES 1000

// globals
int silent = 0;                         // if 1 then dont generate any warnings on the console

struct s_carrier {
        int packet_num;                 // packet number read from this line of the carrier meta_data finle 
        double vx_epoch_ms;             // encoder output epoch time after stripping out the embedded occ and v2t delay
        double tx_epoch_ms;             // tx epoch time = encoder epoch + v2t latency
        double rx_epoch_ms;             // rx epoch time
        int modem_occ; 
        int packet_len;
        int frame_start;
        int frame_num;
        int frame_rate;
        int frame_res;
        int frame_end;
        double camera_epoch_ms;
        int retx;
        int check_packet_num;
        double t2r_latency_ms;      // rx_epoch_ms = tx_epoch_ms
};

struct s_latency {
        double latency;
        int count; 
};

struct s_spike {
    int active;                 // 1 if in middle of a spike, 0 otherwise
    int below_occ_threshold;    // 1 if the occupancy stayed below the occ_threshold druing the entire spike duraction 
    int start_packet_num;       // packet number where the latency exceeded latency_threshold for an inactive state
    double start_tx_epoch_ms;   // tx time stamp of the starting packet
    int stop_packet_num;        // packet number where the latency dropped below latency_threshold for an active spike
    double duration_ms;         // duraction
};

#define		FATAL(STR, ARG) {printf (STR, ARG); exit(-1);}
#define		WARN(STR, ARG) {if (!silent) printf (STR, ARG);}
#define		FWARN(FILE, STR, ARG) {if (!silent) fprintf (FILE, STR, ARG);}

// extracts tx_epoch_ms from the vx_epoch_ms embedded format
void decode_sendtime (int format, double *vx_epoch_ms, double *tx_epoch_ms, int *modem_occ) {
            double real_vx_epoch_ms;
            double tx_minus_vx; 
            
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

// reads and parses a meta data line from the specified file. returns 0 if end of file reached or if the scan for all the fields fails due
// to an incomplete line as in last line of the file.
int read_line (int read_header, FILE *fp, struct s_carrier *mdp) {

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

        decode_sendtime (2, /*assume format 2 */ &mdp->vx_epoch_ms, &mdp->tx_epoch_ms, &mdp->modem_occ);
        mdp->t2r_latency_ms = mdp->rx_epoch_ms - mdp->tx_epoch_ms; 

        return 1;

} // end of read_line

void emit_output (int print_header, int index, struct s_latency *lp, int len_spike_table, struct s_spike *sp, FILE *fp) {

    // occupancy
    if (print_header) fprintf (fp, "count, ");          else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", lp->count);
    if (print_header) fprintf (fp, ",");                else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", index);
    if (print_header) fprintf (fp, "avg latency, ");    else if ((index >= 31) || (lp->count ==0)) fprintf (fp, ","); 
                                                        else fprintf (fp, "%.0f, ", lp->latency/lp->count);

    // spike
    if (print_header) fprintf (fp, "belwo occ th, ");   else if (index >= len_spike_table) fprintf (fp, ","); else fprintf (fp, "%d,", sp->below_occ_threshold);
    if (print_header) fprintf (fp, "start, ");          else if (index >= len_spike_table) fprintf (fp, ","); else fprintf (fp, "%d, ", sp->start_packet_num); 
    if (print_header) fprintf (fp, "stop, ");           else if (index >= len_spike_table) fprintf (fp, ","); else fprintf (fp, "%d, ", sp->stop_packet_num); 
    if (print_header) fprintf (fp, "duration_pkts, ");  else if (index >= len_spike_table) fprintf (fp, ","); else fprintf (fp, "%d, ", sp->stop_packet_num - sp->start_packet_num + 1); 
    if (print_header) fprintf (fp, "duration_ms, ");    else if (index >= len_spike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf, ", sp->duration_ms); 

    fprintf(fp, "\n");

} // end of emit_output

void emit_aux (int print_header, struct s_carrier *cp, FILE *fp) {

    if (print_header) fprintf (fp, "packet_num,");          else fprintf (fp, "%d, ", cp->packet_num); 
    if (print_header) fprintf (fp, "vx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->vx_epoch_ms); 
    if (print_header) fprintf (fp, "tx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->tx_epoch_ms); 
    if (print_header) fprintf (fp, "rx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->rx_epoch_ms); 
    if (print_header) fprintf (fp, "camera_epoch_ms,");     else fprintf (fp, "%.0lf,", cp->camera_epoch_ms); 
    if (print_header) fprintf (fp, "modem_occ,");           else fprintf (fp, "%d, ", cp->modem_occ); 
    if (print_header) fprintf (fp, "packet_len,");          else fprintf (fp, "%d, ", cp->packet_len); 
    if (print_header) fprintf (fp, "frame_start,");         else fprintf (fp, "%d, ", cp->frame_start); 
    if (print_header) fprintf (fp, "frame_num,");           else fprintf (fp, "%d, ", cp->frame_num); 
    if (print_header) fprintf (fp, "frame_rate,");          else fprintf (fp, "%d, ", cp->frame_rate); 
    if (print_header) fprintf (fp, "frame_res,");           else fprintf (fp, "%d, ", cp->frame_res); 
    if (print_header) fprintf (fp, "frame_end,");           else fprintf (fp, "%d, ", cp->frame_end); 
    if (print_header) fprintf (fp, "retx,");                else fprintf (fp, "%d, ", cp->retx); 
    if (print_header) fprintf (fp, "check_packet_num,");    else fprintf (fp, "%d, ", cp->check_packet_num); 

    fprintf (fp, "\n");
    return; 

} // end of emit_aux

// prints program usage
void print_usage (void) {
    char *usage = "Usage: occ [-help] [-o <dd>] [-l <dd>] [-s]-pre <prefix>";
    printf ("%s\n", usage);
    printf ("\t -o: occupancy threshold for degraded channel. Default 10.\n"); 
    printf ("\t -l: latency threshold for degraded channel. Default 60ms.\n"); 
    printf ("\t -s: Turns on silent, no console warning. Default off.\n"); 
    printf ("\t -pre: filename without the extension\n"); 
    return; 
} // print_usage`


int main (int argc, char* argv[]) {
    int occ_threshold = 10;                 // occupancy threshold for degraded channel
    int latency_threshold = 60;             // latency threshold for degraded cahnnel 
    FILE *md_fp = NULL;                     // meta data file
    FILE *out_fp = NULL;                    // output file 
    FILE *aux_fp = NULL;                    // auxiliary output with decoded occ etc. 
    FILE *warn_fp = NULL;                   // warnings output file
    int md_index = 0;                       
    struct s_carrier md[MD_BUFFER_SIZE], *mdp = &(md[md_index]);
    int len_mdfile = 0;                     // lines in meta data file not including header line
    double last_tx_epoch_ms;                // remembers tx of the last md line
    int last_modem_occ;                     // remembers occ from the last update from modem
    int last_packet_num;                    // remembers packet number of the last md line

    struct s_latency latency_table_by_occ[31];
    struct s_spike spike_table[MAX_SPIKES];
    int len_spike_table = 0;
    struct s_spike spike, *spikep = &spike; 

    //  read command line arguments
    while (*++argv != NULL) {
        char buffer[100], *bp=buffer; 

        // help/usage
        if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            exit (-1); 
        }

        // occ limit
        else if (strcmp (*argv, "-o") == MATCH) {
            if (sscanf (*++argv, "%d", &occ_threshold) != 1) {
                printf ("Missing specification of the occupancy threshold\n");
                print_usage (); 
                exit (-1); 
            }
        } // occ limit
        
        // latency limit
        else if (strcmp (*argv, "-l") == MATCH) {
            if (sscanf (*++argv, "%d", &latency_threshold) != 1) {
                printf ("Missing specification of the latency threshold\n");
                print_usage (); 
                exit (-1); 
            }
        } // latency limit
        
        // silent mode
        else if (strcmp (*argv, "-s") == MATCH) {
            silent = 1; 
        } // latency limit
        
        // input file prefix
        else if (strcmp (*argv, "-pre") == MATCH) {

            sprintf (bp, "%s.csv", *++argv); 
	        md_fp = open_file (bp, "r");

            sprintf (bp, "%s_out.csv", *argv); 
	        out_fp = open_file (bp, "w");

            sprintf (bp, "%s_aux.csv", *argv);
            aux_fp = open_file (bp, "w"); 

	        sprintf (bp, "%s_warnings.txt", *argv); 
            warn_fp = open_file (bp, "w"); 
        } // prefix

        // invalid option
        else {
            FATAL("Invalid option %s\n", *argv)
        }
	
    } // while there are more arguments to process

    if (md_fp == NULL) {
        printf ("Missing or could not open input file\n"); 
        print_usage (); 
        exit (-1); 
    }

    // initialization
    int i; 

    // latency distribution by occupancy table
    for (i=0; i <31; i++) {
        latency_table_by_occ[i].latency = 0;
        latency_table_by_occ[i].count = 0;
    } // latency_table

    // latency spike duration table
    for (i=0; i < MAX_SPIKES; i++)
        spike_table[i].active = 0;
    len_spike_table = 0; 
    spikep = &spike_table[len_spike_table]; 

    // occupancy spike duration table

    len_mdfile = 0; 
    last_tx_epoch_ms = 0; 
    last_modem_occ = 0; 
    last_packet_num = 0; 

    // skip header. print header
    read_line (1, md_fp, mdp);
    emit_aux (1, mdp, aux_fp); 
    
    // while there is a line to be read from any of the input files
    while (read_line (0, md_fp, mdp)) {
        len_mdfile++;

        // auxiliary output
        emit_aux (0, mdp, aux_fp);

        // occupancy
        if ((mdp->modem_occ < 0) || (mdp->modem_occ > 31)) {
            printf ("INvalid occupancy %d at line %d\n", mdp->modem_occ, len_mdfile /* +1 for header line*/ + 1); 
            exit (-1);
        }
        if (mdp->modem_occ == 31) // new modem occupancy not available with this packet
            mdp->modem_occ = last_modem_occ; 
        
        latency_table_by_occ[mdp->modem_occ].latency += mdp->rx_epoch_ms - mdp->tx_epoch_ms;
        latency_table_by_occ[mdp->modem_occ].count++; 
         
        // spike 
        if (mdp->tx_epoch_ms < last_tx_epoch_ms)
            FATAL("Metadata file not sorted by tx time order at line %d\n", len_mdfile /* +1 for header line */ +1);

        switch (spikep->active) {
            case 0: // spike inactive
                if (mdp->t2r_latency_ms > latency_threshold) {
                    // start of a spike
                    spikep->active = 1;
                    spikep->below_occ_threshold = mdp->modem_occ <= occ_threshold; 
                    spikep->stop_packet_num = spikep->start_packet_num = mdp->packet_num; 
                    spikep->start_tx_epoch_ms = mdp->tx_epoch_ms; 
                } // start of a spike
                // else stay inactive
                break;
            case 1: // spike active
                // check for continuation 
                if (mdp->t2r_latency_ms > latency_threshold) { // spike continues
                    // check if the occupancy exceeded threshold
                    if (mdp->modem_occ > occ_threshold) {
                        // if the occupancy exceeded threshold anytime during the spike, it is considered high occupancy spike
                        spikep->below_occ_threshold = 0; 
                    } // high occ spike
                } // spike continues

                // else spike completed
                else {
                    spikep->stop_packet_num = last_packet_num; 
                    spikep->duration_ms = mdp->tx_epoch_ms - spikep->start_tx_epoch_ms;
                    spikep->active = 0;
                    len_spike_table++; 
                    if (len_spike_table == MAX_SPIKES)
                        FATAL ("Spike table is full. Increase MAX_SPIKE constant %d\n", MAX_SPIKES)
                    spikep = &spike_table[len_spike_table];
                }
                break;
        } // end of switch

        last_tx_epoch_ms = mdp->tx_epoch_ms; 
        last_modem_occ = mdp->modem_occ; 
        last_packet_num = mdp->packet_num; 
        md_index = (md_index + 1) % MD_BUFFER_SIZE; 
        mdp = &md[md_index]; 
        
        if (len_mdfile % 1000 == 0)
            printf ("Reached line %d\n", len_mdfile); 
    } // while there are more files to be read

    // if a spike is still active when the file end is reached, then close it.
    if (spikep->active) {
        // roll back mdp to point to the last data line
        md_index = (md_index + 1) % MD_BUFFER_SIZE; 
        mdp = &md[md_index]; 
        spikep->stop_packet_num = mdp->packet_num; 
        spikep->duration_ms = mdp->tx_epoch_ms - spikep->start_tx_epoch_ms;
        spikep->active = 0;
        len_spike_table++; 
    } // spike was active when the end of the file was reached

    // print output
    // header
    emit_output (1, 0, &latency_table_by_occ[0], 0, &spike_table[0], out_fp); 
    int num_of_output_lines = MAX(31, len_spike_table);
    for (i=0; i < num_of_output_lines; i++){
        emit_output (0, i, &latency_table_by_occ[MIN(31-1,i)], len_spike_table, &spike_table[i], out_fp); 
    }

    

    exit (0); 
} // end of main
