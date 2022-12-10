#include 	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>

#define		MAX_POP_CHAR	1000				// largest text string in the pop up 
#define 	MAX_OSM			25000				// maximum entries in the OSM file. fatal if there are more.
#define 	MAX_GPS			25000				// maximum entries in the gps file. fatal if there are more.
#define 	MAX_VQ_FILES	50					// maximum number of vq (and gps) files
#define 	MAX_NAME		100					// maximum lenght of the file name
#define 	MAX_LINE_SIZE	1000				// maximum size of a line in any of the files
#define		MATCH			0					// strcmp success output
#define 	ACCEPTABLE_GPS_MATCH_RATIO 0.75		// vq filter points to gps file map ratio. below this generates warning
#define		ACCEPTABLE_OSM_MATCH_RATIO 0.75		// gps coords to osm bin map ratio. below this genrates warning
#define		NEUTRAL_COLOR 	0xC0C0C0			// color of the untraversed bin point
#define		FRAME_COLOR 	0x9933FF			// camera frame position before mapping to bin point
#define		BAD_COLOR		0x990000			// bad coverage at this bin point
#define		GOOD_COLOR		0x009900			// good coverage at this bin point
#define		MIXED_COLOR		0x3333FF			// mixed coverage: some times good, sometimes bad at this bin point
#define		GPS_COLOR		0x00FFFF			// color of the gps file points; will be overwritten by frame colors for the matches
#define		NO_BIN_MATCH_COLOR	0xFF00FF		// color of the frames that did not match the osm points
#define		R_EARTH_KM		6378.1
#define		FATAL(STR, ARG) {printf (STR, ARG); exit(-1);}
#define		WARN(STR, ARG) {if (!silent) printf (STR, ARG);}
#define		IGNORE_FIRST_xxx_FRAMES 100			// first xxx will not be marked defective

struct s_coord {
	double	lon;
	double	lat;
}; 

struct s_window {
	struct s_coord	sw;						// min (lon,lat) south-west point
	struct s_coord	ne;						// max (lon,lat) north-east point
};

// frame characterstics read from the curent vq file
struct s_frame {
	unsigned		num;						// frame number
	double 			epoch_ms; 					// earliest rx time stamp of the frame
	struct s_coord	coord;						// interpolated coordinates based on gps file
	struct s_window	win; 						// window to search osm bin points in
	unsigned		late;						// number of late packets in this frame
	unsigned		missing;					// number of missing packets in this frame
	float			bit_rate;					// bit-rate of the frame
	int				sizeB; 						// frame size in bytes
	unsigned		has_annotation;				// 1 if this frame has user generated annotation
	int				no_fast_channel;			// 1 if there was atleast one packet in this frame with no fast channel
	float			distance;					// distance of the frame from the nearest bin point; invalid if not in search window
	float			speed; 						// interpolated speed of this frame
	int				defective; 					// 1 if this frame is defective
	unsigned		color; 						// BAD_COLOR if defective else GOOD_COLOR
}; 

// osm data based for the target area
struct s_osm {
	struct s_coord	coord;						// OSM bin point coordinate
	unsigned		color;						// color sassigned to this point
	int 			priority;					// 0 highest, -1 if this point has not been assigned to
	char			popup[MAX_POP_CHAR]; 		// pop up text
	int				new_file; 					// 1 if this point has not been updated for the vq/gps file being processed; else 0
} osm[MAX_OSM];
int 				len_osm;					// number of valid entires in osm array

// list of vq filter output and gps files that are to be anlzyed for the specified target area
struct s_vqlist {
	FILE 			*vqfp; 						// vq file
	char			vqname[MAX_NAME]; 			// vq file name
	FILE			*gpsfp;						// corresponding gps file
	char			gpsname[MAX_NAME]; 			// gps file name
	int				priority; 					// priority of this vq file
} vqlist[MAX_VQ_FILES]; 
int 				len_vqlist = 0;				// number of valid entries in the vqlist array

