import sys
import math
import time
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy

#########################################################################################
# Global constants
#########################################################################################
FRAME_PERIOD = 33.33

#########################################################################################
# field definitions
#########################################################################################

file_list_fields = namedtuple ("file_list_fields", "in_dir, rx_infix, tx_infix")

files_dic_fields = namedtuple ("files_dic_fields", "filename, fields")

# uplink_queue. ch: 1, timestamp: 1681947064182, queue_size: 316, elapsed_time_since_last_queue_update: 9, actual_rate: 0
uplink_fields = namedtuple ("uplink_fields", "channel, queue_size_sample_TS, queue_size, elapsed_time_since_last_queue_update, actual_rate")

# latency log
# ch: 0, received a latency, numCHOut:2, packetNum: 4294967295, latency: 40, time: 1681947064236, sent from ch: 0
# receive_TS is the time when the back propagated t2r info is received by the vehicle
latency_fields = namedtuple ("latency_fields", "communicating_channel, numCHOut, PktNum, bp_t2r, bp_t2r_receive_TS, sending_channel")

# service log
# CH: 2, change to out-of-service state, latency: 0, latencyTime: 0, estimated latency: 2614851439, stop_sending flag: 0 , uplink queue size: 0, zeroUplinkQueue: 0, service flag: 0, numCHOut: 1, Time: 1681947064175, packetNum: 0
service_fields = namedtuple ("service_fields", "channel, bp_t2r, bp_t2r_receive_TS, est_t2r, stop_sending_flag, \
                             uplink_queue_size, zeroUplinkQueue, service_flag, numCHOut, service_transition_TS, bp_t2r_packetNum")

# skip decision log
skip_fields = namedtuple ("skip_fields", 
                             # CH: 1,  Method: 0, t2r: 0, lastPacketNumber to throwaway: 0, qsize: 313, framePacketNum: 0,  lastRetiredCH: 2, lastPacketRetired: 0, lastPacketRetiredTIme: 0, lastPacketSearchTime: xxx, i
                             # ch0 service: 0, ch1 service: 1, ch2 service: 1, ch0In: 0, ch1In : 1, ch2In: 1, ch0Matched:1, ch1Matched:1, ch2Matched:0, i
                             # ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1685999169730, ch1InTime: 1685999171821, ch2InTime:1685999169730, time: 1685999171821, skip: 0, firstPacketinQ: 0
                              "ch,     method,    t2r,    last_skip_pkt_num,                qsize,       szP,               lrp_channel,       lrp_num,               lrp_bp_TS,               last_skip_tx_TS, \
                               ch0_IS_now,    ch1_IS_now,      ch2_IS_now,    ch0_IS,    ch1_IS,     ch2_IS,   ch0Matched,  ch1Matched,    ch2Matched, \
                               ch0_x,    ch1_x,    ch2_x,   ch0_IS_x_TS,             ch1_IS_x_TS,              ch2_IS_x_TS,              resume_TS,        skip,    first_pkt_in_Q, \
                               ch0_lrp, ch1_lrp, ch2_lrp, ch0_lrp_bp_ts, ch1_lrp_bp_ts, ch2_lrp_bp_ts")
    
# bitrate log
# send_bitrate: 830000, encoder state: 2, ch0 quality state: 1, ch1 quality state: 1, ch2 quality state: 1, time: 1681947065306
brm_fields = namedtuple ("brm_fields", "send_bitrate, encoder_state, ch0_quality_state, ch1_quality_state, ch2_quality_state, TS")

# avgQ log
# RollingAvg75. Probe. CH: 2, RollingAvg75: 0.000000, qualityState: 1, queue size: 0, time: 1681947064175
# avgq_fields 
    
# retx log
# ch: 2, received a retx, numCHOut:2, startPacketNum: 39753, run: 1, time: 1681946093091
    
# probe log
# ch: 0, receive_a_probe_packet. sendTime: 1681946022261, latency: 45, receivedTime: 1681946022306
probe_fields = namedtuple ("probe_fields", "sending_channel, send_TS, latency, receive_TS")

# chq log
# CH: 2, chQ:0, qSize: 0, est_bpt2r: 30, ch_inservice: 1, serviceStateTransitionTime: 2042, InGoodQualityTime: 1688092037043, qTranTime: 1688092037043, time: 1688092037043
# CH: 2, chQ:0, qSize: 0, est_bpt2r: 30, ch_inservice: 1, serviceStateTransitionTime: 2028, InGoodQualityTime: 1689024682615, qTranTime: 1689024682615, time: 1689024682615, ptime: 1689024680615, stage: 0
chq_fields = namedtuple ("chq_fields", "ch, channel_quality, qsize, est_t2r, IS, ms_since_IS, good_quality_x_TS, ignore, TS, ptime, stage")

# carrier csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 chPacketNum
# 0	             8.62473E+14	     1.68452E+12	     1384	              1	              0	             0	          0	                 0	         1.68452E+12	     0	      0
chrx_fields = namedtuple ("chrx_fields", "pkt_num, tx_TS, rx_TS, pkt_len, frame_start, frame_number, frame_rate, frame_res, frame_end, camera_TS, retx, chk_pkt")

# dedup csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 ch	 latency
# 0	             8.61156E+14	     1.68195E+12	     1384	             1	             0	             0	         0	                 0	         1.68195E+12	     0	     2	    37
dedup_fields = namedtuple ("dedup_fields", "pkt_num, tx_TS, rx_TS, pkt_len, frame_start, frame_number, frame_rate, frame_res, frame_end, camera_TS, retx, ch, latency")

frame_sz_list_fields = namedtuple ("frame_sz_list_fields", "frame_num, frame_szP")

#########################################################################################
# function definitions
#########################################################################################
def WARN (s):
    " prints warning string on stderr"
    sys.stderr.write (s)
    # exit ()
    return

def FATAL (s):
    " prints error string on stderr and exits"
    sys.stderr.write (s)
    exit ()
    return

def robust_split (separator, str_s):
    """ returns a list of fields found int str_s stripped of leading/trailing blanks, [,],{,},(,).
        tolerates leading separator, separtors with no fields in between, or no separators at all in which case
        str_s after stripping leading and trailing white space or tabs
        """
    split_list = []
    str_s = str_s.strip (" \t")
    if not separator in str_s:
        return [str_s]
    for field in str_s.split(separator):
        field = field.strip (" \t[]{}()")
        if not len (field):
            continue
        split_list += [field]
    return split_list
# end of robust_split

def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False
# end of is_number

class CsvOut:

    def __init__(self, fout):
        self.fout = fout
        self.header_printed_already = 0
        self.found_number_last = 0
        self.found_string_last = 0
        self.output_list = []
        self.header_list = []

    def write (self, str_s):

        for field in robust_split (",", str_s):
            if field == "\n": # generate output
                # if len(self.header_list) != len(self.output_list):
                    # WARN ("missing a header or value field") 
                if not self.header_printed_already:
                    self.fout.write (",".join (self.header_list) + "," + "\n")
                    self.header_printed_already = 1
                self.fout.write (",".join (self.output_list) + "," + "\n")
                self.header_list = []
                self.output_list = []
                self.found_number_last = 0
                self.found_string_last = 0
                return
            # if found the end of line

            if not is_number (field): # found key
                self.header_list += [field]
                self.found_number_last = 0
                if self.found_string_last: # multiple strings i.e. values masquarading as keys; copy them into output list
                    if self.found_string_last == 1: # add previous "key" to value list
                        self.output_list += [self.header_list[-2]]
                    self.output_list += [field]
                self.found_string_last += 1
            else: # found value; can have multiple values for the same key in case the argument was a list etc.
                self.output_list += [field]
                self.found_string_last = 0
                if self.found_number_last: # multiple values for the same key. repeat the last key in header line
                    if self.found_number_last == 1: # append 0 to the last header
                        self.header_list[-1] += str(0)
                    self.header_list += [self.header_list[-1][0:-1] + str(self.found_number_last)]
                self.found_number_last += 1
        # for all fields
        return
    # end of write

    def close (self):
        self.fout.close ()
        return
# end of CsvOut

def read_worklist (file_name):
    """ returns a list of file_list_fields tuples by reading file_name """

    work_list = []
    srx_defined = False
    stx_defined = False
    ipath_defined = False
    comment_nest_count = 0

    input_file = open (file_name, "r")

    for i, line in enumerate(input_file):
    
        line = line.strip ()
        
        # skip lines in comment blocks
        if line.startswith ("/*"):
            comment_nest_count += 1
            continue
        elif line.startswith ("*/"):
            comment_nest_count -= 1
            continue
        elif comment_nest_count > 0:
            continue
        elif comment_nest_count < 0:
            FATAL ("FATAL: Incorrectly nested comments start line: ", + str(i))
    
        # skip comment lines (outside comment block) and empty lines
        if line.startswith ("//") or line.startswith ("#") or line == "":
            continue
    
        # parse a non-comment line 
        line_tokens = line.strip().split('"')
        try: 
            # print ("line #:", i, "0:",line_tokens[0], "1:", line_tokens[1], "2:", line_tokens[2], "3:", line_tokens[3])
            if line_tokens[1] == "-srx_pre": 
                # print ("srx:", line_tokens[3])
                srx = line_tokens[3]
                srx_defined = True
            elif line_tokens[1] == "-stx_pre":
                # print ("stx:", line_tokens[3])
                stx = line_tokens[3]
                stx_defined = True
            elif line_tokens[1] == "-ipath":
                # print ("ipath:", line_tokens[3])
                ipath = line_tokens[3]
                ipath_defined = True
            else:
                WARN (f"Invalid syntax at line: {str(i)} {line_tokens[1]}. Ignoring\n")
            
            if stx_defined and srx_defined and ipath_defined:
                work_list += [file_list_fields (in_dir=ipath, rx_infix=srx, tx_infix=stx)]
                srx_defined = False 
                stx_defined = False
        except:
            FATAL ("FATAL read_worklist: Invalid syntax at line {n}: {l} in file {f}".format (n=i, l=line, f=input_file))
        # end of parse a non-comment line
    # for all lines in the input_file
    return work_list
