#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define MATCH 0
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MAX_MD_LINE_SIZE 500
#define MD_BUFFER_SIZE (20*60*1000)
#define MAX_SPIKES 1000
#define MAX_FILES 100

// globals
int silent = 0;                         // if 1 then dont generate any warnings on the console

struct s_files {
    char input_directory[500];
    char file_prefix[500]; 
};

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
        double t2r_latency_ms;          // rx_epoch_ms - tx_epoch_ms
};
struct s_carrier *md, *mdp; 

struct s_latency {
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
    int stop_packet_num;        // last packet number in active state where the latency exceeded latency_threshold 
    double duration_ms;         // duration
};

struct s_ospike {
    int active;                 // 1 if in middle of a spike, 0 otherwise
    int max_occ;                // highest occ value during this spike
    int start_packet_num;       // first packet number in inactive state where occupancy is greater than threshold
    int stop_packet_num;        // last packet number in active state where occupancy is greater than threshold
    double start_tx_epoch_ms;   // tx time stamp of the starting packet
    double duration_ms;         // duration
}; 

int my_exit (int);

#define		FATAL(STR, ARG) {printf (STR, ARG); my_exit(-1);}
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

// reads and parses a meta data line from the specified file. 
// returns 0 if end of file reached or if the scan for all the fields fails due to an incomplete line as in last line of the file.
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

void emit_output (int print_header, int index, 
    struct s_latency *lp, 
    int len_lspike_table, struct s_lspike *lsp, 
    int len_ospike_table, struct s_ospike *osp, 
    FILE *fp) {

    // occupancy
    if (print_header) fprintf (fp, "count, ");          else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", lp->count);
    if (print_header) fprintf (fp, ",");                else if (index >= 31) fprintf (fp, ","); else fprintf (fp, "%d, ", index);
    if (print_header) fprintf (fp, "avg latency, ");    else if ((index >= 31) || (lp->count ==0)) fprintf (fp, ","); 
                                                        else fprintf (fp, "%.0f, ", lp->latency/lp->count);

    // latency spike
    if (print_header) fprintf (fp, "L: occ, ");         else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->max_occ);
    if (print_header) fprintf (fp, "start, ");          else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->start_packet_num); 
    if (print_header) fprintf (fp, "stop, ");           else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->stop_packet_num); 
    if (print_header) fprintf (fp, "duration_pkts, ");  else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->stop_packet_num - lsp->start_packet_num + 1); 
    if (print_header) fprintf (fp, "duration_ms, ");    else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", lsp->duration_ms); 
    if (print_header) fprintf (fp, "max t2r, ");        else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", lsp->max_latency); 
    if (print_header) fprintf (fp, "max t2r pnum, ");   else if (index >= len_lspike_table) fprintf (fp, ","); else fprintf (fp, "%d,", lsp->max_lat_packet_num); 

    // occupancy spike
    if (print_header) fprintf (fp, "O: occ,");          else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->max_occ); 
    if (print_header) fprintf (fp, "start,");           else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->start_packet_num); 
    if (print_header) fprintf (fp, "stop,");            else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->stop_packet_num); 
    if (print_header) fprintf (fp, "duration_pkts,");   else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%d,", osp->stop_packet_num - osp->start_packet_num + 1); 
    if (print_header) fprintf (fp, "duration_ms,");     else if (index >= len_ospike_table) fprintf (fp, ","); else fprintf (fp, "%.0lf,", osp->duration_ms); 

    fprintf(fp, "\n");

} // end of emit_output

