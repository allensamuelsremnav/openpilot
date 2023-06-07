import sys
import math
import time
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy

#########################################################################################
# function definitions
#########################################################################################

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

def init_channel_x (num):
    """ initializes speicified lists to 3 elements to 0 """
    a = []
    for x in range(num):
        a += [[0]*3]
    return a

def set_channel (channel, line, index):
    channel["x"][line.sending_channel] = 1
    channel["t2r"][line.sending_channel] = line.bp_t2r
    channel["lrp_num"][line.sending_channel] = line.PktNum
    channel["lrp_bp_TS"][line.sending_channel] = line.bp_t2r_receive_TS
    channel["lindex"][line.sending_channel] = index
    return

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
            print (len(field_list), len (tuplename))
            # exit ()

    return array
# end of read_csv_file

#########################################################################################
# in/out directory and file prefix/suffix
#########################################################################################
out_dir = "C:/Users/gopal/Downloads/analysis_output/"

in_dir = "C:/Users/gopal/Downloads/skip_unit_test/"
rx_name_part = "2023_06_06_15_54_37_v_9_7_3_online"
tx_name_part = "2023_06_06_15_54_32_v11_8_4"

# in_dir = "C:/Users/gopal/Downloads/skip_unit_test/"
# rx_name_part = "2023_06_05_21_48_41_v_9_7_3_online"
# tx_name_part = "2023_06_05_21_48_36_v11_8_4"

# rx_name_part = "2023_06_05_14_06_14_v_9_7_3_online"
# tx_name_part = "2023_06_05_14_06_09_v11_8_4"

# in_dir = "C:/Users/gopal/Downloads/brm_06_01/"
# rx_name_part = "2023_06_02_22_11_09_v_9_7_3_online"
# tx_name_part = "2023_06_02_22_11_04_v11_8_4"

# in_dir = "C:/Users/gopal/Downloads/06_02_2023/"
# rx_name_part = "2023_06_02_14_06_43_v_9_7_3_online"
# tx_name_part = "2023_06_02_14_06_38_v11_8_4"

# rx_name_part = "2023_05_22_14_24_18_v_9_7_3_online"
# tx_name_part = "2023_05_22_14_24_13_v11_8_4"

# rx_name_part = "2023_05_22_14_41_50_v_9_7_3_online"
# tx_name_part = "2023_05_22_14_41_45_v11_8_4"

# rx_name_part = "2023_05_19_10_16_32_v_9_7_3_online"
# tx_name_part = "2023_05_19_10_16_27_v11_8_4"

# rx_name_part = "2023_05_19_10_25_42_v_9_7_3_online"
# tx_name_part = "2023_05_19_10_25_37_v11_8_4"

# rx_name_part = "2023_05_19_10_42_40_v_9_7_3_online"
# tx_name_part = "2023_05_19_10_42_35_v11_8_4"

# rx_name_part = "2023_05_19_10_55_32_v_9_7_3_online"
# tx_name_part = "2023_05_19_10_55_27_v11_8_4"

# tx and rx file prefix
# rx_name_part = "2023_05_19_11_20_37_v_9_7_3_online"
# tx_name_part =  "2023_05_19_11_20_31_v11_8_4"

# rx_name_part = "2023_05_19_11_29_52_v_9_7_3_online"
# tx_name_part =  "2023_05_19_11_29_47_v11_8_4"

# tx_name_part = "2023_05_19_11_38_37_v11_8_4"
# rx_name_part = "2023_05_19_11_38_43_v_9_7_3_online"

# tx_name_part = "2023_05_19_11_03_57_v11_8_4"
# rx_name_part = "2023_05_19_11_04_02_v_9_7_3_online"

# tx_name_part = "2023_05_19_10_34_26_v11_8_4"
# rx_name_part = "2023_05_19_10_34_31_v_9_7_3_online"

# tx_name_part = "2023_05_12_14_31_29_v11_8_4"
# rx_name_part = "2023_05_12_14_31_34_v_9_7_3_online"

#########################################################################################
# log and csv file structures
#########################################################################################
files_dic_fields = namedtuple ("files_dic_fields", "filename, fields")
files_dic = {}
log_dic = {}

#
# log files
#
# uplink log
# uplink_queue. ch: 1, timestamp: 1681947064182, queue_size: 316, elapsed_time_since_last_queue_update: 9, actual_rate: 0
uplink_fields = namedtuple ("uplink_fields", "channel, queue_size_sample_TS, queue_size, \
                             elapsed_time_since_last_queue_update, actual_rate")
files_dic.update ({"uplink":  files_dic_fields._make ([in_dir+"uplink_queue_"+tx_name_part+".log", uplink_fields])})
uplink_array = []
log_dic.update ({"uplink": uplink_array})