# end of read_worklist

def add_to_dic (key, file_name, tuple_fields, array, files_dic, log_dic ):
    "adds an element to the file attribute and log dictionary if the specified file exists"
    path = Path (file_name)
    if path.is_file ():
        files_dic.update ({key: files_dic_fields._make ([file_name, tuple_fields])})
        log_dic.update ({key: array})

def create_dic (in_dir, tx_infix, rx_infix):
    " creates file attribute dictionary and corresponding entry in log dictionary"
    
    files_dic = {}
    log_dic = {}

    # log files
    uplink_array = []
    add_to_dic (key="uplink", file_name=in_dir+"uplink_queue_"+tx_infix+".log", tuple_fields = uplink_fields, array=uplink_array, files_dic=files_dic, log_dic=log_dic)
    
    all_latency_array = []
    add_to_dic (key="all_latency", file_name=in_dir+"latency_"+tx_infix+".log", tuple_fields=latency_fields, array=all_latency_array, files_dic=files_dic, log_dic=log_dic)
    
    service_array = []
    add_to_dic (key="service", file_name=in_dir+"service_"+tx_infix+".log", tuple_fields=service_fields, array=service_array, files_dic=files_dic, log_dic=log_dic)
    
    skip_array = []
    add_to_dic (key="skip", file_name=in_dir+"skip_decision_"+tx_infix+".log", tuple_fields=skip_fields, array=skip_array, files_dic=files_dic, log_dic=log_dic)
    
    brm_array = []
    add_to_dic (key="brm", file_name=in_dir+"bitrate_"+tx_infix+".log", tuple_fields=brm_fields, array=brm_array, files_dic=files_dic, log_dic=log_dic)

    chq_array = []
    add_to_dic (key="chq", file_name=in_dir+"chQ_"+tx_infix+".log", tuple_fields=chq_fields, files_dic=files_dic, array=chq_array, log_dic=log_dic)

    # csv files
    for i in range (3):
        chrx_array = []
        add_to_dic (key="chrx"+str(i), file_name=in_dir+rx_infix+"_ch"+ str(i) + ".csv", tuple_fields=chrx_fields, array=chrx_array, files_dic=files_dic, log_dic=log_dic)
    dedup_array = []
    add_to_dic (key="dedup", file_name=in_dir+rx_infix+".csv", tuple_fields=dedup_fields, array=dedup_array, files_dic=files_dic, log_dic=log_dic)

    return files_dic, log_dic
# end of create_dic

def read_files (files_dic, log_dic):
    " reads all the log/csv file specified in files dictionary and stores them in the log dictonary"
    TX_TS_INDEX = 1
    CAMERA_TS_INDEX = 9
    # read all the log and csv files
    for item in files_dic:
        print ("reading file: ", files_dic[item].filename)
        if item == "dedup" or item.startswith("chrx"):
            log_dic[item] = read_csv_file (files_dic[item].filename, files_dic[item].fields, TX_TS_INDEX, CAMERA_TS_INDEX)
        else:
            log_dic[item] = read_log_file (files_dic[item].filename, files_dic[item].fields)
        if (log_dic[item] != None):
            print ("\t file length = {}".format (len(log_dic[item])))
    return
    
def schedule (lst, new):
    """ schedules a new entry at the right place in the lst using TS.
        assumes lst of named tuples with TS field """
    if len (lst) == 0:
        lst.insert(0, new)
        return
    for i, line in enumerate(lst):
        if new.TS < line.TS:
            lst.insert (i, new)
            return
    lst.insert (len(lst), new)
    return
# end of schedule

def unschedule (lst, id, reason):
    """ removes the first entry that matches the id and the reason.
        assumes the lst of namedtuples with id and reason fields.
        returns -1 if no removal, else 0"""
    for i, line in enumerate (lst):
        if lst[i].id == id and lst[i].reason == reason:
            del lst[i]
            return 0
    return -1
# end of unschedule

def read_log_file (filename, tuplename):
    """ returns an array (list) of specified namedtuples from the data in the filename"""

    path = Path(filename)
    if path.is_file () == False: 
        print ("Could not find file " + filename)
        exit ()

    array = []
    file = open (filename, "r")
    warnings_count = 0
    for line_num, line in enumerate (file):
        field_list = []
        for fields in line.split(","):
            try: # collect all integer values following ":"
                field_list += [(int(fields.split(":")[1]))]
            except: # skip if no numerical value following ":"
                pass

        try: 
            array += [tuplename._make(field_list)]
        except:
            WARN (f"WARNING read_log_file: incorrect number of filelds: {filename} Line {line_num}: {line} \n")
            warnings_count += 1
            if warnings_count >= 10: exit ()
        if line_num % 100_000 == 0:
            print (f"at line {line_num}")
    return array
# end of read_log_file

def read_csv_file(filename, tuplename, tx_TS_index, camera_TS_index):
    """ returns an array (list) of specified namedtuples from the data in the filename"""

    path = Path(filename)
    if path.is_file () == False: 
        print ("Could not find file " + filename)
        exit ()

    array = []
    file = open (filename, "r")
    for line_num, line in enumerate (file):

        if line_num == 0: # skip header
            continue

        field_list = []
        for fields in line.split(","):
            try: # collect all integer values following ":"
                field_list += [int(fields)]
            except: # skip if no numerical value following ":"
                pass

        # fix the tx time and camera TS
        field_list[tx_TS_index] = int (field_list[tx_TS_index]/512)
        if field_list[camera_TS_index]==0 and len (array) != 0 : field_list[camera_TS_index] = array[-1].camera_TS 

        try: 
            array += [tuplename._make(field_list)]
        except:
            WARN ("WARNING read_csv_file: incorrect number of filelds: " + filename  + " Line " + str(line_num) + ": " + " ".join (str(e) for e in field_list) +"\n")

    return array
# end of read_csv_file

def create_self_bp_list (log_dic):
    " creates latency list that retains lines with same sending (originating) and communicating channel and adds it to log_dic"

    print ("Removing unnecessary lines from the latency file")
    print ("Original latency file length: {}".format (len(log_dic["all_latency"])))

    short_list = []
    for i, line in enumerate (log_dic["all_latency"]):
        if line.communicating_channel == line.sending_channel: 
            short_list += [(line)]
    log_dic.update ({"latency": short_list})
    print ("After removing unnecessary lines, latency file lenght: {}".format (len(log_dic["latency"])))
    return
# end of create_self_bp_list

def create_max_bp_list (log_dic):
    "creates a list of max bp pkt num received up to each line in the latency file and adds it to the latency file"

    max_bp_pkt_num_list = [0]
    for i, line in enumerate (log_dic["all_latency"]):
        if i:
            max_bp_pkt_num_list += [max(line.PktNum, max_bp_pkt_num_list[i-1])] if (line.PktNum != 4294967295) else [max_bp_pkt_num_list[i-1]] 
    log_dic.update ({"max_bp_pkt_num": max_bp_pkt_num_list})
    return
# end of create_latency_aux_lists
    
def create_frame_szP_list (log_dic):
    " creates an  array indexable by packet number that returns frame number and frame size in packets"
    frame_sz_list = []
    frame_num = 0
    frame_szP = 0
    for i, line in enumerate (log_dic["dedup"]):
        if i and line.frame_start:
            for j in range(frame_szP):
                frame_sz_list += [frame_sz_list_fields(frame_num, frame_szP)]
            frame_szP = 1
            frame_num += 1
        else:
            frame_szP += 1
# end of create_frmae_szP_list
    
def remove_network_duplicates (log_dic):
    "removes duplicates introduced by the network"
    chrx_a_sorted_by_pkt_num = []
    for i in range(3):
        chrx_a = log_dic["chrx"+str(i)]
        chrx_a.sort (key = lambda a: a.tx_TS) 
        chrx_a_sorted_by_pkt_num = sorted (chrx_a, key = lambda a: a.pkt_num)
        count = 0
        for j, line in enumerate (chrx_a):
            if j+1 < len(chrx_a):
                if line.retx == chrx_a[j+1].retx and line.pkt_num == chrx_a[j+1].pkt_num: # network introduce duplicates
                   del chrx_a[j] 
                   count += 1
        if (count): 
            array_name = "chrx"+str(i)
            print ("remvoved {n} duplicates from {s} metadata array".format (n=count, s=array_name))
        log_dic.update ({"chrx_sorted_by_pkt_num"+str(i): chrx_a_sorted_by_pkt_num})
    return
# end of remove_network_duplicates

def create_chrx_sorted_by_pkt_num (log_dic):
    "creates chrx sorted by pkt num (as opposed to tx_TS) and adds them to log_dic"
    chrx_a_sorted_by_pkt_num = []
    for i in range(3):
        chrx_a = log_dic["chrx"+str(i)]
        chrx_a.sort (key = lambda a: a.tx_TS) 
        chrx_a_sorted_by_pkt_num = sorted (chrx_a, key = lambda a: a.pkt_num)
        log_dic.update ({"chrx_sorted_by_pkt_num"+str(i): chrx_a_sorted_by_pkt_num})
    return
# end of remove_network_duplicates

def open_output_files (file_name_prefix):
    "Opens output csv file and returns CsvOut object containing it. Redirects stderr to warning file"
    fout = CsvOut (open (file_name_prefix + ".csv", "w"))
    sys.stderr = open (file_name_prefix + "_warnings"+".log", "w")
    return fout

def close_output_files (fout):
    "closes file contained in CsvOut object fout and warning file. Redirects stderr to standard error"
    fout.close ()
    sys.stderr.close ()
    sys.stderr = sys.__stderr__
    return