// gps data corresponding to the current vq_filter output file
struct s_gps {
	double 			epoch_ms; 					// time stamp
	int 			mode;						// quality indicator
	struct s_coord	coord; 						// lon, lat of the gps point (at 1Hz)
	float			speed; 						// vehicle speed 
	int				count; 						// number of frames that mapped into this gps coord
} gps[MAX_GPS];
int					len_gps = 0; 				// number of valid entries in gps array

// returns number of entries found in the osm bin file
int read_osm (FILE *);							// reads and error checks osm file

// returns 1 if successfully able to open the vq and gps file 
int add_to_vqlist (
	char *argv[],
	struct s_vqlist *);							// current out index of vqlist; add_to... fills out index+1

// returns number of enteries found in the gps file
int read_gps (FILE *);							// reads and error checks the gps file

// reads a frame from vq filter output file. returns 0 at the end of the file else 1
int get_frame (
	struct s_vqlist *, 						// vq file being read
	struct s_frame 	*); 					// fills out the frame info from the next line

// returns 1 if able to find coordinates for the frame; else 0
int get_gps_coord (struct s_frame *);

// returns index in the osm array closest to frame coord. -1 if could not find anything in serarch_window/2 radius
int find_osm_bin (struct s_frame *);

// calculates the window around the gps coord that needs to be searched in the osm bin file
void calc_search_window (struct s_frame *); 

// returns distance between two geodetic points
double compute_distance (
	struct s_coord *pt1, 
	struct s_coord *pt2); 

// sets the new file flag in the osm array which is used to keep the popup message short
// modifies global osm
void set_new_file_flag (void);

// fills out color, pop up text etc. for the bin point located at osmp using the framep from files pointed to by vqp
void update_osm_bin( 
	struct s_osm *osmp,
	struct s_frame *framep,
	struct s_vqlist *vqp);

// returns 1 if the frame is defective else 0
int is_defective (struct s_frame *); 

// writes out the new osm bin file
void emit_osm ();

void print_usage () {

	char *usage =  "Usage: -osm <file> [-w <search window in m>] [-v] [-a] [-nofc] [-f <dddd>] [-l] [-m] [-b <bit rate d.d Mbps>] [-s] -p 0 <vq file> <gps file> [-p 0/1/2/3 <vq file> <gps file>] -out <file>\n"; 
	printf ("%s\n", usage); 
	printf ("\t-osm: osm bin points file name\n");
	printf ("\t-w: wats will search for bin ponts in w/2 radius from the gps points (which the frames are mapped to). Default w = 40m\n");
	printf ("\t-v: wats will generate debug files and points in the output cvs file. Default off\n"); 
	printf ("\t-a: wats will use annotation column from the vq file for defective frame calculation. Default off\n");
	printf ("\t-0fst: wats will use 0fst column from the vq file for defective frame calculation. Default off\n");
	printf ("\t-f: wats will consider frames below the specified size as defective. Default off\n");
	printf ("\t-l: wats will use late column from the vq file for defective frame calculation. Default off\n");
	printf ("\t-m: wats will use missing column from the vq file for defective frame calculation. Default off\n");
	printf ("\t-b: wats will consider frames with bit rate below this value as defective. Default 0\n");
	printf ("\t-s: wats will run in silent mode and not generate too many console messages. Default off, NOT in silent mode\n"); 
	printf ("\t-p: priority of the vq file\n");
	printf ("\t-out: output file name\n"); 

	return;
} // end of print_usage

void print_header (FILE *fp) {
	int i; 
	
	/*
	// command line arguments
	if (silent) fprintf (fp, "-s ");
	if (include_late) fprintf (fp, "-l " ); 
	if (include_anno) fprintf (fp, "-a " ); 
	fprintf (fp, "-w %0.1f ", search_range);
	fprintf (fp, "-b %0.1f ", -minimum_acceptable_bitrate);
	for (i=0; i < len_vqlist; i++)
		fprintf (fp, "-p%d %s %s ", (vqlist+i)->vqname, (vqlist+i)->gpsname);

	fprintf (fp, "\n"); 
	*/

	// data line header
	fprintf (fp, "lat,lon,color,tooltip\n");

	return;
} // end of print_header