# latency log
# ch: 0, received a latency, numCHOut:2, packetNum: 4294967295, latency: 40, time: 1681947064236, sent from ch: 0
# receive_TS is the time when the back propagated t2r info is received by the vehicle
latency_fields = namedtuple ("latency_fields", "communicating_channel, numCHOut, PktNum, bp_t2r, bp_t2r_receive_TS, sending_channel")
files_dic.update ({"all_latency": files_dic_fields._make ([in_dir+"latency_"+tx_name_part+".log", latency_fields])})
all_latency_array = []
log_dic.update ({"all_latency": all_latency_array})

# service log
# CH: 2, change to out-of-service state, latency: 0, latencyTime: 0, estimated latency: 2614851439, stop_sending flag: 0 , uplink queue size: 0, zeroUplinkQueue: 0, service flag: 0, numCHOut: 1, Time: 1681947064175, packetNum: 0
service_fields = namedtuple ("service_fields", "channel, bp_t2r, bp_t2r_receive_TS, est_t2r, stop_sending_flag, \
                             uplink_queue_size, zeroUplinkQueue, service_flag, numCHOut, service_transition_TS, bp_t2r_packetNum")
files_dic.update ({"service": files_dic_fields._make ([in_dir+"service_"+tx_name_part+".log", service_fields])})
service_array = []
log_dic.update ({"service": service_array})

"""
# skip decision log
# CH: 0, Skip  packets. Method: 0, packetSent[lastPacketRetired]: 1, skip: 0, qsize: 10, framePacketNum: 0,  lastRetiredCH: 1, lastPacketRetired: 0, lastPacketRetiredTIme: 0, ch0 service: 1, ch1 service: 1, ch2 service: 0, ch0In: 1, ch1In : 1, ch2In: 0, ch0Matched:1, ch1Matched:1, ch2Matched:1, ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1684521522628, ch1InTime: 1684521522572, ch2InTime:1684521517919, time: 1684521522628
skip_fields = namedtuple ("skip_fields", "ch, method, ignore1, skip, qsize, szP,  ignore2, lrp, lrp_bp_TS, ch0_IS_now, ch1_IS_now, ch2_IS_now, ch0_IS, ch1_IS, ch2_IS, ch0Matched, ch1Matched, ch2Matched, ch0_x, ch1_x, ch2_x, ch0InTime, ch1InTime, ch2InTime, resume_TS")
files_dic.update ({"skip": files_dic_fields._make ([in_dir+"skip_decision_"+tx_name_part+".log", skip_fields])})
skip_array = []
log_dic.update ({"skip": skip_array})
"""

# skip decision log - new format
# CH: 1, Skip  packets. Method: 0, t2r: 0, lastPacketNumber to throwaway: 0, qsize: 313, framePacketNum: 0,  lastRetiredCH: 1, lastPacketRetired: 0, lastPacketRetiredTIme: 0, lastPacketSearchTime: 18446742387968496066, ch0 service: 1, ch1 service: 1, ch2 service: 0, ch0In: 1, ch1In : 1, ch2In: 0, ch0Matched:1, ch1Matched:1, ch2Matched:0, ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1685741053515, ch1InTime: 1685741055610, ch2InTime:1685741053515, time: 1685741055610
skip_fields = namedtuple ("skip_fields", 
                         # CH: 1,  Method: 0, t2r: 0, lastPacketNumber to throwaway: 0, qsize: 313, framePacketNum: 0,  lastRetiredCH: 2, lastPacketRetired: 0, lastPacketRetiredTIme: 0, lastPacketSearchTime: xxx, i
                         # ch0 service: 0, ch1 service: 1, ch2 service: 1, ch0In: 0, ch1In : 1, ch2In: 1, ch0Matched:1, ch1Matched:1, ch2Matched:0, i
                         # ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1685999169730, ch1InTime: 1685999171821, ch2InTime:1685999169730, time: 1685999171821, skip: 0, firstPacketinQ: 0
                          "ch,     method,    t2r,    last_skip_pkt_num,                qsize,       szP,               lrp_channel,       lrp_num,               lrp_bp_TS,               last_skip_TS, \
                           ch0_IS_now,    ch1_IS_now,      ch2_IS_now,    ch0_IS,    ch1_IS,     ch2_IS,   ch0Matched,  ch1Matched,    ch2Matched, \
                           ch0_x,    ch1_x,    ch2_x,   ch0_IS_x_TS,             ch1_IS_x_TS,              ch2_IS_x_TS,              resume_TS,        skip,    first_pkt_in_Q, \
                           ch0_lrp, ch1_lrp, ch2_lrp, ch0_lrp_bp_ts, ch1_lrp_bp_ts, ch2_lrp_bp_ts")

files_dic.update ({"skip": files_dic_fields._make ([in_dir+"skip_decision_"+tx_name_part+".log", skip_fields])})
skip_array = []
log_dic.update ({"skip": skip_array})