def spike_analysis (log_dic, out_dir, capture):
    "performance spike analysis - mostly requested vs delivered bandwidth. outputs spike_ file"

    print ("Running spike analysis on file {f}".format (f=capture.tx_infix))
    fout = open_output_files (out_dir + "spike_" + capture.tx_infix)
    
    service_start_index = 0
    itr_count = 0
    tx_rx_bandwidth = [0]*3
    rx_rx_bandwidth = [0]*3
    sd_bandwidth = [0]*3
    
    while service_start_index < len(log_dic["service"]):
    
        # service start
        service_start_line = log_dic["service"][service_start_index]
        if (service_start_line.service_flag == 0): # not going into serivce, so ignore
            service_start_index += 1
            continue
        chrx_a = log_dic["chrx"+str (service_start_line.channel)]
        dedup_a = log_dic["dedup"]

        # service stop
        service_stop_index = service_start_index + 1
        while service_stop_index < len (log_dic["service"]):
           service_stop_line = log_dic["service"][service_stop_index]
           if service_stop_line.channel == service_start_line.channel and service_stop_line.service_flag == 0: break
           else: service_stop_index += 1
        else: 
            WARN ("Could not find service stop corresponding to service start of channel {c} at {t}".format(
                c=service_start_line.channel, t=service_start_line.service_transition_TS))
            service_start_index += 1
            continue
        
        # first and last packet during this service duration
        first_pkt_index = bisect_left (chrx_a, service_start_line.service_transition_TS, key = lambda a: a.tx_TS)
        if first_pkt_index >= len (chrx_a): 
            WARN ("Could not find first pkt after service start of channel {c} at {t}".format(
                 c=service_start_line.channel, t=service_start_line.service_transition_TS))
            service_start_index += 1
            continue
        last_pkt_index = bisect_left (chrx_a, service_stop_line.service_transition_TS, key = lambda a: a.tx_TS) - 1
        if last_pkt_index >= len (chrx_a) or last_pkt_index < 0: 
            WARN ("Could not find last pkt after service stop of channel {c} at {t}".format(
                c=service_start_line.channel, t=service_stop_line.service_transition_TS))
            service_start_index += 1
            continue

        # transmitted packets, useful packets, retx packets, bytes
        tx_pkts = last_pkt_index - first_pkt_index + 1
        tx_bytes = 0
        tx_retx = 0
        useful_tx_pkts = 0
        for chrx_line in chrx_a[first_pkt_index : last_pkt_index + 1]:
            tx_bytes += chrx_line.pkt_len
            tx_retx += chrx_line.retx
            dedup_index = bisect_left (dedup_a, chrx_line.pkt_num, key = lambda a: a.pkt_num)
            if dedup_a[dedup_index].pkt_num != chrx_line.pkt_num: # should not happen
                WARN ("Could not find packet {p} of channel {c} in dedup".format (
                    p=chrx_line.pkt_num, c=service_start_line.channel))
                continue
            if dedup_a[dedup_index].ch == service_start_line.channel: useful_tx_pkts += 1 
        try: percent_useful_pkts = (useful_tx_pkts / tx_pkts)
        except: percent_useful_pkts = 0
        last_pkt_t2r = chrx_a[last_pkt_index].rx_TS - chrx_a[last_pkt_index].tx_TS

        # delivered bandwidth
        i = service_start_line.channel
        tx_rx_duration = chrx_a[last_pkt_index].rx_TS - chrx_a[first_pkt_index].tx_TS
        rx_rx_duration = chrx_a[last_pkt_index].rx_TS - chrx_a[first_pkt_index].rx_TS
        service_duration = service_stop_line.service_transition_TS - service_start_line.service_transition_TS
        try: tx_rx_bandwidth[i] = tx_bytes * 8 / tx_rx_duration / 1000 
        except: tx_rx_bandwidth[i] = 0
        try: rx_rx_bandwidth[i] = tx_bytes * 8 / rx_rx_duration / 1000 
        except: rx_rx_bandwidth[i] = 0
        try: sd_bandwidth[i] = tx_bytes * 8 / service_duration / 1000 
        except: sd_bandwidth[i] = 0

        # requested bandwidth
        # if service_start_line.service_transition_TS == 1686705460014:
            # print ("debug")
        camera_start_index = min (len (dedup_a) - 1, bisect_left (dedup_a, service_start_line.service_transition_TS, key = lambda a: a.camera_TS))
        camera_stop_index = min (len (dedup_a) - 1, bisect_left (dedup_a, service_stop_line.service_transition_TS, key = lambda a: a.camera_TS))
        camera_bytes = 0
        index = camera_start_index
        while index < len (dedup_a) and (\
            dedup_a[index].camera_TS < dedup_a[camera_stop_index].camera_TS or \
            (camera_start_index == camera_stop_index and dedup_a[index].camera_TS == dedup_a[camera_stop_index].camera_TS)): 
            camera_bytes += dedup_a[index].pkt_len
            index += 1
        try: rq_bandwidth = camera_bytes * 8 / max (FRAME_PERIOD, service_duration) / 1000
        except: rq_bandwidth = 0
        
        # Q size at service start
        start_skip_index = bisect_left (log_dic["skip"], service_start_line.service_transition_TS, key = lambda a: a.resume_TS)
        while start_skip_index >= 0 and abs (service_start_line.service_transition_TS - log_dic["skip"][start_skip_index-1].resume_TS) < 2:
            start_skip_index -= 1 # in case skip log transition TS is a bit off from service log transition TS
        while start_skip_index < len (log_dic["skip"]):
            if log_dic["skip"][start_skip_index].ch != service_start_line.channel: 
                start_skip_index +=1
            else: break
        else: 
            WARN ("Could not find line in skip log correspoiding to service start of channel {c} at {t}".format(
                c=service_start_line.channel, t=service_start_line.service_transition_TS))
            service_start_index += 1
            continue

        # dedup c2r
        index = min (len (dedup_a) - 1, bisect_left (dedup_a, service_start_line.service_transition_TS, key = lambda a: a.tx_TS))
        start_dedup_c2r = dedup_a[index].rx_TS - dedup_a[index].camera_TS

        # Occ at the end of service
        # if service_stop_line.service_transition_TS == 1686705460144:
            # print ("debug")
        stop_uplink_index = bisect_left (log_dic["uplink"], service_stop_line.service_transition_TS, key = lambda a: a.queue_size_sample_TS)
        while stop_uplink_index >= 0 and abs (service_stop_line.service_transition_TS - log_dic["uplink"][stop_uplink_index-1].queue_size_sample_TS) < 2:
            stop_uplink_index -= 1 # in case uplink log transition TS is a bit off from service log transition TS
        while stop_uplink_index < len (log_dic["uplink"]):
            if log_dic["uplink"][stop_uplink_index].channel != service_start_line.channel: 
                stop_uplink_index += 1
            else:
                break
        else: 
            WARN ("Could not find line in uplink log correspoiding to service stop of channel {c} at {t}".format(
                c=service_start_line.channel, t=service_stop_line.service_transition_TS))
            service_start_index += 1
            continue

        # t2r at the end of service
        t2r_at_service_end = chrx_a[last_pkt_index].rx_TS - chrx_a[last_pkt_index].tx_TS

        # outputs
        # start of service
        fout.write ("ch,{c}, start_TS,{T}, start_pkt,{p}, dedup_c2r,{t}, enc_qsz,{q}, skip,{s},".format (
            c=service_start_line.channel, T=service_start_line.service_transition_TS, p=chrx_a[first_pkt_index].pkt_num, 
            t=start_dedup_c2r, q=log_dic["skip"][start_skip_index].qsize, s=log_dic["skip"][start_skip_index].skip))
        # end of service
        fout.write ("stop_TS,{T}, stop_pkt,{p}, occ,{o}, est_t2r,{t1}, last_pkt_t2r,{t2},".format (
            T=service_stop_line.service_transition_TS, p=chrx_a[last_pkt_index].pkt_num, o=log_dic["service"][service_stop_index].uplink_queue_size,
            t1=log_dic["service"][service_stop_index].est_t2r, t2=last_pkt_t2r))
        # bandwidths
        bandwidth = {"tx_rx_bw": tx_rx_bandwidth, "rx_rx_bw": rx_rx_bandwidth, "sd_bw": sd_bandwidth}
        for key, value in bandwidth.items():
            fout.write ("{},".format(key))
            for j in range (3):
                fout.write ("{:.2f},".format(value[j]))
        fout.write ("rq_bw,{b:.2f}, camera_B,{B}, camera_start_pkt,{s1}, camera_stop_pkt,{s2},".format(
            b=rq_bandwidth, B=camera_bytes, s1=dedup_a[camera_start_index].pkt_num, s2=dedup_a[camera_stop_index].pkt_num))
        # for i in range (3):
            # fout.write ("t2r_bw_{c},{b1:.2f}, r2r_bw_{c},{b2:.2f}, sd_bw_{c},{b3:.2f}, rq_bw_{c},{b4:.2f},".format (
              # c=i, b1=tx_rx_bandwidth[i], b2=rx_rx_bandwidth[i], b3=sd_bandwidth[i], b4=rq_bandwidth[i]))
        # service duration, tranmitted pkts, retx pkts, useful pkts, bytes,
        fout.write ("s_dur,{d1}, t2r_dur,{d2}, r2r_dur,{d3}, tx_pkts,{p}, %useful_pkts,{u:.2f}, tx_retx,{r}, tx_bytes,{b},".format (
            d1=service_duration, d2=tx_rx_duration, d3=rx_rx_duration, p=tx_pkts, r=tx_retx, u=percent_useful_pkts, b=tx_bytes))
        fout.write ("\n")

        service_start_index += 1
        itr_count += 1
        if itr_count % 1000 == 0:
            print ("Reached itr: ", itr_count)
    # while there are more service transitions to be analyzed
    close_output_files (fout)
    return
# end of spike_analysis