// globals
float 	search_range = 40; 						// search window to map gps coord into osm coords (in m)
float 	minimum_acceptable_bitrate = 0;			// below will be treated as defective
int 	minimum_acceptable_framesize = 0;		// below will be treated as defective
FILE	*outfp=NULL; 							// outfp - output file pointer
int		silent=0; 								// setting to 1 suppresses warning
FILE	*debug_gpsfp, *debug_binfp;				// debug output file
int		verbose = 0; 							// if set to 1, then generate debug output
int		include_late = 0;						// if set to 1, include latency in defective frame calculation
int		include_anno = 0; 						// if set to 1, include annotation in defective frame calculation
int		include_0fst = 0; 						// if set to 1, include 0fst column in defective frame calculation
int		include_missing = 0; 					// if set to 1, include missing column in defective frame calculation

int main (int argc, char *argv[]) {
FILE	*fp; 									// temp file pointer
int i;

	// read and parse arguments
	while (*++argv != NULL) {
		// while there are more parameters to process

		//usage
		if (strcmp (*argv, "--help") == MATCH || strcmp (*argv, "-help") == MATCH) {
			print_usage ();
			exit (0); 

		// osm file
		} else if (strcmp (*argv, "-osm") == MATCH) {
			if ((fp = fopen (*++argv, "r")) == NULL)
				FATAL ("Could not open osm file: %s\n", *argv)
			else 
				len_osm = read_osm (fp); 

		// silent mode
		} else if (strcmp (*argv, "-s") == MATCH) {
			silent = 1; 

		// include missing 
		} else if (strcmp (*argv, "-m") == MATCH) {
			include_missing = 1; 

		// include anno
		} else if (strcmp (*argv, "-a") == MATCH) {
			include_anno = 1; 

		// include anno
		} else if (strcmp (*argv, "-0fst") == MATCH) {
			include_0fst = 1; 

		// incldue latency
		} else if (strcmp (*argv, "-l") == MATCH) {
			include_late = 1; 

		// verbose
		} else if (strcmp (*argv, "-v") == MATCH) {
			verbose = 1; 

		// search window size
		} else if (strcmp (*argv, "-w") == MATCH) {
			if (sscanf (*++argv, "%f", &search_range) != 1)
				FATAL ("-w should be followed by search window. Missing window specification%s\n", "")

		// minimum acceptable bit-rate
		} else if (strcmp (*argv, "-b") == MATCH) {
			if (sscanf (*++argv, "%f", &minimum_acceptable_bitrate) != 1)
				FATAL ("-b should be followed by minimum acceptable bit rate. Missing bit-rate specification%s\n", "")

		// minimum acceptable frame size
		} else if (strcmp (*argv, "-f") == MATCH) {
			if (sscanf (*++argv, "%d", &minimum_acceptable_framesize) != 1)
				FATAL ("-f should be followed by minimum acceptable frame size. Missing frame size specification%s\n", "")

		// priority, vq and gps files
		} else if (strcmp (*argv, "-p") == MATCH) {
			len_vqlist += add_to_vqlist (argv, (vqlist+len_vqlist));
			argv += 3; // for priority, vq file name and gps file name 
			if (len_vqlist > MAX_VQ_FILES)
				FATAL ("Input VQ files exceed maximum permissible (MAX_VQ_FILES) %d\n", len_vqlist)
		
		// output file
		} else if (strcmp (*argv, "-out") == MATCH) {
			if ((outfp = fopen (*++argv, "w")) == NULL)
				FATAL ("Could not open out file: %s\n", *argv)

		// unrecognized argument
		} else 
			FATAL ("invalid argument: %s\n", *argv)
	} // end of while there are more parameters to process

	// check if sufficent number of arguments were supplied
	if (len_osm == 0) FATAL ("Missing osm file%s\n", "")
	if (outfp == NULL) FATAL ("Missing output file specification%s\n", "")
	if (len_vqlist == 0) FATAL ("Missing vq and gps file specificaiton%s\n", "")
	if (search_range <0) FATAL ("Invalid or unspeficied search window %f\n", search_range)

	if (verbose) {
	if ((debug_gpsfp = fopen("debug_gps.csv", "w")) == NULL)
		FATAL ("could not open file: %s\n", "debug_gps.csv") 
	fprintf (debug_gpsfp, "F#,F_epoch_ms,F_coord.lon,F_coord.lat,P_epoch_ms,P_coord.lon,P_coord.lat,N_epoch_ms,N_coord.lon,N_coord_lat,gps_index\n");

	if ((debug_binfp = fopen ("debug_bin.csv", "w")) == NULL)
		FATAL ("could not open file: %s\n", "debug_bin.csv") 
	fprintf (debug_binfp, "F#,F_epoch_ms,F_coord.lon,F_coord.lat,dist(m),F_color,B_coord.lon,B_coord.lat,B_color\n");
	}

	// print header lines
	print_header(outfp); 

	int vqlist_index;
	// for each file in the vq files list
	for (vqlist_index=0; vqlist_index < len_vqlist; vqlist_index++) {
		struct s_frame	frame, *framep = &frame;	// current frame and pointer to it
		int frame_count = 0; 				// count of the frames in this file
		int gps_match_count = 0; 			// count of frames which received coord from gps file 
		int osm_match_count =0; 			// count of frames which found a suitable bin point in osm file
		struct s_vqlist *vqp = vqlist + vqlist_index; 

		// read gps file
		len_gps = read_gps(vqp->gpsfp); // reads the gps file for this vq file into global array gps

		// set new file flag in osm array
		set_new_file_flag (); 

		// for each frame in vq file
		while (get_frame(vqp, framep)) {
			frame_count++; 
			// find the gps coordinate of this frame
			if (get_gps_coord (framep)) {
				gps_match_count++;
				// find the nearest bin point in osm file and create/update status
				int osm_index; 
				if ((osm_index = find_osm_bin (framep)) != -1){
					osm_match_count++; 
					// update the stats of this bin point
					update_osm_bin((osm+osm_index), framep, vqp);
				} // if found osm bin for the frame
			} // if found the gps coordinates of the frame

			if ((frame_count % 1000) == 0) {
				printf ("%s: frame %d\n", vqp->vqname, frame_count); 
			}

		} // while there are more liines in this vq file

		// check % match between vq file with gps and osm files
		float ratio;
		ratio = (float) gps_match_count / (float) frame_count;
		printf ("%.0f%% match between vq file %s and gps file %s\n", ratio*100, vqp->vqname, vqp->gpsname);
		ratio = (float) osm_match_count / (float) frame_count; 
		printf ("%.0f%% match between vq file %s and osm file\n", ratio*100, vqp->vqname);


		if (verbose) {
			fprintf (debug_gpsfp, "gps_index, count, lon, lat\n");
			for (i=0; i < len_gps; i++)
				fprintf (debug_gpsfp, "%d,%d,%lf,%lf\n",
				i, (gps+i)->count, 
				(gps+i)->coord.lon, (gps+i)->coord.lat);
		} // verbose

	} // for each file in the vqlist

	// output new osm bin file
	emit_osm ();
	exit (0);
} // end of main