# bitrate log
# send_bitrate: 830000, encoder state: 2, ch0 quality state: 1, ch1 quality state: 1, ch2 quality state: 1, time: 1681947065306
brm_fields = namedtuple ("brm_fields", "send_bitrate, encoder_state, ch0_quality_state, ch1_quality_state, ch2_quality_state, TS")
files_dic.update ({"brm": files_dic_fields._make ([in_dir+"bitrate_"+tx_name_part+".log", brm_fields])})
brm_array = []
log_dic.update ({"brm": brm_array})

# avgQ log
# RollingAvg75. Probe. CH: 2, RollingAvg75: 0.000000, qualityState: 1, queue size: 0, time: 1681947064175
# avgq_fields 

"""
# retx log
# ch: 2, received a retx, numCHOut:2, startPacketNum: 39753, run: 1, time: 1681946093091

# probe log
# ch: 0, receive_a_probe_packet. sendTime: 1681946022261, latency: 45, receivedTime: 1681946022306
probe_fields = namedtuple ("probe_fields", "sending_channel, send_TS, latency, receive_TS")
files_dic.update ({"probe":   files_dic_fields._make ([in_dir+"probe_"+rx_name_part+".log",probe_fields])})
# print (files_dic)
probe_array = []
log_dic.update ({"probe": probe_array})
"""

#
# csv files
#
TX_TS_INDEX = 1
# carrier csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 chPacketNum
# 0	             8.62473E+14	     1.68452E+12	     1384	              1	              0	             0	          0	                 0	         1.68452E+12	     0	      0

chrx_fields = namedtuple ("chrx_fields", "pkt_num, tx_TS, rx_TS, pkt_len, frame_start, frame_number, \
                           frame_rate, frame_res, frame_end, camera_TS, retx, chk_pkt")
for i in range (3):
    files_dic.update ({"chrx"+str(i): files_dic_fields._make ([in_dir+rx_name_part+"_ch"+ str(i) + ".csv", chrx_fields])})
    chrx_array = []
    log_dic.update ({"chrx"+str(i): chrx_array})

# dedup csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 ch	 latency
# 0	             8.61156E+14	     1.68195E+12	     1384	             1	             0	             0	         0	                 0	         1.68195E+12	     0	     2	    37
dedup_fields = namedtuple ("dedup_fields", "pkt_num, tx_TS, rx_TS, pkt_len, frame_start, frame_number, \
                           frame_rate, frame_res, frame_end, camera_TS, retx, ch, latency")
files_dic.update ({"dedup": files_dic_fields._make ([in_dir+rx_name_part+".csv", dedup_fields])})
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

#########################################################################################
# read all the log data files. Each log is stored as list of namedtuples defined earlier
#########################################################################################

# read all the log and csv files
for item in files_dic:
    print ("reading file: ", files_dic[item].filename)
    if item == "dedup" or item.startswith("chrx"):
        log_dic[item] = read_csv_file (files_dic[item].filename, files_dic[item].fields, TX_TS_INDEX)
    else:
        log_dic[item] = read_log_file (files_dic[item].filename, files_dic[item].fields)
    if (log_dic[item] != None):
        print ("\t file length = {}".format (len(log_dic[item])))

# create cleaned up latency file to retain channel to channel communication only
# create max pcaket list - largest packet number that has been back propagated up to each latency array line
print ("Removing unnecessary lines from the latency file")
print ("Original latency file length: {}".format (len(log_dic["all_latency"])))
short_list = []
max_bp_pkt_num_list = [0]
for i, line in enumerate (log_dic["all_latency"]):
    if line.communicating_channel == line.sending_channel: 
        short_list += [(line)]
    if i:
        max_bp_pkt_num_list += [max(line.PktNum, max_bp_pkt_num_list[i-1])] if (line.PktNum != 4294967295) else [max_bp_pkt_num_list[i-1]] 
log_dic.update ({"latency": short_list})
print ("After removing unnecessary lines, latency file lenght: {}".format (len(log_dic["latency"])))

# create array indexable by packet number that returns frame number and frame size in packets
frame_sz_list_fields = namedtuple ("frame_sz_list_fields", "frame_num, frame_szP")
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

# clean chrx: remove duplicates not due to retx, sort chrx arrays by tx_TS
for i in range(3):
    chrx_a = log_dic["chrx"+str(i)]
    chrx_a.sort (key = lambda a: a.tx_TS) 
    chrx_a.sort (key = lambda a: a.pkt_num)
    count = 0
    for j, line in enumerate (chrx_a):
        if j+1 < len(chrx_a):
            if line.retx == chrx_a[j+1].retx and line.pkt_num == chrx_a[j+1].pkt_num: # network introduce duplicates
               del chrx_a[j] 
               count += 1
    if (count): 
        array_name = "chrx"+str(i)
        print ("remvoved {n} duplicates from {s} metadata array".format (n=count, s=array_name))
    # now sort it back by tx_TS
    chrx_a.sort (key = lambda a: a.tx_TS) 

    log_dic.update({"chrx_sorted_by_pkt_num"+str(i): sorted(log_dic["chrx"+str(i)], key = lambda a: a.pkt_num)})