def resume_algo_check (log_dic, capture, out_dir): 
    "performs resume algo and effectiveness checks and outputs skip_algo_chk_ file"
    
    print ("Running resume algo checks")
    fout = open_output_files (out_dir + "skip_algo_chk_" + capture.tx_infix)
    
    service_index = 0
    skip_index = 0
    
    while service_index < len(log_dic["service"]) and skip_index < len(log_dic["skip"]):
    
        # read a a going into service line
        service_line = log_dic["service"][service_index]
        if (service_line.service_flag == 0): # not going into serivce, so ignore
            service_index += 1
            continue
    
        # find largest (last) packet number that was retired closest to the service resumption 
        # and channel-x which is all the channels that retired it the earliest
        index = bisect_left (log_dic["all_latency"], service_line.service_transition_TS, key=lambda a: a.bp_t2r_receive_TS)
        if (index):
            index -= 1 # since bisect_left will return index of element with bp_t2r_recevive_TS GE service_transition_TS
    
        channel = {"x": [0]*3, "t2r": [0]*3, "lrp_num": [0]*3, "lrp_bp_TS": [0]*3}

        lrp_num = log_dic["max_bp_pkt_num"][index]
        lrp_bp_TS = log_dic["all_latency"][index].bp_t2r_receive_TS # this is closest bp pkt to service_transition_TS
        while (index >= 0) and (lrp_num == log_dic["max_bp_pkt_num"][index]):
            line = log_dic["all_latency"][index]
            if (line.PktNum == lrp_num):
                if (line.bp_t2r_receive_TS < lrp_bp_TS):
                    # this is the new candidate channel for the lrp so clear out previous candidates
                    for item in channel: channel[item] = [0]*3
                    lrp_bp_TS = line.bp_t2r_receive_TS
                channel["x"][line.sending_channel] = 1
                channel["t2r"][line.sending_channel] = line.bp_t2r
                channel["lrp_num"][line.sending_channel] = line.PktNum
                channel["lrp_bp_TS"][line.sending_channel] = line.bp_t2r_receive_TS
            index -= 1
        # while there are more entries max_bp_pkt_num_list equal to lrp
    
        # check if there is a mismatch with implementation and we should change our decision
        skip_line = log_dic["skip"][skip_index]
        if service_line.service_transition_TS == skip_line.lrp_bp_TS: # race between processes
            channel = {"x": [0]*3, "t2r": [0]*3, "lrp_num": [0]*3, "lrp_bp_TS": [0]*3}
            channel["x"] = [skip_line.ch0_x, skip_line.ch1_x, skip_line.ch2_x]
            for i in range (3):
                if channel["x"][i]:
                    channel["t2r"][i] = skip_line.t2r
                    channel["lrp_num"][i] = skip_line.lrp_num
                    channel["lrp_bp_TS"][i] = skip_line.lrp_bp_TS
    
        # find the last service transition of each channel 
        channel.update ({"found_last_state_x": [0]*3, "last_state_x": [0]*3, "last_state_x_TS": [0]*3})
        index = service_index
        while ((index >= 0) and (sum(channel["found_last_state_x"]) != 3)):
            line = log_dic["service"][index]
            if (channel["found_last_state_x"][line.channel] == 0):
                channel["last_state_x"][line.channel] = line.service_flag
                channel["last_state_x_TS"][line.channel] = line.service_transition_TS
                channel["found_last_state_x"][line.channel] = 1
            index -= 1
    
        # find if channel-x has remained in service since transmitting the last retired packet
        channel.update ({"x_IS": [0]*3})
        for i in range (3):
            channel["x_IS"][i] = int (\
                # channel supplied lrp
                channel["x"][i] and \
                # channel is in service now
                channel["last_state_x"][i] and \
                # channel has been in service transmitting lrp
                (channel["last_state_x_TS"][i] <= (lrp_bp_TS -30 - channel["t2r"][i])))
    
        """
        # compute the number of packets to be skipped
        if (sum(channel["x_IS"]) == 0) or (skip_line.qsize==0):
            skip = 0
        else:
            skip = int(1.5 * frame_sz_list[lrp_num].frame_szP)
        # skip field no longer in the 
        # skip_diff = skip - skip_line.skip 
    
        # temporary deubgging filters
        ignore = int (skip_line.resume_TS == skip_line.lrp_bp_TS)
    
        # outputs
        fout.write ("ch,{c}, is_TS,{t}, skip,{sk}, err,{e}, ig,{ig}, qsz,{q}, ch-x,{cx}, ch-t2r,{t2r}, lrp,{lrp}, lrp_bp_TS,{bp},".format \
            (c=service_line.channel, t=service_line.service_transition_TS, sk=skip, e=skip_diff, ig=ignore, q=skip_line.qsize, \
            cx=channel["x"], t2r=channel["t2r"], lrp=lrp_num, bp=lrp_bp_TS))
        fout.write ("last_x,{x}, last_x_TS,{t}, ch-x_IS,{i},".format(
            x=channel["last_state_x"], t=channel["last_state_x_TS"], i=channel["x_IS"]))
        fout.write ("f#,{f}, szP,{s},".format(f=frame_sz_list[lrp_num].frame_num+1,
            s=frame_sz_list[lrp_num].frame_szP))
        """
    
        #
        # Revised algo 
        # 
    
        # relax channel_x to incldue other channels that transmitted packets neighboring lrp in porximity of lrp_bp_TS
        SEARCH_TS_WINDOW = 0
        SEARCH_PKT_WINDOW = 0
        IN_SERVICE_PERIOD = 15
        MAX_SKIP_PACKETS = 20
        start_TS = lrp_bp_TS - SEARCH_TS_WINDOW
        start_TS_index = bisect_left (log_dic["all_latency"], start_TS, key = lambda a: a.bp_t2r_receive_TS)
        stop_TS = min (lrp_bp_TS + SEARCH_TS_WINDOW, skip_line.resume_TS)
        stop_TS_index = bisect_left (log_dic["all_latency"], stop_TS, key = lambda a: a.bp_t2r_receive_TS)
    
        channel.update({"x_debug": [0]*3})
        for i in range (3):
            if channel["x"][i]:
                continue # only consider the channels that are not supplying lrp
            # consider new candidates for channel-x
            for index, line in enumerate (log_dic["all_latency"][start_TS_index : stop_TS_index][::-1]):
                if (line.sending_channel == i) and (line.PktNum != 4294967295) and \
                    (line.PktNum >= (lrp_num - SEARCH_PKT_WINDOW)): 
                    # valid candidate for channel_x if pkt_num is within search window
                    if (channel["x"][i]==0) or (line.PktNum > channel["lrp_num"][i]): 
                        # find the line with the largest pacekt num in search window
                        channel["x"][i] = 1
                        channel["t2r"][i] = line.bp_t2r
                        channel["lrp_num"][i] = line.PktNum
                        channel["lrp_bp_TS"][i] = line.bp_t2r_receive_TS
                        channel["x_debug"][i] = 1
            # for all the lines in the search window
        # for all channels
        
        # relaxed in_service condition
        channel["x_IS"] = [0]*3
        channel.update({"x_IS_debug": [0]*3})
        for i in range (3):
            channel["x_IS"][i] = int (
                # channel supplied lrp
                channel["x"][i] and 
                # channel is in service now
                channel["last_state_x"][i] and (
                    # channel has been in service for atleast IN_SERVICE_PERIOD prior to resume time
                    channel["last_state_x_TS"][i] <= (service_line.service_transition_TS - IN_SERVICE_PERIOD)
                    or
                    # channel has been in service since it transmitted lrp
                    channel["last_state_x_TS"][i] <= (channel["lrp_bp_TS"][i] -30 - channel["t2r"][i])
                    )
            ) 
            channel["x_IS_debug"][i] = int (
                channel["last_state_x_TS"][i] <= (service_line.service_transition_TS - IN_SERVICE_PERIOD)  and 
                not (channel["last_state_x_TS"][i] <= (channel["lrp_bp_TS"][i] -30 - channel["t2r"][i])))
        
        # compute lrp_tx_TS and find the index in the carrier csv file that corresponds to that lrp_tx_TS
        channel.update({"lrp_tx_TS": [0]*3})
        for i in range (3):
            if (channel["x_IS"][i]):
                # tx_index = bisect_left (log_dic["chrx_sorted_by_pkt_num"+str(i)], channel["lrp_num"][i], key = lambda a: a.pkt_num)
                # channel["lrp_tx_TS"][i] = log_dic["chrx"+str(i)][tx_index].tx_TS
                channel["lrp_tx_TS"][i] = channel["lrp_bp_TS"][i] -30 - channel["t2r"][i]
    
        #
        # if there are multiple candidates for lrp, then select final as lrp_channel. lrp_channel is invalid if x_IS is all 0s
        #
    
        # initialize lrp_channel to one of channel_x so in case x_IS is 000, rest of the code does not go haywire
        for i in channel["x"]:
            lrp_channel = i
            if i: break
    
        """
        # LRP_channel selection Algo 1: DEPRECATED TO MATCH THE IMPLEMENTATION
        # pick according the the table below. 
        # lrp_num  tx_TS
        #  eq       eq              send any
        #  eq       not eq          send smaller TS
        # not eq    eq              send larger pkt num
        # not eq    not eq          send smaller TS
        # if the lrp_tx_TS are not equal then the lrp channel is the one with the smallest TS
        lrp_TS_are_equal = 1
        found_a_candidate = 0
        for i in range (3):
            if channel["x_IS"][i]: 
                if (found_a_candidate == 0):
                    found_a_candidate = 1
                    min_lrp_tx_TS = channel["lrp_tx_TS"][i]
                    lrp_channel = i
                else: # evaluate if current channel has the same TS as the ones already found so far
                    if channel["lrp_tx_TS"][i] < min_lrp_tx_TS: # this channel is better candidate
                        min_lrp_tx_TS = channel["lrp_tx_TS"][i]
                        lrp_channel = i
                        lrp_TS_are_equal = 0
                    elif channel["lrp_tx_TS"][i] > min_lrp_tx_TS: 
                        # this channel is not a candidate but different lrp_tx_TS exist
                        lrp_TS_are_equal = 0
    
        if lrp_TS_are_equal:
            # if all candidates had same TS then select the channel one with largest lrp_num
            found_a_candidate = 0
            for i in range (3):
                if channel["x_IS"][i] and (found_a_candidate==0 or channel["lrp_num"][i] > max_lrp_num): 
                    found_a_candidate = True
                    max_lrp_num = channel["lrp_num"][i]
                    lrp_channel = i
        """
        #
        # LRP channel selection Algo 2: pick the channel with the smallest t2r
        #  
        found_a_candidate = 0
        for i in range (3):
            if channel["x_IS"][i]: 
                if (found_a_candidate == 0):
                    found_a_candidate = 1
                    min_t2r = channel["t2r"][i]
                    lrp_channel = i
                elif channel["t2r"][i] < min_t2r: # this channel is better candidate
                    min_t2r = channel["t2r"][i]
                    lrp_channel = i
    
        #
        # revised skip calculation: skip packets transferred between lrp_tx_TS and lrp_tx_TS + 60 + (resume_TS-lrp_bp_TS)
        #
        # first calculate packets to be skipped relative to lrp_num (calculations are invalid if channel["x_IS"] = [0,0,0])
        chrx_a = log_dic["chrx_sorted_by_pkt_num"+str(lrp_channel)]
        lrp_tx_TS = channel["lrp_tx_TS"][lrp_channel]
        lrp_bp_to_sx_delta = skip_line.resume_TS - channel["lrp_bp_TS"][lrp_channel] if sum (channel["x_IS"]) != 0 else 0 # changed resume TS from service to skip to match implementation
        last_skip_tx_TS = lrp_tx_TS + 60 + lrp_bp_to_sx_delta if sum (channel["x_IS"]) != 0 else 0
        qhead_index = min (len (chrx_a) - 1, bisect_left (chrx_a, skip_line.first_pkt_in_Q, key = lambda a: a.pkt_num))
        qhead_tx_TS = chrx_a[qhead_index].tx_TS
        index = qhead_index
        while index < len(chrx_a) and skip_line.qsize != 0 and (
            chrx_a[index].retx or chrx_a[index].tx_TS <= min (skip_line.resume_TS, last_skip_tx_TS)):
            index += 1
        index = index - 1 # since while exits when chrx_a[index].tx_TS is strictly GT
        while index >=0 and chrx_a[index].retx: index -= 1 # get rid of trailing retx

        # now adjust to match implementation
        # match dynamics
        s_skip_index = bisect_left (chrx_a, skip_line.last_skip_pkt_num, key = lambda a: a.pkt_num)
        next_smaller_tx_TS_index = bisect_left (chrx_a, chrx_a[index].tx_TS, key = lambda a: a.tx_TS) - 1
        possible_implementation_stop_TS = min (chrx_a[s_skip_index].tx_TS, skip_line.resume_TS)
        if possible_implementation_stop_TS == chrx_a[index].tx_TS or \
           possible_implementation_stop_TS == chrx_a[next_smaller_tx_TS_index].tx_TS: 
           index = s_skip_index # override computed index to match dynamics
        # match for missing packets in carrier csv because they were skipped
        # if skip_line.first_pkt_in_Q == 49805:
            # print ("debug")
        if lrp_channel == skip_line.ch and chrx_a[qhead_index].pkt_num != skip_line.first_pkt_in_Q and chrx_a[index].pkt_num == (skip_line.first_pkt_in_Q - 1):
            last_skip_pkt_num = skip_line.last_skip_pkt_num
        else:
            last_skip_pkt_num = chrx_a[index].pkt_num

        # final results
        skippable_packets_in_q = max (0, last_skip_pkt_num - skip_line.first_pkt_in_Q + 1)
        # unretired_skippable_packets = max (0, last_skip_pkt_num + 1 - skip_line.first_pkt_in_Q)
        # skip_from_lrp_num = int (0.8 * skip_from_lrp_num) # guardbanded to not trigger too many retxi
        if sum (channel["x_IS"]) == 0: 
            skip = 0
        else: 
            skip = min (skippable_packets_in_q, skip_line.qsize, MAX_SKIP_PACKETS)
        err = skip - skip_line.skip
    
        """
        algo_resume_pkt_num = skip_line.qhead + skip if skip_line.qsize else 0
    
        index = bisect_left (log_dic["chrx"+str(skip_line.ch)], skip_line.resume_TS, key = lambda a: a.tx_TS)
        impl_resume_pkt_num = log_dic["chrx"+str(skip_line.ch)][index].pkt_num
    
        if sum (channel["x_IS"]) == 0 or skip_line.qsize == 0:
            skip = 0
        else:
            skip = min (skip_line.qsize, MAX_SKIP_PACKETS, max (0, last_skip_pkt_num - resume_pkt_num))
    
        # now check if the resuming channel was effective
        dd_new_resume_index = bisect_left (log_dic["dedup"], resume_pkt_num + skip, key = lambda a: a.pkt_num)
        dd_new_resume_pkt_num = log_dic["dedup"][dd_new_resume_index].pkt_num
        dd_new_resume_rx_TS = log_dic["dedup"][dd_new_resume_index].rx_TS # this is the current delivery time of the proposed first packet after resumption
    
        diff = resume_rx_TS - dd_new_resume_rx_TS
        err = skip - skip_line.skip
        """
    
        # debug print outs
        # resume service related
        fout.write ("ch,{c}, Res_TS,{t},".format (c=skip_line.ch, t=skip_line.resume_TS))
        # ch-x related
        s_ch_x = [skip_line.ch0_x, skip_line.ch1_x, skip_line.ch2_x]
        s_ch_x_IS = [skip_line.ch0_IS, skip_line.ch1_IS, skip_line.ch2_IS]
        s_IS_now = [skip_line.ch0_IS_now, skip_line.ch1_IS_now, skip_line.ch2_IS_now]
        s_lrp_tx_TS = skip_line.lrp_bp_TS - 30 - skip_line.t2r
        fout.write ("ch_x,{x}, x_IS,{x_IS},s_ch_x,{sx}, s_x_IS,{sx_IS}, s_IS_now,{i},".format (
            x=channel["x"], x_IS=channel["x_IS"], sx=s_ch_x, sx_IS=s_ch_x_IS, i=s_IS_now))
        # lrp related
        fout.write ("lrp_ch,{l}, s_lrp_ch,{sl}, lrp_num,{n}, s_lrp_num,{sn}, lrp_bp_TS,{bp_t}, s_lrp_bp_TS,{sbp_t}, lrp_tx_TS,{tx_t}, s_lrp_tx_TS,{stx_t}, t2r,{t2r}, s_t2r,{st2r},".format (
            l=lrp_channel, sl=skip_line.lrp_channel, n=channel["lrp_num"], sn=skip_line.lrp_num, bp_t=channel["lrp_bp_TS"], sbp_t=skip_line.lrp_bp_TS, 
            tx_t=channel["lrp_tx_TS"], stx_t=s_lrp_tx_TS, t2r=channel["t2r"][lrp_channel], st2r=skip_line.t2r))
        # skip related 
        fout.write ("skip_from_q,{s}, last_skip_pkt,{n}, s_last_skip_pkt,{sn}, last_skip_tx_TS,{t}, s_last_skip_tx_TS,{st}, s_qsz,{q}, s_1st_pkt_in_Q,{f}, s_1st_pkt_tx_TS,{ft},".format (
            s=skippable_packets_in_q, n=last_skip_pkt_num, sn=skip_line.last_skip_pkt_num, t=last_skip_tx_TS, st=skip_line.last_skip_tx_TS, q=skip_line.qsize, f=skip_line.first_pkt_in_Q,
            ft=qhead_tx_TS))
        fout.write ("skip,{s}, s_skip,{ss}, err,{e},".format (s=skip, ss=skip_line.skip, e=err))
        #
        # Skip effectiveness
        #
        chrx_a = log_dic["chrx"+str(skip_line.ch)]
        resume_index = bisect_left (chrx_a, skip_line.resume_TS, key=lambda a: a.tx_TS)
        resume_pkt_num = chrx_a[resume_index].pkt_num
        resume_t2r = chrx_a[resume_index].rx_TS - chrx_a[resume_index].tx_TS

        dd_index = bisect_left (log_dic["dedup"], resume_pkt_num, key = lambda a: a.pkt_num)
        c_d = chrx_a[resume_index].rx_TS - log_dic["dedup"][dd_index].rx_TS

        chrx_a = log_dic["chrx_sorted_by_pkt_num"+str(lrp_channel)] # not using skip_line.lrp channel since it can be 9
        # if lrp_num == 24041:
            # print ("debug")
        lrp_index = min (len (chrx_a) - 1, bisect_left (chrx_a, skip_line.lrp_num, key = lambda a: a.pkt_num))
        lrp_t2r = chrx_a[lrp_index].rx_TS - chrx_a[lrp_index].tx_TS
        last_skip_index = min (len (chrx_a) - 1, bisect_left (chrx_a, skip_line.lrp_num + skip_line.skip, key = lambda a: a.pkt_num))
        last_skip_t2r = chrx_a[last_skip_index].rx_TS - chrx_a[last_skip_index].tx_TS

        fout.write (",res_ch,{c}, lrp_ch,{lc}, lrp_num,{ln}, x_IS,{xIS}, res_pkt_num,{rn}, lrp_t2r,{lt2r}, lskip_t2r,{lst2r}, res_t2r,{rt2r}, qsz,{q}, skip,{s}, c-d,{cmd},".format (
            c=skip_line.ch, rn=resume_pkt_num, rt2r=resume_t2r, cmd=c_d, xIS=channel["x_IS"], lc=lrp_channel, ln=skip_line.lrp_num, s=skip, lt2r=lrp_t2r,lst2r=last_skip_t2r, q=skip_line.qsize))
    
        """
        # check against implementation.
        fout.write ("err,{e},".format (e=err))
    
        # efficiency calculations
        fout.write ("o_res_pkt,{rp}, o_res_TS,{rt}, n_res_pkt,{nrp}, n_res_TS,{nrt}, c-d,{d}, retx,{r}".format (
            rp=resume_pkt_num, rt=resume_rx_TS, nrp=dd_new_resume_pkt_num, nrt=dd_new_resume_rx_TS, d=diff, r=resume_tx_retx))
        """
    
        fout.write ("\n")
    
        if service_index % 1000 == 0: 
            print ("Resume algo checks @ service index: ", service_index)
        # if service_index == 1000: 
        #    break
        service_index += 1
        skip_index += 1
    # while there are more service transitions to be analyzed

    close_output_files (fout)
    return