void emit_aux (int print_header, struct s_carrier *cp, FILE *fp) {

    if (print_header) fprintf (fp, "packet_num,");          else fprintf (fp, "%d, ", cp->packet_num); 
    if (print_header) fprintf (fp, "vx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->vx_epoch_ms); 
    if (print_header) fprintf (fp, "tx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->tx_epoch_ms); 
    if (print_header) fprintf (fp, "rx_epoch_ms,");         else fprintf (fp, "%.0lf,", cp->rx_epoch_ms); 
    if (print_header) fprintf (fp, "camera_epoch_ms,");     else fprintf (fp, "%.0lf,", cp->camera_epoch_ms); 
    if (print_header) fprintf (fp, "t2r latency,");         else fprintf (fp, "%.0lf,", cp->t2r_latency_ms); 
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
    char *usage = "Usage: occ [-help] [-o <dd>] [-l <dd>] [-s] -ipath <dir> -opath <dir> -file_pre <prefix> [-file_pre <prefix>] [-ipath <dir>]";
    printf ("%s\n", usage);
    printf ("\t -o: occupancy threshold for degraded channel. Default 10.\n"); 
    printf ("\t -l: latency threshold for degraded channel. Default 60ms.\n"); 
    printf ("\t -s: Turns on silent, no console warning. Default off.\n"); 
    printf ("\t -ipath: input path directory. last ipath name applies to next input file\n"); 
    printf ("\t -opath: out path directory \n"); 
    printf ("\t -file_pre: filename without the extension.\n"); 
    return; 
} // print_usage

int my_exit (int n) {
    if (md != NULL) free (md); 
    exit (n);
} // my_exit

void sort_by_tx (struct s_carrier *mdp, int len) {
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

    } // for elements in the metadata array

    return;
} // end of sort_by_tx

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

int main (int argc, char* argv[]) {
    int occ_threshold = 10;                 // occupancy threshold for degraded channel
    int latency_threshold = 60;             // latency threshold for degraded cahnnel 
    FILE *md_fp = NULL;                     // meta data file
    FILE *out_fp = NULL;                    // output file 
    FILE *aux_fp = NULL;                    // auxiliary output with decoded occ etc. 
    FILE *warn_fp = NULL;                   // warnings output file
    int len_mdfile;                         // lines in meta data file not including header line
    double last_tx_epoch_ms;                // remembers tx of the last md line
    int last_modem_occ;                     // remembers occ from the last update from modem
    int last_packet_num;                    // remembers packet number of the last md line

    struct s_latency latency_table_by_occ[31];
    struct s_lspike lspike_table[MAX_SPIKES];
    struct s_ospike ospike_table[MAX_SPIKES];
    int len_lspike_table;
    int len_ospike_table;
    struct s_lspike lspike, *lspikep = &lspike; 
    struct s_ospike ospike, *ospikep = &ospike; 

    char buffer[1000], *bp=buffer; 
    char ipath[500], *ipathp = ipath; 
    char opath[500], *opathp = opath; 

    struct s_files file_name_table[MAX_FILES];
    int len_file_name_table = 0; 

    //  read command line arguments
    while (*++argv != NULL) {

        // help/usage
        if (strcmp (*argv, "--help")==MATCH || strcmp (*argv, "-help")==MATCH) {
            print_usage (); 
            my_exit (-1); 
        }

        // occ limit
        else if (strcmp (*argv, "-o") == MATCH) {
            if (sscanf (*++argv, "%d", &occ_threshold) != 1) {
                printf ("Missing specification of the occupancy threshold\n");
                print_usage (); 
                my_exit (-1); 
            }
        } // occ limit
        
        // latency limit
        else if (strcmp (*argv, "-l") == MATCH) {
            if (sscanf (*++argv, "%d", &latency_threshold) != 1) {
                printf ("Missing specification of the latency threshold\n");
                print_usage (); 
                my_exit (-1); 
            }
        } // latency limit
        
        // silent mode
        else if (strcmp (*argv, "-s") == MATCH) {
            silent = 1; 
        } // latency limit
        
        // input / output file locations and file_name prefix
        else if (strcmp (*argv, "-ipath") == MATCH) {
            strcpy (ipathp, *++argv); 
        }

        else if (strcmp (*argv, "-opath") == MATCH) {
            strcpy (opathp, *++argv); 
        }

        else if (strcmp (*argv, "-file_pre") == MATCH) {
            strcpy (file_name_table[len_file_name_table].input_directory, ipathp); 
            strcpy (file_name_table[len_file_name_table].file_prefix, *++argv); 
            len_file_name_table++;
        }

        // invalid option
        else {
            FATAL("Invalid option %s\n", *argv)
        }
	
    } // while there are more arguments to process

    // open files
    int file_name_index = 0; 
PROCESS_EACH_FILE:
    sprintf (bp, "%s%s.csv", file_name_table[file_name_index].input_directory, file_name_table[file_name_index].file_prefix); 
	md_fp = open_file (bp, "r");

    sprintf (bp, "%s%s_out.csv", opath, file_name_table[file_name_index].file_prefix); 
	out_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_aux.csv", opath, file_name_table[file_name_index].file_prefix); 
	aux_fp = open_file (bp, "w");

    sprintf (bp, "%s%s_warnings.txt", opath, file_name_table[file_name_index].file_prefix); 
	warn_fp = open_file (bp, "w");

    if ((md_fp == NULL) || (out_fp == NULL) || (aux_fp == NULL) || (warn_fp == NULL)) {
        printf ("Missing or could not open input file\n"); 
        print_usage (); 
        my_exit (-1); 
    }

    // initialization
    int i; 

    // allocate storage
    md = (struct s_carrier *) malloc (sizeof (struct s_carrier) * MD_BUFFER_SIZE);
    if (md==NULL)
        FATAL("Could not allocate storage to read the meta data file in an array%s\n", "")
    mdp = md; 

    // read meta data file into array and sort it
    len_mdfile = 0; 
    read_line (1, md_fp, mdp); // skip header
    while (read_line (0, md_fp, mdp)) {
        len_mdfile++;
        if (len_mdfile == MD_BUFFER_SIZE)
            FATAL ("Meta data array is not large enough to ready the meta data file. Increase MD_BUFFER_SIZE%S\n", "");
        mdp++;
    } // while there are more lines to be read
    if (len_mdfile == 0)
        FATAL("Meta data file is empty%s", "\n")

    // sort by tx_epoch_ms
    sort_by_tx (md, len_mdfile); 
    

    // latency distribution by occupancy table
    for (i=0; i <31; i++) {
        latency_table_by_occ[i].latency = 0;
        latency_table_by_occ[i].count = 0;
    } // latency_table

    // latency spike duration table
    for (i=0; i < MAX_SPIKES; i++)
        lspike_table[i].active = 0;
    len_lspike_table = 0; 
    lspikep = &lspike_table[len_lspike_table]; 

    // occupancy spike duration table
    for (i=0; i < MAX_SPIKES; i++)
        ospike_table[i].active = 0;
    len_ospike_table = 0; 
    ospikep = &ospike_table[len_ospike_table]; 

    last_tx_epoch_ms = 0; 
    last_modem_occ = 0; 
    last_packet_num = 0; 

    // skip header. print header
    emit_aux (1, mdp, aux_fp); 
    emit_output (1, 0, &latency_table_by_occ[0], 0, &lspike_table[0], 0, &ospike_table[0], out_fp); 

    
    // while there is a line to be read from any of the input files
    for (i=0, mdp=md; i<len_mdfile; i++, mdp++) {
//     while (read_line (0, md_fp, mdp)) {
//         len_mdfile++;

        // auxiliary output
        emit_aux (0, mdp, aux_fp);

        // occupancy
        if ((mdp->modem_occ < 0) || (mdp->modem_occ > 31)) {
            printf ("INvalid occupancy %d for packet %d\n", mdp->modem_occ, mdp->packet_num); 
            my_exit (-1);
        }
        if (mdp->modem_occ == 31) // new modem occupancy not available with this packet
            mdp->modem_occ = last_modem_occ; 
        
        latency_table_by_occ[mdp->modem_occ].latency += mdp->rx_epoch_ms - mdp->tx_epoch_ms;
        latency_table_by_occ[mdp->modem_occ].count++; 
         
        // latency spike 
        if (mdp->tx_epoch_ms < last_tx_epoch_ms)
            FATAL("Metadata file not sorted by tx time order at packet %d\n", mdp->packet_num);

        switch (lspikep->active) {
            case 0: // spike inactive
                if (mdp->t2r_latency_ms > latency_threshold) {
                    // start of a spike
                    lspikep->active = 1;
                    lspikep->max_occ = mdp->modem_occ;
                    lspikep->max_latency = mdp->t2r_latency_ms; 
                    lspikep->max_lat_packet_num = mdp->packet_num; 
                    lspikep->stop_packet_num = lspikep->start_packet_num = mdp->packet_num; 
                    lspikep->start_tx_epoch_ms = mdp->tx_epoch_ms; 
                } // start of a spike
                // else stay inactive
                break;
            case 1: // spike active
                // check for continuation 
                if (mdp->t2r_latency_ms > latency_threshold) { // spike continues
                    if (lspikep->max_occ < mdp->modem_occ) 
                        lspikep->max_occ = mdp->modem_occ;
                    if (lspikep->max_latency < mdp->t2r_latency_ms) {
                        lspikep->max_latency = mdp->t2r_latency_ms; 
                        lspikep->max_lat_packet_num = mdp->packet_num; 
                    }
                } // spike continues

                // else spike completed
                else {
                    lspikep->stop_packet_num = last_packet_num; 
                    lspikep->duration_ms = mdp->tx_epoch_ms - lspikep->start_tx_epoch_ms;
                    lspikep->active = 0;
                    len_lspike_table++; 
                    if (len_lspike_table == MAX_SPIKES)
                        FATAL ("Spike table is full. Increase MAX_SPIKE constant %d\n", MAX_SPIKES)
                    lspikep = &lspike_table[len_lspike_table];
                }
                break;
        } // end of latency spike state switch
        
        // occupancy spike 
        switch (ospikep->active) {
            case 0: // spike inactive
                if (mdp->modem_occ > occ_threshold) {
                    // start of a spike
                    ospikep->active = 1;
                    ospikep->stop_packet_num = ospikep->start_packet_num = mdp->packet_num; 
                    ospikep->max_occ = mdp->modem_occ; 
                    ospikep->start_tx_epoch_ms = mdp->tx_epoch_ms; 
                } // start of a spike
                // else stay inactive
                break;
            case 1: // spike active
                // check for continuation of the spike
                if (mdp->modem_occ > occ_threshold) { // spike continues
                    // update spike occ value if occ value increased 
                    if (mdp->modem_occ > ospikep->max_occ) {
                        ospikep->max_occ = mdp->modem_occ; 
                    }
                } // spike continues

                // check for spike completion
                else if (mdp->modem_occ <= occ_threshold) { // spike completed
                    ospikep->stop_packet_num = last_packet_num; 
                    ospikep->duration_ms = mdp->tx_epoch_ms - ospikep->start_tx_epoch_ms;
                    ospikep->active = 0;
                    len_ospike_table++; 
                    if (len_ospike_table == MAX_SPIKES)
                        FATAL ("Spike table is full. Increase MAX_SPIKE constant %d\n", MAX_SPIKES)
                    ospikep = &ospike_table[len_ospike_table];
                }
                break;
        } // end of occupancy spike state switch

        last_tx_epoch_ms = mdp->tx_epoch_ms; 
        last_modem_occ = mdp->modem_occ; 
        last_packet_num = mdp->packet_num; 
        
        if (i % 1000 == 0)
            printf ("Reached line %d\n", i); 
    } // while there are more files to be read

    // if a spike is still active when the file end is reached, then close it.
    if (lspikep->active || ospikep->active) {

        printf ("Spike was active when EOF was reached\n"); 

        // roll back mdp to point to the last data line
        mdp--; 
        
        if (lspikep->active) {
            lspikep->stop_packet_num = mdp->packet_num; 
            lspikep->duration_ms = mdp->tx_epoch_ms - lspikep->start_tx_epoch_ms;
            lspikep->active = 0;
            len_lspike_table++; 
        } // if latency spike active 

        if (ospikep->active) {
            ospikep->stop_packet_num = mdp->packet_num; 
            ospikep->duration_ms = mdp->tx_epoch_ms - ospikep->start_tx_epoch_ms;
            ospikep->active = 0;
            len_ospike_table++; 
        } // if latency spike active 

    } // spike was active when the end of the file was reached

    // print output
    sort_lspike (lspike_table, len_lspike_table);
    sort_ospike (ospike_table, len_ospike_table);
    int num_of_output_lines = MAX(len_ospike_table, MAX(31, len_lspike_table));
    for (i=0; i < num_of_output_lines; i++){
        emit_output (0, i, 
            &latency_table_by_occ[MIN(31-1,i)], 
            len_lspike_table, &lspike_table[i], 
            len_ospike_table, &ospike_table[i], 
            out_fp); 
    } // for all oputput lines

    // close files and check if there are more files to be processed
    fclose (md_fp); fclose(out_fp); fclose(aux_fp); fclose(warn_fp); 
    if (++file_name_index < len_file_name_table)
        goto PROCESS_EACH_FILE;    

    my_exit (0); 
} // end of main