########################################################################################
# channel quality state and bit rate modulation checks
########################################################################################

fout = open (out_dir + "brm_chk" + tx_name_part + ".csv", "w")

print ("Running Channel quality state and BRM modulation checks")

# channel quality state machine states
GOOD_QUALITY = 0
POOR_QUALITY = 1
WAITING_TO_GO_POOR = 3
WAITING_TO_GO_GOOD = 2
quality_state = [GOOD_QUALITY]*3
next_quality_state = [GOOD_QUALITY]*3
# alternate quality state machine states
alt_quality_state = [GOOD_QUALITY]*3
alt_next_quality_state = [GOOD_QUALITY]*3

# encoder bit rate modulation state machine
HIGH_BIT_RATE = 0
LOW_BIT_RATE = 2
encoder_state = LOW_BIT_RATE
next_encoder_state = LOW_BIT_RATE
encoder_state_x_TS = 0
# altnernate bit rate modulation 
alt_encoder_state = LOW_BIT_RATE
alt_next_encoder_state = LOW_BIT_RATE
alt_encoder_state_x_TS = 0

uplink_index = 0
service_index = 0
brm_index = 0
latency_index = 0
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
ms_since_channel_degraded = [0]*3

while uplink_index < len (log_dic["uplink"]) and service_index < len (log_dic["service"]) and \
    latency_index < len (log_dic["latency"]) and brm_index < len (log_dic["brm"]):

    uplink_line = log_dic["uplink"][uplink_index]
    service_line = log_dic["service"][service_index]
    brm_line = log_dic["brm"][brm_index]
    latency_line = log_dic["latency"][latency_index]

    # advance time to what needs to be evaluated first next
    if len (internal_scheduling_list): 
        current_TS = min (
            uplink_line.queue_size_sample_TS, service_line.service_transition_TS, brm_line.TS,
            latency_line.bp_t2r_receive_TS, internal_scheduling_list[HEAD].TS
        )
    else: 
        current_TS = min (
            uplink_line.queue_size_sample_TS, service_line.service_transition_TS, brm_line.TS, 
            latency_line.bp_t2r_receive_TS)
    
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
    # quality state machine 
    #
    next_quality_state = deepcopy (quality_state)
    for channel in range(3): 

        ms_since_in_service_x[channel] = current_TS - in_service_state_x_TS[channel]

        if not queue_size_monitor_working[channel]:
            next_quality_state[channel] = POOR_QUALITY

        elif quality_state[channel] == GOOD_QUALITY:
            if queue_size[channel] > 10 or est_t2r[channel] > 120:
                next_quality_state[channel] = POOR_QUALITY

        elif quality_state[channel] == POOR_QUALITY: 
            if in_service_state[channel] == IN_SERVICE and \
                queue_size[channel] < 5 and \
                est_t2r[channel] < 80:
                if ms_since_in_service_x[channel] > 35: 
                    next_quality_state[channel] = GOOD_QUALITY
                else:
                    next_quality_state[channel] = WAITING_TO_GO_GOOD
                    new_entry = scheduler_fields (
                        reason=REASON_WAITING_TO_GO_GOOD, 
                        TS=35 + current_TS - ms_since_in_service_x[channel], 
                        id=channel)
                    schedule (internal_scheduling_list, new_entry)
        
        else: # WAITING_TO_GO_GOOD
            head = internal_scheduling_list[HEAD]
            if in_service_state == OUT_OF_SERVICE or queue_size[channel] >= 5 or est_t2r[channel] >= 80:
                next_quality_state[channel] = POOR_QUALITY
                unschedule (internal_scheduling_list, id=channel, reason=REASON_WAITING_TO_GO_GOOD)
            elif head.id == channel and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_GOOD:
                next_quality_state[channel] = GOOD_QUALITY
                unschedule (internal_scheduling_list, id=channel, reason=REASON_WAITING_TO_GO_GOOD)
        # end of quality state machine 
    # for each channel 

    #
    # alternate quality state machine
    #
    alt_next_quality_state = deepcopy (alt_quality_state)
    for channel in range(3): 

        ms_since_in_service_x[channel] = current_TS - in_service_state_x_TS[channel]
        channel_degraded[channel] = 1 # assume degraded
        if queue_size[channel] > 10 and est_t2r[channel] > 120: # both degraded
            ms_since_channel_degraded[channel] = max (current_TS - queue_size_update_TS[channel],  max (0, est_t2r[channel] - 120))
        elif queue_size[channel] > 10: 
            ms_since_channel_degraded[channel] = current_TS - queue_size_update_TS[channel]
        elif est_t2r[channel] > 120:
            ms_since_channel_degraded[channel] = max (0, est_t2r[channel] - 120)
        else: # channel is not degraded
            ms_since_channel_degraded[channel] = 0
            channel_degraded[channel] = 0

        if (current_TS == 1685769077272): 
            print ("debug")
        
        # state machine
        """
        if not queue_size_monitor_working[channel]:
            alt_next_quality_state[channel] = POOR_QUALITY

        elif alt_quality_state[channel] == GOOD_QUALITY:
        """
        if alt_quality_state[channel] == GOOD_QUALITY:
            if channel_degraded[channel]:
                if ms_since_channel_degraded[channel] > 35:
                    alt_next_quality_state[channel] = POOR_QUALITY
                else: 
                    alt_next_quality_state[channel] = WAITING_TO_GO_POOR # was POOR_QUALITY
                    new_entry = scheduler_fields (
                        reason=REASON_WAITING_TO_GO_POOR,
                        TS = 35 + current_TS - ms_since_channel_degraded[channel],
                        id = 3+channel)
                    schedule (internal_scheduling_list, new_entry)

        elif alt_quality_state[channel] == POOR_QUALITY: 
            if in_service_state[channel] == IN_SERVICE and \
                queue_size[channel] < 5 and \
                est_t2r[channel] < 80:
                if ms_since_in_service_x[channel] > 20: # was 35
                    alt_next_quality_state[channel] = GOOD_QUALITY
                else:
                    alt_next_quality_state[channel] = WAITING_TO_GO_GOOD
                    new_entry = scheduler_fields (
                        reason = REASON_WAITING_TO_GO_GOOD, 
                        TS = 20 + current_TS - ms_since_in_service_x[channel], # was 35
                        id = 3+channel)
                    schedule (internal_scheduling_list, new_entry)
        
        elif alt_quality_state[channel] == WAITING_TO_GO_POOR:
            head = internal_scheduling_list[HEAD]
            if not channel_degraded[channel]:
                alt_next_quality_state[channel] = GOOD_QUALITY
                unschedule (internal_scheduling_list, id = 3+channel, reason = REASON_WAITING_TO_GO_POOR) 
            elif head.id == (3+channel) and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_POOR:
                alt_next_quality_state[channel] = POOR_QUALITY
                unschedule (internal_scheduling_list, id = 3+channel, reason = REASON_WAITING_TO_GO_POOR) 

        else: # WAITING_TO_GO_GOOD
            head = internal_scheduling_list[HEAD]
            if in_service_state == OUT_OF_SERVICE or queue_size[channel] >10 or est_t2r[channel] > 120: # was >=5 and >=80
                alt_next_quality_state[channel] = POOR_QUALITY
                unschedule (internal_scheduling_list, id=3+channel, reason=REASON_WAITING_TO_GO_GOOD)
            elif head.id == (3+channel) and head.TS == current_TS and head.reason == REASON_WAITING_TO_GO_GOOD:
                alt_next_quality_state[channel] = GOOD_QUALITY
                unschedule (internal_scheduling_list, id=3+channel, reason=REASON_WAITING_TO_GO_GOOD)
        # end of alternate quality state machine
    # for each channel 

    #
    # encoder bit rate modulation state machine
    #
    num_of_channels_in_poor_quality_state = sum (
        int (state == POOR_QUALITY or state == WAITING_TO_GO_GOOD) for state in next_quality_state)
    ms_since_encoder_state_x = current_TS - encoder_state_x_TS

    next_encoder_state = encoder_state
    if current_TS == brm_line.TS: # evaluate at brm sample (frame) intervals
        if encoder_state == HIGH_BIT_RATE: 
            if num_of_channels_in_poor_quality_state == 3:
                next_encoder_state = LOW_BIT_RATE 
                encoder_state_x_TS = current_TS
    
        else: # encoder_state == LOW_BIT_RATE: 
            if num_of_channels_in_poor_quality_state < 3 and ms_since_encoder_state_x > 60:
                next_encoder_state = HIGH_BIT_RATE 
                encoder_state_x_TS = current_TS
    # end of encoder state machine

    #
    # alternate encoder bit rate modulation state machine
    #
    alt_num_of_channels_in_poor_quality_state = sum (
        int (state == POOR_QUALITY or state == WAITING_TO_GO_GOOD) for state in alt_next_quality_state)
    alt_ms_since_encoder_state_x = current_TS - alt_encoder_state_x_TS

    alt_next_encoder_state = alt_encoder_state
    if current_TS == brm_line.TS: # evaluate at brm sample (frame) intervals
        if alt_encoder_state == HIGH_BIT_RATE: 
            if alt_num_of_channels_in_poor_quality_state == 3:
                alt_next_encoder_state = LOW_BIT_RATE 
                alt_encoder_state_x_TS = current_TS
    
        elif alt_encoder_state == LOW_BIT_RATE: 
            if num_of_channels_in_poor_quality_state < 3 and alt_ms_since_encoder_state_x > 60:
                alt_next_encoder_state = HIGH_BIT_RATE 
                alt_encoder_state_x_TS = current_TS
    # end of alternate encoder state machine

    #
    # error check
    #
    brm_qst = [brm_line.ch0_quality_state, brm_line.ch1_quality_state, brm_line.ch2_quality_state]

    qst_err = 0
    est_err = 0
    int_qst = [GOOD_QUALITY]*3
    nxt_int_qst = [GOOD_QUALITY]*3
    for i in range (3): # convert WAITING_TO_GO_GOOD to POOR_QUALITY
        if (quality_state[i] != GOOD_QUALITY): int_qst[i] =  POOR_QUALITY
        if (next_quality_state[i] != GOOD_QUALITY): nxt_int_qst[i] =  POOR_QUALITY

    if current_TS == brm_line.TS: # check errors only at brm tick
        for i in range (3): qst_err += int (brm_qst[i] != int_qst[i] and brm_qst[i] != nxt_int_qst[i])
        est_err = int (brm_line.encoder_state != encoder_state and brm_line.encoder_state != next_encoder_state)

    alt_qst_err = 0
    alt_est_err = 0
    if current_TS == brm_line.TS: # check errors only at brm tick
        for i in range (3): alt_qst_err += int (brm_qst[i] != alt_quality_state[i] and brm_qst[i] != alt_next_quality_state[i])
        alt_est_err = int (brm_line.encoder_state != alt_encoder_state and brm_line.encoder_state != alt_next_encoder_state)

    #
    # debug and analysis print outs
    #
    i_TS = internal_scheduling_list[HEAD].TS \
        if len (internal_scheduling_list) else service_line.service_transition_TS # arbitrary else clause

    # time stamps
    fout.write (",TS,{t}, u_TS,{ut}, u_TS_i,{uti}, s_TS,{st}, s_TS_i,{sti}, l_TS,{lt}, l_TS_i,{lti}, b_TS,{bt}, b_TS_i,{bti}, i_TS,{it}".format (
        t=current_TS, ut=uplink_line.queue_size_sample_TS, uti=uplink_index,
        st=service_line.service_transition_TS, sti=service_index, lt=latency_line.bp_t2r_receive_TS, lti=latency_index, 
        bt=brm_line.TS, bti=brm_index, it=i_TS))

    # alternate brm implementation states
    fout.write (",a_est,{e}, b_est,{be}, est_err,{ee}, a_ch_pq,{p}, a_ms_e_st_x,{m}, a_qst,{q}, b_qst,{bq}, qst_err,{qe}, ch_dgrd,{d}, ms_ch_dgrd,{td}".format (
        e=alt_encoder_state, be=brm_line.encoder_state, ee= alt_est_err, p=alt_num_of_channels_in_poor_quality_state, m=alt_ms_since_encoder_state_x, 
        q=alt_quality_state, bq=brm_qst, qe=alt_qst_err, d=channel_degraded, td=ms_since_channel_degraded))

    # current brm implementation states
    fout.write (",qst,{q}, n_qst,{nq}, b_qst,{bc}, qst_err,{qe}, est,{e}, n_est,{ne}, b_est,{be}, est_err,{ee}".format (
        q=quality_state, nq=next_quality_state, bc=brm_qst, qe=qst_err, e=encoder_state, ne=next_encoder_state, be=brm_line.encoder_state, ee=est_err))
    fout.write (",qsz,{q}, qsz_m,{qm}, IS_st,{i}, ms_IS_x,{mx}, IS_st_x,{ix}, bp_t2r,{t2r}, est_t2r,{t}, s_est_t2r,{st}".format (
        q=queue_size, qm=queue_size_monitor_working, i=in_service_state, mx=ms_since_in_service_x, ix=in_service_state_x_TS, 
        t2r=bp_t2r, t=est_t2r, st=s_est_t2r))
    fout.write (",ch_pq,{p}, ms_e_st_x,{m}".format (p=num_of_channels_in_poor_quality_state, m=ms_since_encoder_state_x))

    # general 
    fout.write (", brm,{b}".format (b=brm_line))
    fout.write (", sched,{s}".format (s=internal_scheduling_list))

    fout.write ("\n")

    # update states 
    quality_state = deepcopy (next_quality_state)
    encoder_state = deepcopy (next_encoder_state)

    alt_quality_state = deepcopy (alt_next_quality_state)
    alt_encoder_state = deepcopy (alt_next_encoder_state)

    uplink_index = next_uplink_index
    service_index = next_service_index
    brm_index = next_brm_index
    latency_index = next_latency_index

    itr_count += 1
    if (itr_count % 10_000) == 0: 
        print ("Channel Quality state and BRM checks: itr count: ", itr_count)
    # if itr_count == 1_000: break