// returns number of entries found in the osm bin file
// uses global osm
int read_osm (FILE *fp) { // assumes fp has been error checked and is not null
	char	line[MAX_LINE_SIZE], *lp = line; 
	int 	id, bin;					// fields of osm file
	char	name[MAX_NAME];					// fields of osm file
	int		index = -1; 				// osm array index
	double	f1, f2;
	
	// skip header
	if (fgets (lp, MAX_LINE_SIZE, fp) == NULL)
		FATAL ("Empty or incomplete osm file%s\n", "")

	// while there are more lines to be read from the osm file
	while (fgets (lp, MAX_LINE_SIZE, fp) != NULL) {
		struct s_osm *osmp = osm + ++index;

		if (index >= MAX_GPS)
			FATAL ("OSM bin file has more that osm[MAX_OSM] array. Increas MAX_OSM %d \n", index)

		if (sscanf (lp, "%d,%[^,],%d,%lf,%lf", // format 1 for bin csv file
			&id, 
			name,
			&bin,
			&osmp->coord.lon,
			&osmp->coord.lat) != 5)
			// check if it is the second format
			if (sscanf (lp, "%d,,%d,%lf,%lf", // format 1 for bin csv file
			&id, 
			&bin,
			&osmp->coord.lon,
			&osmp->coord.lat) != 4)
				FATAL ("Invalid osm line - missing fields: %s\n", lp)

		// fill in default fields of s_osm
		osmp->color = NEUTRAL_COLOR;
		osmp->priority = -1; 	// default, since no updates yet
		
		// error checking
		if (index == 0) continue; // on the first line; nothing to error check yet
		if (osmp->coord.lon < (osmp-1)->coord.lon) 
			FATAL ("osm file: lon is not in increasing order: %s\n", lp)
	} // while there are more lines to be read from the osm file

	if (index < 0) 
		FATAL ("empty osm file %s\n", "")

	return index+1;
} // end of read_osm

