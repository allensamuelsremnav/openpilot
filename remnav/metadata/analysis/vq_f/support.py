import sys
import math
import time
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy

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
# CH: 1, Skip  packets. Method: 0, t2r: 0, lastPacketNumber to throwaway: 0, qsize: 313, framePacketNum: 0,  lastRetiredCH: 1, lastPacketRetired: 0, lastPacketRetiredTIme: 0, lastPacketSearchTime: 18446742387968496066, ch0 service: 1, ch1 service: 1, ch2 service: 0, ch0In: 1, ch1In : 1, ch2In: 0, ch0Matched:1, ch1Matched:1, ch2Matched:0, ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1685741053515, ch1InTime: 1685741055610, ch2InTime:1685741053515, time: 1685741055610
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
            err_str = "FATAL: Incorrectly nested comments start line: ", + str(i)
            sys.stderr.write (err_str)
            exit
    
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
                err_str = "Invalid syntax at line :" + str(i)
                sys.stderr.write (err_str)
            
            if stx_defined and srx_defined and ipath_defined:
                work_list += [file_list_fields (in_dir=ipath, rx_infix=srx, tx_infix=stx)]
                srx_defined = False 
                stx_defined = False
        except:
            sys.stderr ("FATAL read_worklist: Invalid syntax at line {n}: {l} in file {f}".format (n=i, l=line, f=input_file))
            exit
        # end of parse a non-comment line
    # for all lines in the input_file
    return work_list
# end of read_worklist

def create_dic (in_dir, tx_infix, rx_infix):
    " creates file attribute dictionary and corresponding entry in log dictionary"
    
    files_dic = {}
    log_dic = {}

    #
    # log files
    #
    files_dic.update ({"uplink":  files_dic_fields._make ([in_dir+"uplink_queue_"+tx_infix+".log", uplink_fields])})
    uplink_array = []
    log_dic.update ({"uplink": uplink_array})
    
    files_dic.update ({"all_latency": files_dic_fields._make ([in_dir+"latency_"+tx_infix+".log", latency_fields])})
    all_latency_array = []
    log_dic.update ({"all_latency": all_latency_array})
    
    files_dic.update ({"service": files_dic_fields._make ([in_dir+"service_"+tx_infix+".log", service_fields])})
    service_array = []
    log_dic.update ({"service": service_array})
    
    files_dic.update ({"skip": files_dic_fields._make ([in_dir+"skip_decision_"+tx_infix+".log", skip_fields])})
    skip_array = []
    log_dic.update ({"skip": skip_array})
    
    files_dic.update ({"brm": files_dic_fields._make ([in_dir+"bitrate_"+tx_infix+".log", brm_fields])})
    brm_array = []
    log_dic.update ({"brm": brm_array})

    """
    files_dic.update ({"probe":   files_dic_fields([in_dir+"probe_"+rx_infix+".log",probe_fields])})
    probe_array = []
    log_dic.update ({"probe": probe_array})
    """
    
    #
    # csv files
    # 
    for i in range (3):
        files_dic.update ({"chrx"+str(i): files_dic_fields._make ([in_dir+rx_infix+"_ch"+ str(i) + ".csv", chrx_fields])})
        chrx_array = []
        log_dic.update ({"chrx"+str(i): chrx_array})
    
    files_dic.update ({"dedup": files_dic_fields._make ([in_dir+rx_infix+".csv", dedup_fields])})
    dedup_array = []
    log_dic.update ({"dedup": dedup_array})

    # check that files_dic and log_dic are consistent
    files_dic_keys = set (list (files_dic.keys()))
    log_dic_keys = set (list (log_dic.keys()))
    if (files_dic_keys != log_dic_keys): 
        print ("keys don't match")
        print ("files_dic_keys: ", files_dic_keys)
        print ("log_dic keys:   ", log_dic_keys)
        exit ()

    return files_dic, log_dic
# end of create_dic

def read_files (files_dic, log_dic):
    " reads all the log/csv file specified in files dictionary and stores them in the log dictonary"
    TX_TS_INDEX = 1
    # read all the log and csv files
    for item in files_dic:
        print ("reading file: ", files_dic[item].filename)
        if item == "dedup" or item.startswith("chrx"):
            log_dic[item] = read_csv_file (files_dic[item].filename, files_dic[item].fields, TX_TS_INDEX)
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
            err_str = "WARNING read_log_file: incorrect number of filelds: " + filename  + " Line " + str(line_num) + ": " + " ".join (str(e) for e in field_list) +"\n"
            sys.stderr.write (err_str)
            # exit ()

    return array
# end of read_log_file

def read_csv_file(filename, tuplename, tx_TS_index):
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

        # fix the tx time
        field_list[tx_TS_index] = int (field_list[tx_TS_index]/512)

        try: 
            array += [tuplename._make(field_list)]
        except:
            err_str = "WARNING read_csv_file: incorrect number of filelds: " + filename  + " Line " + str(line_num) + ": " + " ".join (str(e) for e in field_list) +"\n"
            sys.stderr.write (err_str)
            # exit ()

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