# end of resume_alog_check


def brm_algo_check (log_dic, out_dir, capture):
    "Checks bit-rate modulation and outputs brm_algo_chk_ file"

    print ("Running Channel quality state and BRM modulation checks")
    fout = open_output_files (out_dir + "brm_algo_chk_" + capture.tx_infix)
    
    # channel quality state machine states
    GOOD_QUALITY = 0
    POOR_QUALITY = 1
    WAITING_TO_GO_POOR = 3
    WAITING_TO_GO_GOOD = 2
    quality_state = [GOOD_QUALITY]*3
    next_quality_state = [GOOD_QUALITY]*3
    # alternate quality state machine states
    quality_state = [GOOD_QUALITY]*3
    next_quality_state = [GOOD_QUALITY]*3
    
    # encoder bit rate modulation state machine
    HIGH_BIT_RATE = 0
    INTERMEDIATE_BIT_RATE = 1
    LOW_BIT_RATE = 2
    encoder_state = LOW_BIT_RATE
    next_encoder_state = LOW_BIT_RATE
    encoder_state_x_TS = 0
    
    uplink_index = 0
    service_index = 0
    brm_index = 0
    latency_index = 0
    chq_index = 0
    itr_count = 0
    
    scheduler_fields = namedtuple("scheduler_fields", "reason, TS, id")
    HEAD = 0
    ENCODER_ID = 3 # channels use 0..2 channel id as their id
    REASON_WAITING_TO_GO_GOOD = 0
    REASON_WAITING_TO_GO_TO_HIGH_BIT_RATE = 1
    REASON_WAITING_TO_GO_POOR = 2
    internal_scheduling_list = [] # list of time stamps which the state machines will like to be updated
    
    # structures for to hold the inputs from sample to sample 
    IN_SERVICE = 1
    OUT_OF_SERVICE = 0
    queue_size = [0]*3
    queue_size_update_TS = [0]*3
    queue_size_monitor_working = [0]*3
    in_service_state = [IN_SERVICE] *3
    in_service_state_x_TS = [0]*3
    ms_since_in_service_x = [0]*3
    s_est_t2r = [30]*3 # est_t2r read from service log (for debug)
    bp_t2r = [30]*3
    bp_t2r_receive_TS = [0]*3
    est_t2r = [30]*3
    channel_degraded = [0]*3
    brm_qst = [GOOD_QUALITY]*3
    chq_qst = [GOOD_QUALITY]*3
    chq_good_quality_x_TS = [0]*3
    
    # queue_size > 10 duration counter
    qsize_gt_10 = [0]*3
    qsize_gt_10_start_TS = [0]*3
    
    while uplink_index < len (log_dic["uplink"]) and service_index < len (log_dic["service"]) and \
        latency_index < len (log_dic["latency"]) and brm_index < len (log_dic["brm"]) and \
        (chq_index < len (log_dic["chq"]) if "chq" in log_dic else True):
    
        uplink_line = log_dic["uplink"][uplink_index]
        service_line = log_dic["service"][service_index]
        brm_line = log_dic["brm"][brm_index]
        latency_line = log_dic["latency"][latency_index]
        chq_line = log_dic["chq"][chq_index] if "chq" in log_dic else ()
    
        # advance time to what needs to be evaluated first next
        current_TS = min (uplink_line.queue_size_sample_TS, service_line.service_transition_TS, brm_line.TS, latency_line.bp_t2r_receive_TS, 
                          chq_line.TS if "chq" in log_dic else brm_line.TS, # if chq file does not exit, then don't care
                          internal_scheduling_list[HEAD].TS if len (internal_scheduling_list) else brm_line.TS) # don't care if internal_scheduling_list is empty
        
        # read external inputs
        #
        next_uplink_index = uplink_index # assume it is not moving
        # uplink log
        while current_TS == uplink_line.queue_size_sample_TS: 
            queue_size[uplink_line.channel] = uplink_line.queue_size
            queue_size_update_TS[uplink_line.channel] = current_TS
            next_uplink_index += 1 # current line was consumed
            # check if there multiple lines in the log with with the same TS
            if next_uplink_index < len (log_dic["uplink"]) and log_dic["uplink"][next_uplink_index].queue_size_sample_TS == current_TS:
                uplink_index = next_uplink_index
                uplink_line = log_dic["uplink"][uplink_index]
            else: break
        
        # service log
        next_service_index = service_index
        while current_TS == service_line.service_transition_TS:
            queue_size_monitor_working[service_line.channel] = int (service_line.zeroUplinkQueue == 0)
            in_service_state[service_line.channel] = service_line.service_flag
            in_service_state_x_TS[service_line.channel] = service_line.service_transition_TS
            s_est_t2r[service_line.channel] = service_line.est_t2r
            next_service_index += 1 # current line was consumed
            # check if there multiple lines in the log file with the same TS
            if next_service_index < len (log_dic["service"]) and log_dic["service"][next_service_index].service_transition_TS == current_TS: 
                service_index = next_service_index
                service_line = log_dic["service"][service_index]
            else: break
    
        # bit rate modulation log
        next_brm_index = brm_index
        while current_TS == brm_line.TS: 
            next_brm_index = brm_index + 1
            # check if there multiple lines in the log file with the same TS
            if next_brm_index < len (log_dic["brm"]) and log_dic["brm"][next_brm_index].TS == current_TS: 
                brm_index = next_brm_index
                brm_line = log_dic["brm"][brm_index]
            else: break

        # chq log
        next_chq_index = chq_index
        if "chq" in log_dic: 
            while current_TS == chq_line.TS: 
                chq_qst[chq_line.ch] = chq_line.channel_quality
                chq_good_quality_x_TS[chq_line.ch] = chq_line.good_quality_x_TS
                next_chq_index = chq_index + 1
                # check if there multiple lines in the log file with the same TS
                if next_chq_index < len (log_dic["chq"]) and log_dic["chq"][next_chq_index].TS == current_TS: 
                    chq_index = next_chq_index
                    chq_line = log_dic["chq"][chq_index]
                else: break
    
        # latency log
        next_latency_index = latency_index
        while current_TS == latency_line.bp_t2r_receive_TS:
            bp_t2r[latency_line.sending_channel] = latency_line.bp_t2r
            bp_t2r_receive_TS[latency_line.sending_channel] = latency_line.bp_t2r_receive_TS
            next_latency_index = latency_index + 1
            # check if there are multiple line in the log file with the same TS as current_TS
            if next_latency_index < len (log_dic["latency"]) and log_dic["latency"][next_latency_index].bp_t2r_receive_TS == current_TS:
                latency_index = next_latency_index
                latency_line = log_dic["latency"][latency_index]
            else: break
        # compute current est_t2r
        for i in range (3):
            est_t2r[i] = bp_t2r[i] + (current_TS - bp_t2r_receive_TS[i])
    
        #
        # channel quality state machine 
        #
        next_quality_state = deepcopy (quality_state)
        for channel in range(3): 
            ms_since_in_service_x[channel] = current_TS - in_service_state_x_TS[channel]
            channel_degraded[channel] = queue_size[channel] > 10 or est_t2r[channel] > 120
            # state machine
            if quality_state[channel] == GOOD_QUALITY:
                if channel_degraded[channel]:
                    next_quality_state[channel] = WAITING_TO_GO_POOR # was POOR_QUALITY
                    new_entry = scheduler_fields (
                        reason=REASON_WAITING_TO_GO_POOR, TS = 20 + current_TS, id = channel)
                    schedule (internal_scheduling_list, new_entry)
    
            elif quality_state[channel] == POOR_QUALITY: 
                if in_service_state[channel] == IN_SERVICE and \
                    queue_size[channel] < 5 and est_t2r[channel] < 80:
                    if ms_since_in_service_x[channel] > 20:
                        next_quality_state[channel] = GOOD_QUALITY
                    else:
                        next_quality_state[channel] = WAITING_TO_GO_GOOD
                        new_entry = scheduler_fields (
                            reason = REASON_WAITING_TO_GO_GOOD, TS = 20 + current_TS - ms_since_in_service_x[channel], id = channel)
                        schedule (internal_scheduling_list, new_entry)
            
            elif quality_state[channel] == WAITING_TO_GO_POOR:
                head = internal_scheduling_list[HEAD]
                if not channel_degraded[channel]:
                    next_quality_state[channel] = GOOD_QUALITY
                    unschedule (internal_scheduling_list, id = channel, reason = REASON_WAITING_TO_GO_POOR) 
                elif head.id == channel and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_POOR:
                    next_quality_state[channel] = POOR_QUALITY
                    unschedule (internal_scheduling_list, id = channel, reason = REASON_WAITING_TO_GO_POOR) 
    
            else: # WAITING_TO_GO_GOOD
                head = internal_scheduling_list[HEAD]
                if in_service_state == OUT_OF_SERVICE or channel_degraded[channel]:
                    next_quality_state[channel] = POOR_QUALITY
                    unschedule (internal_scheduling_list, id=channel, reason=REASON_WAITING_TO_GO_GOOD)
                elif head.id == channel and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_GOOD:
                    next_quality_state[channel] = GOOD_QUALITY
                    unschedule (internal_scheduling_list, id=channel, reason=REASON_WAITING_TO_GO_GOOD)
            # end of alternate quality state machine
        # for each channel 
    
        #
        # encoder bit rate modulation state machine
        #
        num_of_channels_in_poor_quality_state = sum (
            int (state == POOR_QUALITY or state == WAITING_TO_GO_GOOD) for state in next_quality_state)
        ms_since_encoder_state_x = current_TS - encoder_state_x_TS
        next_encoder_state = deepcopy (encoder_state)
        if current_TS == brm_line.TS: # to match implmentation evaluate encoder state only at brm tick
            if encoder_state == HIGH_BIT_RATE: 
                if num_of_channels_in_poor_quality_state == 3:
                    next_encoder_state = LOW_BIT_RATE 
                    encoder_state_x_TS = current_TS

            elif encoder_state == LOW_BIT_RATE: 
                if num_of_channels_in_poor_quality_state < 2:
                    next_encoder_state = INTERMEDIATE_BIT_RATE
                    encoder_state_x_TS = current_TS
                    index = bisect_left (log_dic["brm"], 375+current_TS, key = lambda a: a.TS) # align to brm execution boundry
                    if index == len (log_dic["brm"]): index -= 1
                    new_entry = scheduler_fields (
                        reason=REASON_WAITING_TO_GO_TO_HIGH_BIT_RATE,
                        TS = log_dic["brm"][index].TS,
                        id = ENCODER_ID) 
                    schedule (internal_scheduling_list, new_entry)

            elif encoder_state == INTERMEDIATE_BIT_RATE:
                head = internal_scheduling_list[HEAD]
                if (num_of_channels_in_poor_quality_state >= 2):
                    next_encoder_state = LOW_BIT_RATE
                    encoder_state_x_TS = current_TS
                    unschedule (internal_scheduling_list, id=ENCODER_ID, reason=REASON_WAITING_TO_GO_TO_HIGH_BIT_RATE)
                elif head.id == ENCODER_ID and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_TO_HIGH_BIT_RATE:
                    next_encoder_state = HIGH_BIT_RATE
                    encoder_state_x_TS = current_TS
                    unschedule (internal_scheduling_list, id=ENCODER_ID, reason=REASON_WAITING_TO_GO_TO_HIGH_BIT_RATE)
            # end of encoder state machine
        ibr_to_x = encoder_state==INTERMEDIATE_BIT_RATE and next_encoder_state!=INTERMEDIATE_BIT_RATE
        ibr_to_nxt = 2 if encoder_state==INTERMEDIATE_BIT_RATE and next_encoder_state==LOW_BIT_RATE else 0
        
        # error check
        brm_qst = [brm_line.ch0_quality_state, brm_line.ch1_quality_state, brm_line.ch2_quality_state]
        qst_err = 0
        est_err = 0
        if current_TS == brm_line.TS: # check errors only at brm tick
            for i in range (3): qst_err += int (brm_qst[i] != quality_state[i] and brm_qst[i] != next_quality_state[i])
            est_err = int (brm_line.encoder_state != encoder_state and brm_line.encoder_state != next_encoder_state)
    
        # update states 
        quality_state = deepcopy (next_quality_state)
        encoder_state = deepcopy (next_encoder_state)
    
        quality_state = deepcopy (next_quality_state)
        encoder_state = deepcopy (next_encoder_state)

        # debug and analysis print outs
        # qsize > 10 duration counter
        qsize_gt_10_duration = [0]*3
        for i in range (3):
            if queue_size[i] > 10:
                if not qsize_gt_10[i]: # start of a run
                    qsize_gt_10_start_TS[i] = current_TS
                qsize_gt_10[i] = 1
            else: # end of a run or continuation of < 10 
                if qsize_gt_10[i]: # end of a run
                    qsize_gt_10_duration[i] = current_TS - qsize_gt_10_start_TS[i]
                qsize_gt_10[i] = 0
        # time stamps
        fout.write ("TS,{t}".format(t=current_TS))
        fout.write (",up_idx,{u}, s_idx,{s}, lat_idx,{l}, brm_idx,{b}, chq_idx,{c}".format (
                i=itr_count, u=uplink_index, s=service_index, b=brm_index, l=latency_index, c=chq_index))
        # encoder and channel quality states and error
        fout.write (",est,{e}, b_est,{be}, est_err,{ee}, qst,{q}, c_qst,{cq}, b_qst,{bq}, qst_err,{qe}".format (
            e=encoder_state, be=brm_line.encoder_state, ee= est_err, q=quality_state, cq=chq_qst, bq=brm_qst, qe=qst_err))
        # encoder state machine inputs
        chq_ms_since_good_quality_x_TS = [0]*3
        if "chq" in log_dic:
            for i in range (3): chq_ms_since_good_quality_x_TS[i] = chq_good_quality_x_TS[i] - current_TS
        fout.write (",ch_pq,{p}, ms_e_st_x,{m}, c_ms_gq_x_TS,{g}, ibr2nxt,{i1}".format (
            p=num_of_channels_in_poor_quality_state, m=ms_since_encoder_state_x, g=chq_ms_since_good_quality_x_TS,
            i1=ibr_to_nxt if ibr_to_x else 9))
        # channel quality state machine inputs
        fout.write (",qsz,{q}, bp_t2r,{t2r}, est_t2r,{t}, s_est_t2r,{st}, IS_st,{i}, ms_IS_x,{mx}, IS_st_x,{ix}, qsz_gt10_d,{qd}, qsz_m,{qm}".format (
            q=queue_size, t2r=bp_t2r, t=est_t2r, st=s_est_t2r, i=in_service_state, mx=ms_since_in_service_x, ix=in_service_state_x_TS, 
            qd=qsize_gt_10_duration, qm=queue_size_monitor_working))
        # internal scheduling list 
        fout.write (", sched,{s}".format (s=internal_scheduling_list))
    
        fout.write ("\n")
    
        # move to next line
        uplink_index = next_uplink_index
        service_index = next_service_index
        brm_index = next_brm_index
        latency_index = next_latency_index
        chq_index = next_chq_index
        itr_count += 1
        if (itr_count % 10_000) == 0: 
            print ("ENCODER BRM checks: itr count:{i} up_index:{u} s_index:{s}, brm_index:{b}, lat_index:{l}, chq_index{c}".format (
                i=itr_count, u=uplink_index, s=service_index, b=brm_index, l=latency_index, c=chq_index))
        # if itr_count == 3_000: break
    # while have not exhausted at least one file
    
    close_output_files (fout)
    return