// returns 1 if successfully able to open the vq and gps file 
int add_to_vqlist (char *argv[], struct s_vqlist *vqp) {

	// read priority
	if (sscanf (*++argv, "%d", &vqp->priority) != 1)
		FATAL ("-p argument should be a followed by a number between 0 and 3%s\n", "")
	if (vqp->priority < 0 || vqp->priority > 3)
		FATAL ("invalid priority argument, should bet between 0 and 3: %d", vqp->priority)
	
	// read vq file name
	if ((vqp->vqfp = fopen (*++argv, "r")) == NULL)
		FATAL ("Could not open gps file: %s\n", *argv)
	if (strlen (*argv) > sizeof (vqp->vqname))
		FATAL ("File name %s is too long. Increase MAX_NAME\n", *argv)
	strcpy (vqp->vqname, *argv); 
	
	// read gps file name
	if ((vqp->gpsfp = fopen (*++argv, "r")) == NULL)
		FATAL ("Could not open gps file: %s\n", *argv)
	if (strlen (*argv) > sizeof (vqp->gpsname))
		FATAL ("File name %s is too long. Increase MAX_NAME\n", *argv)
	strcpy (vqp->gpsname, *argv); 

	// get here only if no errors
	return (1); 
} // add_to_vq_list

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
		
		if (verbose) {
			char popup[20];
			sprintf (popup, "%s %d", "gps", index);
			fprintf (outfp, "%lf,%lf,#%06X,%s\n", gpsp->coord.lat, gpsp->coord.lon, GPS_COLOR, popup);
		}

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