# while have not exhausted at least one file

fout.close ()

########################################################################################
# resume effectiveness checks
########################################################################################

fout = open (out_dir + "skip_eff_chk_" + tx_name_part + ".csv", "w")

print ("Running resume effectiveness checks")

for i, is_line in enumerate (log_dic["service"] if log_dic["skip"] == None else log_dic["skip"]):
    if log_dic["skip"] == None: # skip_decision file does not exist, so use service file
        resume_ch = is_line.channel
        resume_TS = is_line.service_transition_TS
    else: # use skip_decision file
        resume_ch = is_line.ch
        resume_TS = is_line.resume_TS

    # check if the first 10 packets of the resuming channel have the best or near best delivery time
    ch_resume_index = bisect_left (log_dic["chrx"+str(resume_ch)], resume_TS, key=lambda a: a.tx_TS)

    for j in range (10):
        ch_index = ch_resume_index + j
        if ch_index > len(log_dic["chrx"+str(resume_ch)])-1:
            break # reached the end of the array, no more transmissions to check
        ch_line = log_dic["chrx"+str(resume_ch)][ch_index]
        pkt_num = ch_line.pkt_num

        # find this packet in the dedup array
        dd_index = bisect_left (log_dic["dedup"], pkt_num, key = lambda a: a.pkt_num)
        if (dd_index == len (log_dic["dedup"])) or (log_dic["dedup"][dd_index].pkt_num != pkt_num):
            err_str = "WARNING Resume effectiveness check: Could not find pkt {p} in dedup array. Res Ch={c} Res_TS={t}\n".format (
                p=pkt_num, c=resume_ch, t=resume_TS)
            sys.stderr.write (err_str)
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
        fout.write ("ch,{c}, is_TS,{t}, {i}, ch_idx,{ci}, pkt#,{p}, ch_rx_TS,{crt}, dd_rx_TS,{drt}, c-d,{d},".format (
            c=resume_ch, t=resume_TS, i=j, ci=ch_index, p=pkt_num, crt=resume_rx_TS, drt=dd_rx_TS, d=diff))
        if (log_dic["skip"] != None):
            fout.write ("qsz,{q}, ch-x{x}, ch_x_is,{xis}, lrp,{lrp}, lrp_bp_TS,{lrpt}, c2r,{c2r}, t2r,{t2r}, c2t,{c2t},".format (
                q=is_line.qsize, x=[is_line.ch0_x, is_line.ch1_x, is_line.ch2_x], 
                xis=[is_line.ch0_IS, is_line.ch1_IS, is_line.ch2_IS], lrp=is_line.lrp_num, lrpt=is_line.lrp_bp_TS,
                c2r=dd_c2r, t2r=resume_t2r, c2t=resume_c2t))
        fout.write ("\n")

    # for the first 10 transmissions of the resuming channel

    if i % 10000 == 0: 
        print ("Resume effectiveness checks @ skip_decision line:", i)