# end of brm_algo_check    

def skip_effectiveness_check (log_dic, capture, out_dir):
    "checks skip effectiveness by testing if dedup used the first 10 packets from the resume channel. Outputs skip_eff_chk_ file"

    print ("Running resume effectiveness checks")
    fout = open_output_files (out_dir + "skip_eff_chk_" + capture.tx_infix)
    
    for i, is_line in enumerate (log_dic["service"] if log_dic["skip"] == None or len (log_dic["skip"]) == 0 else log_dic["skip"]):
        if log_dic["skip"] == None or len(log_dic["skip"]) == 0: # skip_decision file does not exist, so use service file
            resume_ch = is_line.channel
            resume_TS = is_line.service_transition_TS
        else: # use skip_decision file
            resume_ch = is_line.ch
            resume_TS = is_line.resume_TS
    
        # check if the first 10 packets of the resuming channel have the best or near best delivery time
        ch_resume_index = bisect_left (log_dic["chrx"+str(resume_ch)], resume_TS, key=lambda a: a.tx_TS)
    
        for j in range (1): # was 10
            # find resume packet numbers
            ch_index = ch_resume_index + j
            if ch_index > len(log_dic["chrx"+str(resume_ch)])-1:
                break # reached the end of the array, no more transmissions to check
            ch_line = log_dic["chrx"+str(resume_ch)][ch_index]
            pkt_num = ch_line.pkt_num
    
            # find this packet in the dedup array
            dd_index = bisect_left (log_dic["dedup"], pkt_num, key = lambda a: a.pkt_num)
            if (dd_index == len (log_dic["dedup"])) or (log_dic["dedup"][dd_index].pkt_num != pkt_num):
                WARN ("WARNING Resume effectiveness check: Could not find pkt {p} in dedup array. Res Ch={c} Res_TS={t}\n".format (
                    p=pkt_num, c=resume_ch, t=resume_TS))
                continue # skip checking this packet
            
            # check if resuming channel was effective
            resume_t2r = ch_line.rx_TS - ch_line.tx_TS
            resume_c2t = ch_line.tx_TS - ch_line.camera_TS if ch_line.camera_TS != 0 else 0
            resume_rx_TS = ch_line.rx_TS
            dd_rx_TS = log_dic["dedup"][dd_index].rx_TS
            dd_tx_TS = log_dic["dedup"][dd_index].tx_TS
            dd_cx_TS = log_dic["dedup"][dd_index].camera_TS 
            dd_c2r = dd_rx_TS - dd_cx_TS if dd_cx_TS != 0 else 0
            diff = resume_rx_TS - dd_rx_TS
            fout.write ("ch,{c}, Res_TS,{t}, run_idx,{i}, ch_idx,{ci}, pkt#,{p}, ch_rx_TS,{crt}, dd_rx_TS,{drt}, c-d,{d},".format (
                c=resume_ch, t=resume_TS, i=j, ci=ch_index, p=pkt_num, crt=resume_rx_TS, drt=dd_rx_TS, d=diff))
            if not (log_dic["skip"] == None or len(log_dic["skip"]) == 0):
                fout.write ("qsz,{q}, ch_x,{x}, ch_x_is,{xis}, lrp,{lrp}, lrp_bp_TS,{lrpt}, c2r,{c2r}, t2r,{t2r}, c2t,{c2t},".format (
                    q=is_line.qsize, x=[is_line.ch0_x, is_line.ch1_x, is_line.ch2_x], 
                    xis=[is_line.ch0_IS, is_line.ch1_IS, is_line.ch2_IS], lrp=is_line.lrp_num, lrpt=is_line.lrp_bp_TS,
                    c2r=dd_c2r, t2r=resume_t2r, c2t=resume_c2t))
            fout.write ("\n")
        # for the first 10 transmissions of the resuming channel
    
        if i % 1000 == 0: 
            print ("Resume effectiveness checks @ line:", i)
    # for each service transition
    
    close_output_files (fout)
    return