// reads a frame from vq filter output file. returns 0 at the end of the file else 1
int get_frame (struct s_vqlist *vqp, struct s_frame *framep) {
	static FILE		*fp = NULL;					// remembers current file to detect a file change
	static int		line_count = 0; 			// number of lines read so far from the file
	char			line[MAX_LINE_SIZE], *lp = line; 

	// check if a new file is being read 
	if ((fp == NULL) || (fp != vqp->vqfp)) {
		line_count = 0;
		fp = vqp->vqfp;
	}

	// skip header
	if (line_count == 0) {
		// header line 1
		if (fgets (lp, MAX_LINE_SIZE, vqp->vqfp) == NULL)
			FATAL ("Empty vq file: %s\n", vqp->vqname)
		// header line 2
		if (fgets (lp, MAX_LINE_SIZE, vqp->vqfp) == NULL)
			FATAL ("Empty vq file: %s\n", vqp->vqname)
	}
	
	// read next line from the file
	if (fgets (lp, MAX_LINE_SIZE, vqp->vqfp) != NULL) {
		line_count++;
		// parse the line
		if (sscanf (lp, 
		 	// format line
			"%u, %lf, %u, %u, %f, %u, %d, %d", 
			&framep->num,
			&framep->epoch_ms,
			&framep->late,
			&framep->missing,
			&framep->bit_rate,
			&framep->has_annotation,
			&framep->no_fast_channel,
			&framep->sizeB) != 8) {
				printf ("Invalid vq format in file %s at line %d: %s\n", vqp->vqname, line_count, lp);
				exit (-1);
			} // if scan did not succeed
		else { 
			framep->defective = is_defective (framep);
			framep->color = framep->defective? BAD_COLOR : GOOD_COLOR;
			return 1;
		} // scan succeeded
	} // if were able to read a line from the file

	// get here at the end of the file
	if (line_count == 0) // the file does not have any lines
		FATAL ("Empty vq file: %s\n", vqp->vqname)
	return 0;
} // end of get_frame

// returns 1 if able to find coordinates for the frame; else 0. Assumes gps file is in increasing time order
// uses globals gps and len_gps
int get_gps_coord (struct s_frame *framep) {
	int i;
	struct s_gps	*prev;
	struct s_gps	*next;

	// check if the frame time is in the range
	if ((framep->epoch_ms < gps->epoch_ms) || (framep->epoch_ms > (gps+len_gps-1)->epoch_ms)) {
		if (verbose) {
			fprintf (debug_gpsfp, "%d,%.0lf,%lf,%lf,%.0lf,%lf,%lf,%0lf,%lf,%lf,%s\n", 
			framep->num, 
			framep->epoch_ms, framep->coord.lon, framep->coord.lat, 
			gps->epoch_ms, gps->coord.lon, gps->coord.lat,
			(gps+len_gps-1)->epoch_ms, (gps+len_gps-1)->coord.lon, (gps+len_gps-1)->coord.lat,
			"OUT OF RANGE"); 
		} // verbose
		return 0;
	} // frame timestamp deos not map into the gps file range

	// find the closest before timestamp in gps array (linear search now should be replaced with binary)
	for (i=0; i<len_gps; i++) {
		if ((gps+i)->epoch_ms > framep->epoch_ms)
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
		double dt = (framep->epoch_ms - prev->epoch_ms)/(next->epoch_ms - prev->epoch_ms);
		framep->coord.lat = (1-dt)*prev->coord.lat + (dt * next->coord.lat);
		framep->coord.lon = (1-dt)*prev->coord.lon + (dt * next->coord.lon); 
		framep->speed = (1-dt)*prev->speed + (dt * next->speed); 
	} // interpolate

	// record how frames mapped into gps coordinates for debugging
	prev->count++;

	if (verbose) {
		fprintf (debug_gpsfp, "%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d\n", 
		framep->num, 
		framep->epoch_ms, framep->coord.lon, framep->coord.lat, 
		prev->epoch_ms, prev->coord.lon, prev->coord.lat,
		next->epoch_ms, next->coord.lon, next->coord.lat, 
		i); 
	
	 	/* not necessary to print this as we will print the points that don't map into osm.
		char popup[20];
		sprintf (popup, "F_%d %4.1fMbps %4.1fmph", framep->num, framep->bit_rate, framep->speed);
		fprintf (outfp, "%lf,%lf,#%06X,%s\n", framep->coord.lat, framep->coord.lon, FRAME_COLOR, popup);
		*/
	} // verbose

		return 1;
} // end of get_gps_coord