# for each service transition

fout.close ()

########################################################################################
# Resume algo checks
########################################################################################

fout = open (out_dir + "skip_algo_chk_" + tx_name_part + ".csv", "w")

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

    lrp_num = max_bp_pkt_num_list[index]
    lrp_bp_TS = log_dic["all_latency"][index].bp_t2r_receive_TS # this is closest bp pkt to service_transition_TS

    channel = {"x": [0]*3, "t2r": [0]*3, "lrp_num": [0]*3, "lrp_bp_TS": [0]*3}

    while (index >= 0) and (lrp_num == max_bp_pkt_num_list[index]):
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

    # find the last service transition prior to this resume
    channel.update ({"found_last_state_x": [0]*3, "last_state_x": [0]*3, "last_state_x_TS": [0]*3})
    index = service_index-1 # skip the resuming service state transition being processed
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

    # compute the number of packets to be skipped
    if (sum(channel["x_IS"]) == 0) or (skip_line.qsize==0):
        skip = 0
    else:
        skip = int(1.5 * frame_sz_list[lrp_num].frame_szP)
    # skip field no longer in the 
    # skip_diff = skip - skip_line.skip 

    # temporary deubgging filters
    ignore = int (skip_line.resume_TS == skip_line.lrp_bp_TS)

    """
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
    
    # if there are multiple candidates for lrp, pick according the the table below
    # pkt_nums  TS
    #  eq       eq              send any
    #  eq       not eq          send smaller TS
    # not eq    eq              send larger pkt num
    # not eq    not eq          send smaller TS
    channel.update({"lrp_tx_TS": [0]*3, "lrp_tx_index": [0]*3})
    for i in range (3):
        if (channel["x_IS"][i]):
            # tx_index = bisect_left (log_dic["chrx"+str(i)], channel["lrp_num"][i], key = lambda a: a.pkt_num)
            # channel["lrp_tx_index"][i] = tx_index
            # channel["lrp_tx_TS"][i] = log_dic["chrx"+str(i)][tx_index].tx_TS
            channel["lrp_tx_TS"][i] = channel["lrp_bp_TS"][i] -30 - channel["t2r"][i]
            channel["lrp_tx_index"][i] = bisect_left (log_dic["chrx"+str(i)], channel["lrp_tx_TS"][i], key = lambda a: a.tx_TS)

    # if the lrp_tx_TS are not equal then the lrp channel is the one with the smallest TS
    lrp_TS_are_equal = 1
    found_a_candidate = 0
    lrp_channel = 0 # defined just for debug print out so that skip=0 case does not overwrite it. 
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

    # revised skip calculation: skip packets transferred between lrp_tx_TS and lrp_tx_TS + 60 + (resume_TS-lrp_bp_TS)
    # first calculate packets to be skipped relative to lrp_num (calculations are in valid if channel["x_IS"] = [0,0,0])
    chrx_a = log_dic["chrx"+str(lrp_channel)]
    lrp_tx_TS = channel["lrp_tx_TS"][lrp_channel]
    lrp_bp_to_sx_delta = service_line.service_transition_TS - channel["lrp_bp_TS"][lrp_channel] if sum (channel["x_IS"]) != 0 else 0
    last_skip_TS = lrp_tx_TS + 60 + lrp_bp_to_sx_delta if sum (channel["x_IS"]) != 0 else 0
    lrp_tx_index = channel["lrp_tx_index"][lrp_channel]
    if channel["lrp_num"][1] == 3107:
        print ("debug")
    index = lrp_tx_index
    index_no_retx = index
    while index < len(chrx_a) and chrx_a[index].tx_TS <= last_skip_TS:
        retx = chrx_a[index].retx
        index += 1
        if retx == 0:
            index_no_retx = index
    last_skip_pkt_num = chrx_a[index_no_retx-1].pkt_num
    skip_from_lrp_num = index_no_retx - lrp_tx_index
    unretired_skippable_packets = max (0, last_skip_pkt_num + 1 - skip_line.first_pkt_in_Q)
    # skip_from_lrp_num = int (0.8 * skip_from_lrp_num) # guardbanded to not trigger too many retxi
    if sum (channel["x_IS"]) == 0: 
        skip = 0
    else: 
        skip = min (skip_from_lrp_num, skip_line.qsize, MAX_SKIP_PACKETS, unretired_skippable_packets)

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
    # resuming channel
    fout.write ("ch,{c}, Res_TS,{t},".format (
        c=skip_line.ch, t=skip_line.resume_TS))
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
    # skip calculation 
    fout.write ("skip_from_lrp,{s}, last_skip_pkt,{n}, s_last_skip_pkt,{sn}, last_skip_TS,{t}, s_last_skip_TS,{st}, s_qsz,{q}, s_first_pkt_in_Q,{f},".format (
        s=skip_from_lrp_num, n=last_skip_pkt_num, sn=skip_line.last_skip_pkt_num, t=last_skip_TS, st=skip_line.last_skip_TS, q=skip_line.qsize, f=skip_line.first_pkt_in_Q))
    fout.write ("skip,{s}, s_skip,{ss},".format (s=skip, ss=skip_line.skip))
    # fout.write ("skip_from_lrp,{s}, last_skip_pkt,{n}, s_last_skip_pkt,{sn}, res_pkt,{r}, s_res_pkt,{sr}, qsz,{q},".format (
    #     s=skip_from_lrp_num, n=last_skip_pkt_num, sn=skip_line.last_skip_pkt_num, r=algo_resume_pkt_num, sr=impl_resume_pkt_num, q=skip_line.qsize))
    # fout.write ("lrp_tx_idx,{i},".format (i=lrp_tx_index))
    fout.write ("lrp_tx_TS,{strt}, delta,{d}, last_skip_TS,{stpt},".format (strt=lrp_tx_TS, d=lrp_bp_to_sx_delta, stpt=last_skip_TS))

    """
    # check against implementation
    fout.write ("err,{e},".format (e=err))

    # efficiency calculations
    fout.write ("o_res_pkt,{rp}, o_res_TS,{rt}, n_res_pkt,{nrp}, n_res_TS,{nrt}, c-d,{d}, retx,{r}".format (
        rp=resume_pkt_num, rt=resume_rx_TS, nrp=dd_new_resume_pkt_num, nrt=dd_new_resume_rx_TS, d=diff, r=resume_tx_retx))
    """

    fout.write ("\n")

    # process next service line
    if service_index % 1000 == 0: 
        print ("Resume algo checks @ service index: ", service_index)

    # if service_index == 1000: 
    #    break

    service_index += 1
    skip_index += 1

# while there are more service transitions to be analyzed
fout.close ()

exit ()

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