# end of resume_effecitvenes_checks

"""
    ########################################################################################
    # In_service state machine checks
    ########################################################################################
    
    # In_service state machine states
    IN_SERVICE = 1
    OUT_OF_SERVICE = 0
    WAITING_TO_GO_OUT_OF_SERVICE = 2
    
    #
    # set epoch times and other initial conditions 
    #
    latency_index = 0
    uplink_index = 0
    service_index = 0
    
    latency_line = log_dic["latency"][latency_index]
    uplink_line = log_dic["uplink"][uplink_index]
    service_line = log_dic["service"][service_index]
    
    next_external_eval_TS = min (latency_line.bp_t2r_receive_TS, uplink_line.queue_size_sample_TS, service_line.service_transition_TS)
    
    bp_t2r_receive_TS = [next_external_eval_TS]*3
    bp_t2r = [30]*3
    est_t2r = [30]*3
    
    channel_service_state = [IN_SERVICE]*3 # att, vz, t-mobile
    service_state_transition = [0]*3
    channel_service_state_x_TS = [next_external_eval_TS]*3
    
    queue_size_monitor_working = [True]*3
    queue_size = [0]*3
    
    current_TS = next_external_eval_TS
    # end of start of processing initialization
    
    #
    # main loop
    #
    itr_count = 0
    while latency_index < len(log_dic["latency"]) and uplink_index < len(log_dic["uplink"]) and service_index < len(log_dic["service"]):
        
        #
        # set up next clock tick when the state transition should be evaluated and read external inputs
        #
        latency_line = log_dic["latency"][latency_index]
        uplink_line = log_dic["uplink"][uplink_index]
        service_line = log_dic["service"][service_index]
    
        # find the log input that needs to evaluated next (earliest TS)
        next_external_eval_TS = \
            min (latency_line.bp_t2r_receive_TS, uplink_line.queue_size_sample_TS, service_line.service_transition_TS)
        
        # now check if pending potential state transitions want to be evaluated earlier
        internal_clock_tick = 0
        next_internal_eval_TS = current_TS + 120
        for i in range (3):
            if (channel_service_state[i] == IN_SERVICE) and (est_t2r[i] < 120):
                next_internal_eval_TS = min (next_internal_eval_TS, current_TS + 120 - est_t2r[i]) # + 1)
                internal_clock_tick = 1
    
            if (channel_service_state[i] == WAITING_TO_GO_OUT_OF_SERVICE):
                next_internal_eval_TS = min (next_internal_eval_TS, current_TS + 5 - others_in_service_for[i]) # + 1)
                internal_clock_tick = 1
        # if a state transition occurred then evaluate right away in case another channel is waiting 
        if sum(service_state_transition): 
            next_internal_eval_TS = current_TS
            internal_clock_tick = 1
    
        # prioritize internal clock tick by not reading hte external inputs to match the implementation
        if internal_clock_tick and (next_internal_eval_TS <= next_external_eval_TS):
            current_TS = next_internal_eval_TS
    
        else: # external inputs are read only if no interal pending inputs
            current_TS = next_external_eval_TS    
            
            # read all the nputs with the timestamps = current_TS
            while (current_TS == latency_line.bp_t2r_receive_TS) and (latency_index < len(log_dic["latency"])):
                bp_t2r[latency_line.sending_channel] = latency_line.bp_t2r
                bp_t2r_receive_TS[latency_line.sending_channel] = latency_line.bp_t2r_receive_TS
                latency_index += 1
                if (latency_index < len(log_dic["latency"])): latency_line = log_dic["latency"][latency_index]
            
            while (current_TS == uplink_line.queue_size_sample_TS) and (uplink_index < len(log_dic["uplink"])): 
                queue_size[uplink_line.channel] = uplink_line.queue_size
                uplink_index += 1
                if (uplink_index < len(log_dic["uplink"])): uplink_line = log_dic["uplink"][uplink_index]
            
            while (current_TS == service_line.service_transition_TS) and (service_index < len(log_dic["service"])):
                queue_size_monitor_working[service_line.channel] = service_line.zeroUplinkQueue == 0
                service_index += 1
                if (service_index < len(log_dic["service"])): service_line = log_dic["service"][service_index]
        
        #
        # synthesized inputs
        #
        est_t2r = [0]*3
        for i in range (3):
            est_t2r[i] = bp_t2r[i] + (current_TS - bp_t2r_receive_TS[i])
        
        others_in_service_for = [0]*3
        for i in range (3):
            others_in_service_for[i] = current_TS - \
                min ([(current_TS if channel_service_state[j] == OUT_OF_SERVICE else channel_service_state_x_TS[j]) \
                      for j in range(3) if (j !=i)])
        
        channel_in_service_for = [0]*3 
        for i in range (3):
            channel_in_service_for[i] = current_TS - channel_service_state_x_TS[i]
    
        #
        # In_service state machine
        #
        next_channel_service_state = deepcopy(channel_service_state)
        service_state_transition = [0]*3
        for i in range (3):
    
            # update next state transition depedent inputs
            num_of_channels_in_service = sum (int (x != OUT_OF_SERVICE) for x in next_channel_service_state)
    
            # state machine
            if channel_service_state[i] == IN_SERVICE:
                if (queue_size_monitor_working[i] and (queue_size[i] > 10)) or (est_t2r[i] >= 120): 
                    # channel needs to go out of service as soon as possible
                    if (num_of_channels_in_service == 3): 
                        next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                            [OUT_OF_SERVICE, current_TS, 1]
                    elif (num_of_channels_in_service == 2) and (others_in_service_for[i] >=5):
                        next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                            [OUT_OF_SERVICE, current_TS, 1] 
                    else:
                        next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                            [WAITING_TO_GO_OUT_OF_SERVICE, channel_service_state_x_TS[i], 0]
                    # if this is the only active channel, then can not go out of service
                    
            elif channel_service_state[i] == WAITING_TO_GO_OUT_OF_SERVICE:
                if (num_of_channels_in_service > 2) or (others_in_service_for[i] >= 5):
                    # if the channel has not recovered by now, then proceed to OUT_OF_SERVICE
                    if (queue_size_monitor_working[i] and (queue_size[i] > 10)) or (est_t2r[i] >= 120): 
                        next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                        [OUT_OF_SERVICE, current_TS, 1]
                    else: # else go back to IN_SERVICE
                        next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                        [IN_SERVICE, channel_service_state_x_TS[i], 0]
    
            else: # channel_service_state[i] = OUT_OF_SERVICE
                if (not queue_size_monitor_working[i] or (queue_size[i] < 5)) and (est_t2r[i] < 80):
                    next_channel_service_state[i], channel_service_state_x_TS[i], service_state_transition[i] = \
                        [IN_SERVICE, current_TS, 1]
        
            # emit output if state change
            if service_state_transition[i]: 
    
                fout.write ("x_TS,{TS}, x_ch,{ch}, state,{st}, mon,{m}, qsz,{q}, est_t2r,{t2r}, \
                numChIS,{n}, others,{o}, qsz,{qv}, est_t2r,{t2rv}, st,{stv}, nx_st,{nxstv}, stx,{stxv}".format ( \
                TS=channel_service_state_x_TS[i], ch=i, st=next_channel_service_state[i], m=queue_size_monitor_working[i], \
                q=queue_size[i], t2r=est_t2r[i], n=num_of_channels_in_service, o=others_in_service_for[i], \
                qv=queue_size, t2rv=est_t2r, stv=channel_service_state, nxstv=next_channel_service_state, stxv=service_state_transition))
    
                fout.write("\n")
        
                if itr_count % 1000 == 0: 
                    print ("itr_count: ", itr_count)
                itr_count += 1
    
        # for In_service state machine transition of each channel
        channel_service_state = deepcopy(next_channel_service_state)
        # channel_service_state = [st for st in next_channel_service_state]
    # while have not exhausted at least one input log file
"""