// calculates the window around the gps coord that needs to be searched in the osm bin file
void calc_search_window (struct s_frame *p) {
	// dlon = window/(R*cos(lat)) * 180 / pi
	// dlat = (window/R) * 180 / pi
	double dlon = fabs((search_range / (R_EARTH_KM * 1000 * cos (p->coord.lat*M_PI/180))) * 180 / M_PI); 
	double dlat = fabs((search_range / (R_EARTH_KM * 1000)) * 180 / M_PI); 
	p->win.sw.lat = p->coord.lat - dlat/2;
	p->win.sw.lon = p->coord.lon - dlon/2;
	p->win.ne.lat = p->coord.lat + dlat/2;
	p->win.ne.lon = p->coord.lon + dlon/2;
	return; 
} // end of calc_search_window

// returns index in the osm array closest to frame coord. -1 if could not find anything in serarch_window/2 radius
// uses global osm and len_osm
int find_osm_bin (struct s_frame *framep) {
	float min_distance = search_range/2;
	int i, min_index = -1;
	struct s_osm *osmp;

	calc_search_window (framep); 

	// look for osm bin poits in the search window
	for (i=0; i<len_osm; i++) { // replace with more efficient search that traversers only the relevant portion of osm array
		osmp = osm+i;
		/* this is a more compute efficient version. Commented out for debugging
		if (osmp->coord.lon >= framep->win.sw.lon) // this osm bin east of lon window start
			if (osmp->coord.lon <= framep->win.ne.lon) // this osm bin west of lon window end
				// in the lon window range; now check lat
				if (osmp->coord.lat >= framep->win.sw.lat) // north of lat window start
					if (osmp->coord.lat <= framep->win.ne.lat) { // south of lat window end
						// osm bin point in the search window
						double distance = compute_distance (&framep->coord, &osmp->coord); 
						if (min_distance > distance) {
							// found a closer bin point  
							min_distance = distance;
							min_index = i; 
						} // found a closer bin point
					} // found a osm bin point within the search window
		*/
		double distance = compute_distance (&framep->coord, &osmp->coord); 
		if (min_distance > distance) {
			// found a closer bin point  
			min_distance = distance;
			min_index = i; 
		} // found a closer bin point
	} // for all points in the osm bin file

	// note down the distance for debugging purposes
	framep->distance = min_distance; 

	if (verbose) {
		if (min_index == -1) { // did not find a bin that maps to the frame
			char buffer[100]; 
			static int no_match_count = 0;
			sprintf (buffer, "NO MATCH %d", no_match_count++);
			fprintf (debug_binfp, "%d,%lf,%lf,%lf,%s,%d,#%06X\n", 
				framep->num, framep->epoch_ms, framep->coord.lon, framep->coord.lat, "NO_MATCH", no_match_count, framep->color); 
			fprintf (outfp, "%lf,%lf,#%06X,%s\n", framep->coord.lat, framep->coord.lon, NO_BIN_MATCH_COLOR, buffer);
		}
		else // found a mapping to a bin
			fprintf (debug_binfp, "%d,%lf,%lf,%lf,%.1f,#%06X,%lf,%lf,#%06X\n", 
				framep->num, framep->epoch_ms, framep->coord.lon, framep->coord.lat, framep->distance, framep->color, 
				(osm+min_index)->coord.lon, (osm+min_index)->coord.lat, (osm+min_index)->color);

	} // verbose

	return min_index; // will be -1 if did not find a bin point closer than the search window
} // end of find_osm_bin

// returns distance between two geodetic points
double compute_distance ( struct s_coord *pt1, struct s_coord *pt2)  {
	// dx = R * cos (lat) * dlon
	// dv = R * dlat 
	// dist = sqrt (dx^2 + dy^2) - can skip multiply by R once the code is working
	double dlon = pt1->lon - pt2->lon; 
	double dlat = pt1->lat - pt2->lat; 
	double dx = R_EARTH_KM * 1000 * cos (pt1->lat * M_PI / 180) * (dlon * M_PI / 180); 
	double dy = R_EARTH_KM * 1000 * (dlat * M_PI / 180); 
	return sqrt (dx*dx + dy*dy); 
} // end of compute_distance 

// fills out color, pop up text etc. for the bin point located at osmp using the framep from files pointed to by vqp
void update_osm_bin(struct s_osm *osmp, struct s_frame *framep, struct s_vqlist *vqp) {
	char			s_color[10];  			// color assigned to the poput
	char			s_popup[5000]; 			// length of the popu string
	int				does_not_fit = 0; 		// 1 if the popup message exceeds the length of the popup string
	int				higher_priority = (osmp->priority == -1) /* not assigned yet */ || 
						(vqp->priority < osmp->priority); /* lower number is higher priority */

	// popup message string 
	if (framep->defective) 
		strcpy (s_color, "B");
	else 
		strcpy (s_color, "G");

	// print vq file nmae if new vq file
	if ((osmp->new_file)) { // the osm bin point has not been tranversed by this vqfile; so print vq file name in popup message
		sprintf (s_popup, "VQ:%s GPS:%s F#:%d %s ", vqp->vqname, vqp->gpsname, framep->num, s_color);
		osmp->new_file = 0;
	} // if new vqfile
	else // do not print the file names again to save space. 
		sprintf (s_popup, "F#:%d %s ", framep->num, s_color);

	if (verbose) {
		char buffer[100]; 
		sprintf (buffer, "%.1fm ", framep->distance); // print distance from the bin point
		strcat (s_popup, buffer);
	}

	if (does_not_fit = (strlen(s_popup) > /* remaining space in popup storage */ (sizeof(osmp->popup)-(strlen(osmp->popup)-1))))
		WARN ("Warning: Message does not fit into popup storage size MAX_POP_CHAR: %s. Discarding\n", s_popup)

	// if this update is higher priority then replace previous
	if (higher_priority) { //  
		osmp->color = framep->color;
		osmp->priority = vqp->priority; 
		if (!does_not_fit) 
			sprintf (osmp->popup, "%s", s_popup);
	} 
	// else if this update is the same priority as existing update then fuse
	else if (vqp->priority == osmp->priority) {
		if (framep->color != osmp->color)
		osmp->color = MIXED_COLOR; 
		if (!does_not_fit)
			strcat (osmp->popup, s_popup);
	}
	// else this update is lower priority than existing, then do nothing 
	
	return; 
} // upate_osm_bin

// returns 1 if the frame is defective else 0
int is_defective (struct s_frame *framep) {
	if (framep->num < IGNORE_FIRST_xxx_FRAMES)
		return 0;
	else 
		return 
			include_late * (framep->late !=0) || 
			include_missing * (framep->missing !=0) || 
			(framep->bit_rate < minimum_acceptable_bitrate) || 
			(framep->sizeB < minimum_acceptable_framesize) || 
			include_anno * framep->has_annotation ||
			include_0fst * framep->no_fast_channel; 
} // is_defective 

// writes out the new osm bin file
// uses global osm, len_osm, modifies outfp
void emit_osm () {
	int i; 

	// print data
	for (i=0; i<len_osm; i++) {
		struct s_osm	*osmp = osm + i;
		if (osmp->priority == -1) // no update for this bin point
			fprintf (outfp, "%lf,%lf,#%06X,%s\n", osmp->coord.lat, osmp->coord.lon, NEUTRAL_COLOR, "Not traversed");
		else 
			fprintf (outfp, "%lf,%lf,#%06X,%s\n", osmp->coord.lat, osmp->coord.lon, osmp->color, osmp->popup);
	} // for all lines

	return; 
} // emit_osm

// sets the new file flag in the osm array which is used to keep the popup message short
// modifies global osm
void set_new_file_flag (void) {
	int i; 
	for (i=0; i<len_osm; i++)
		(osm+i)->new_file = 1;
	return;
} // end of set_new_file_flag 