def max_burst_check (log_dic, capture, out_dir):
    "Checks that no more than MAX_BURST_PACKETS are transferred in the occupancy sampling time"

    MAX_BURST_SIZE = 20

    print ("Running max_burst_check")
    fout = open_output_files (out_dir + "max_burst_check_" + capture.tx_infix)

    # determine the occupancy sampling period of each channel
    avg_sampling_period = [0]*3
    for i in range (3):
        sampling_period_list = list ((line.elapsed_time_since_last_queue_update for line in log_dic["uplink"] if line.channel == i))
        avg_sampling_period[i] = math.ceil (sum (sampling_period_list) / len (sampling_period_list))
    
    # check max burst in a single sampling period
    for i in range (3):
        chrx_a = log_dic["chrx"+str(i)]
        index = 0
        while index + MAX_BURST_SIZE + 1 < len (chrx_a): 
            p1_TS = chrx_a[index].tx_TS
            p2_TS = chrx_a[index + MAX_BURST_SIZE + 1].tx_TS
            if p2_TS < p1_TS + avg_sampling_period[i]:
                fout.write ("Channel,{c}, pkt1_num,{p1}, pkt1_TS,{T1}, pkt2_num,{p2}, pkt2_TS,{T2}, TS_delta,{d}, s_per,{s}".format (
                    c=i, p1=chrx_a[index].pkt_num, T1=p1_TS, p2=chrx_a[index + MAX_BURST_SIZE + 1].pkt_num, T2=p2_TS,
                    d=(p2_TS-p1_TS), s=avg_sampling_period[i]))
                fout.write ("\n")
            burst_expire_TS = p1_TS + avg_sampling_period[i]
            index = bisect_left (chrx_a, burst_expire_TS, key = lambda a: a.tx_TS)
        # while there are more lines in the channel array
    #  for each channel
    close_output_files (fout)
    return
# end of max_burst_check