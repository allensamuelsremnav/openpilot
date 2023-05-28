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
        return None

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
        return None

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

#########################################################################################
# in/out directory and file prefix/suffix
#########################################################################################
in_dir = "C:/Users/gopal/Downloads/05_22_2023/"
out_dir = "C:/Users/gopal/Downloads/analysis_output/"

# rx_name_part = "2023_05_22_14_24_18_v_9_7_3_online"
# tx_name_part = "2023_05_22_14_24_13_v11_8_4"

rx_name_part = "2023_05_22_14_41_50_v_9_7_3_online"
tx_name_part = "2023_05_22_14_41_45_v11_8_4"

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
"""
# uplink log
# uplink_queue. ch: 1, timestamp: 1681947064182, queue_size: 316, elapsed_time_since_last_queue_update: 9, actual_rate: 0
uplink_fields = namedtuple ("uplink_fields", "channel, queue_size_sample_TS, queue_size, \
                             elapsed_time_since_last_queue_update, actual_rate")
files_dic.update ({"uplink":  files_dic_fields._make ([in_dir+"uplink_queue_"+tx_name_part+".log", uplink_fields])})
uplink_array = []
log_dic.update ({"uplink": uplink_array})
"""

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

# skip decision log
# CH: 0, Skip  1.5x (60ms) frame worth of packets. Method: 0, packetSent[lastPacketRetired]: 1, skip: 0, qsize: 10, framePacketNum: 0,  lastRetiredCH: 1, lastPacketRetired: 0, lastPacketRetiredTIme: 0, ch0 service: 1, ch1 service: 1, ch2 service: 0, ch0In: 1, ch1In : 1, ch2In: 0, ch0Matched:1, ch1Matched:1, ch2Matched:1, ch0-x:1, ch1-x:1, ch2-x:1, ch0InTime:1684521522628, ch1InTime: 1684521522572, ch2InTime:1684521517919, time: 1684521522628
skip_fields = namedtuple ("skip_fields", "ch, method, ignore1, skip, qsize, szP,  ignore2, lrp, lrp_bp_TS, ch0_IS_now, ch1_IS_now, ch2_IS_now, ch0_IS, ch1_IS, ch2_IS, ch0Matched, ch1Matched, ch2Matched, ch0_x, ch1_x, ch2_x, ch0InTime, ch1InTime, ch2InTime, resume_TS")
files_dic.update ({"skip": files_dic_fields._make ([in_dir+"skip_decision_"+tx_name_part+".log", skip_fields])})
skip_array = []
log_dic.update ({"skip": skip_array})

"""
# retx log
# ch: 2, received a retx, numCHOut:2, startPacketNum: 39753, run: 1, time: 1681946093091

# bitrate log
# send_bitrate: 830000, encoder state: 2, ch0 quality state: 1, ch1 quality state: 1, ch2 quality state: 1, time: 1681947065306

# avgQ log
# RollingAvg75. Probe. CH: 2, RollingAvg75: 0.000000, qualityState: 1, queue size: 0, time: 1681947064175

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

fout = open (out_dir + "skip_eff_chk_" + tx_name_part + ".csv", "w")

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

# sort chrx arrays by tx_TS
for i in range(3):
    log_dic["chrx"+str(i)].sort (key = lambda a: a.tx_TS) 
    log_dic.update({"chrx_sorted_by_pkt_num"+str(i): sorted(log_dic["chrx"+str(i)], key = lambda a: a.pkt_num)})

########################################################################################
# resume effectiveness checks
########################################################################################

for i, is_line in enumerate (log_dic["service"] if log_dic["skip"] == None else log_dic["skip"]):
    if log_dic["skip"] == None: # skip_decision file does not exist, so use service file
        resume_ch = is_line.channel
        resume_TS = is_line.service_transition_TS
        skip = 0
    else: # use skip_decision file
        resume_ch = is_line.ch
        resume_TS = is_line.resume_TS
        skip = is_line.skip

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
            err_str = "WARNING Resume effect. check: Could not find pkt {p} in dedup array. Res Ch={c} Res_TS={t}\n".format (
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
        fout.write ("ch,{c}, is_TS,{t}, skip,{s}, {i}, ch_idx,{ci}, pkt#,{p}, ch_rx_TS,{crt}, dd_rx_TS,{drt}, c-d,{d},".format (
            c=resume_ch, t=resume_TS, s=skip, i=j, ci=ch_index, p=pkt_num, crt=resume_rx_TS, drt=dd_rx_TS, d=diff))
        if (log_dic["skip"] != None):
            fout.write ("qsz,{q}, ch-x{x}, ch_x_is,{xis}, lrp,{lrp}, lrp_bp_TS,{lrpt}, c2r,{c2r}, t2r,{t2r}, c2t,{c2t},".format (
                q=is_line.qsize, x=[is_line.ch0_x, is_line.ch1_x, is_line.ch2_x], 
                xis=[is_line.ch0_IS, is_line.ch1_IS, is_line.ch2_IS], lrp=is_line.lrp, lrpt=is_line.lrp_bp_TS,
                c2r=dd_c2r, t2r=resume_t2r, c2t=resume_c2t))
        fout.write ("\n")

    # for the first 10 transmissions of the resuming channel

    if i % 1000 == 0: 
        print ("Resume effectiveness checks @ skip_decision line:", i)

# for each service transition

fout.close ()

########################################################################################
# Resume algo checks
########################################################################################

fout = open (out_dir + "skip_algo_chk_" + tx_name_part + ".csv", "w")

service_index = 0
skip_index = 0

while service_index < len(log_dic["service"]):

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

    # if service_line.service_transition_TS == 1684791863076: 
        # print ("debug")
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
    skip_line = log_dic["skip"][skip_index]
    if (sum(channel["x_IS"]) == 0) or (skip_line.qsize==0):
        skip = 0
    else:
        skip = int(1.5 * frame_sz_list[lrp_num].frame_szP)
    skip_diff = skip - skip_line.skip

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

    #
    # Revised algo 
    # 

    # relax channel_x to incldue other channels that transmitted packets neighboring lrp in porximity of lrp_bp_TS
    SEARCH_TS_WINDOW = 20
    SEARCH_PKT_WINDOW = 2
    start_TS = lrp_bp_TS - SEARCH_TS_WINDOW
    start_TS_index = bisect_left (log_dic["all_latency"], start_TS, key = lambda a: a.bp_t2r_receive_TS)
    stop_TS = min (lrp_bp_TS + SEARCH_TS_WINDOW, skip_line.resume_TS)
    stop_TS_index = bisect_left (log_dic["all_latency"], stop_TS, key = lambda a: a.bp_t2r_receive_TS)

    channel.update({"x_debug": [0]*3, "lrp_tx_TS": [0]*3, "lrp_tx_index": [0]*3})
    for i in range (3):
        tx_index = bisect_left (log_dic["chrx"+str(i)], channel["lrp_num"][i], key = lambda a: a.pkt_num)
        channel["lrp_tx_index"][i] = tx_index
        channel["lrp_tx_TS"][i] = log_dic["chrx"+str(i)][tx_index].tx_TS

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
    if (service_line.service_transition_TS == 1684791711664):
        print ("debug")
    channel["x_IS"] = [0]*3
    channel.update({"x_IS_debug": [0]*3})
    for i in range (3):
        channel["x_IS"][i] = int (
            # channel supplied lrp
            channel["x"][i] and 
            # channel is in service now
            channel["last_state_x"][i] and 
            # channel has been in service for atleast 30ms prior to resume time
            channel["last_state_x_TS"][i] <= (service_line.service_transition_TS - 30))
        channel["x_IS_debug"][i] = int (
            channel["last_state_x_TS"][i] <= (service_line.service_transition_TS - 30)  and 
            not (channel["last_state_x_TS"][i] <= (channel["lrp_bp_TS"][i] -30 - channel["t2r"][i])))
    
    # if there are multiple candidates for lrp, pick the one that retired the largest pkt_num the earliest
    lrp_channel = 0 # really defined for debug print out
    found_a_candidate = False
    for i in range (3):
        if channel["x_IS"][i] and (found_a_candidate==False or channel["lrp_num"][i] > max_lrp_num): 
            found_a_candidate = True
            max_lrp_num = channel["lrp_num"][i]
            lrp_channel = i
    found_a_candidate = False
    for i in range (3):
        if channel["x_IS"][i] and (found_a_candidate==False or channel["lrp_tx_TS"][i] < min_lrp_tx_TS): 
            found_a_candidate = True
            min_lrp_tx_TS = channel["lrp_tx_TS"][i]
            lrp_channel = i

    # revised skip calculation
    if (sum(channel["x_IS"]) == 0) or (skip_line.qsize==0):
        skip = 0
    else:
        # skip packets transferred between lrp_tx_TS and lrp_tx_TS + 60 + (resume_TS-lrp_bp_TS)
        if (channel["lrp_num"][lrp_channel] == 3421):
            print ("debug")
        chrx_a = log_dic["chrx"+str(lrp_channel)]
        # lrp_tx_index below returns lrp number. Should really be lrp+1 but that may not exist
        # as the lrp_channel may have gone out of service
        lrp_tx_index = channel["lrp_tx_index"][lrp_channel]
        lrp_tx_TS = channel["lrp_tx_TS"][lrp_channel]
        lrp_to_sx_delta = service_line.service_transition_TS - channel["lrp_bp_TS"][lrp_channel]
        lrp_tx_skip_TS = lrp_tx_TS + 60 + lrp_to_sx_delta
        skip = 0
        index = lrp_tx_index
        while index < len(chrx_a) and chrx_a[index].tx_TS <= lrp_tx_skip_TS:
            skip += 1
            index += 1
            # if (lrp_tx_skip_TS==1684791842928):
                # print (chrx_a[index].tx_TS, index, chrx_a[index].pkt_num, skip)
        # skip = int (0.8 * skip) # guardbanded to not trigger too many retx
        skip = int (skip) # debug guardbanded to not trigger too many retx

        # now check if the resuming channel was effective
        chrx_a = log_dic["chrx"+str(service_line.channel)]
        if service_line.service_transition_TS == 1684791737187:
            print ("debug")
        resume_tx_index = bisect_left (chrx_a, service_line.service_transition_TS, key = lambda a: a.tx_TS)
        resume_pkt_num = chrx_a[resume_tx_index].pkt_num
        resume_tx_retx = chrx_a[resume_tx_index].retx
        resume_rx_TS = chrx_a[resume_tx_index].rx_TS
    
        dd_new_resume_index = bisect_left (log_dic["dedup"], channel["lrp_num"][lrp_channel] + 1 + skip, key = lambda a: a.pkt_num)
        try:
            dd_new_resume_rx_TS = log_dic["dedup"][dd_new_resume_index].rx_TS
        except:
            print ("index out of range")
        dd_new_resume_pkt_num = log_dic["dedup"][dd_new_resume_index].pkt_num
        diff = resume_rx_TS - dd_new_resume_rx_TS

    # debug print outs
    sum_ch_x = sum(channel["x"])
    fout.write (",ch-x,{x}, sum_ch-x,{sx}, x_IS,{x_IS}, x_IS_dbg,{x_IS_d}, lrp_ch,{lrp_ch}, skip,{s},".format ( 
       x=channel["x"], sx=sum_ch_x, x_IS=channel["x_IS"], x_IS_d=channel["x_IS_debug"], lrp_ch=lrp_channel, s=skip))
    fout.write ("lrp_num,{n}, lrp_bp_TS,{bp_t}, lrp_tx_TS,{tx_t},".format (
        n=channel["lrp_num"], bp_t=channel["lrp_bp_TS"], tx_t=channel["lrp_tx_TS"]))
    fout.write ("x_debg,{x}, srch_strt_TS,{s1}, srch_stp_TS,{s2}, src_strt_idx,{i1}, srch_stp_idx,{i2},".format (
        x=channel["x_debug"], s1=start_TS, s2=stop_TS, i1=start_TS_index, i2=stop_TS_index))
    if (skip):
        fout.write ("lrp,{l}, lrp_tx_idx,{i},".format (l=channel["lrp_num"][lrp_channel], i=lrp_tx_index))
        fout.write ("start_TS,{stt}, delta,{d}, skip_TS,{stpt},".format (stt=lrp_tx_TS, d=lrp_to_sx_delta, stpt=lrp_tx_skip_TS))
        fout.write ("o_res_pkt,{rp}, o_res_TS,{rt}, n_res_pkt,{nrp}, n_res_TS,{nrt}, c-d,{d}, retx,{r}".format (
            rp=resume_pkt_num, rt=resume_rx_TS, nrp=dd_new_resume_pkt_num, nrt=dd_new_resume_rx_TS, d=diff, r=resume_tx_retx))

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

# channel quality state machine states
GOOD_QUALITY = 0
POOR_QUALITY = 2
WAITING_TO_GO_GOOD = 3

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

channel_quality_state = [GOOD_QUALITY]*3

current_TS = next_external_eval_TS
channel_quality_state_x_TS = [next_external_eval_TS]*3
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
    
    """
    print ("current_TS: ", current_TS)
    if (current_TS == 1683927095731):
        print ("current_TS: ", 1683927095731)
    """
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

    """
    ########################################################################################
    # channel quality state machine
    ########################################################################################
    next_channel_quality_state = deepcopy(channel_quality_state)
    for i in range(3):

        if (queue_size_monitor_working[i] == False):
            channel_quality_state[i] = POOR_QUALITY    

        elif channel_quality_state[i] == GOOD_QUALITY:
            if (queue_size[i] > 10) or (est_t2r[i] > 120):
                channel_quality_state[i], channel_service_state_x_TS[i] = [POOR_QUALITY, current_TS]
        
        elif channel_quality_state[i] == POOR_QUALITY:
            

    # for channel quality state machine of each channel

    if itr_count == 200:
        break
    """

# while have not exhausted at least one input log file


"""
for i, line in enumerate (log_dic["latency"]):
    print (line)
    print (new_list[i])
    if i == 3:
        break

fout = open (out_dir+"delete_me_test.csv", "w")
for line in log_dic["latency"]:
    fout.write("communicating_channel, {}, sending_channel, {}, PktNum, {}, receive_TS, {}\n".format ( \
        line.communicating_channel, line.sending_channel, line.PktNum, line.receive_TS))
fout.close()


##################################################
# start of scratch
##################################################
# print ("uplink len: ", len(log_dic["uplink"]), type (len(log_dic["uplink"])))
# print ("service len: ", len(log_dic["service"]))

fieldname = "uplink"
print ("uplink len2: ", len(log_dic[fieldname]))

print ("reading from generator")
r = read_line (log_dic["service"])
# while (line := next (r, None)) is not None:
    # print (line)


line = (next(r, None))
print (line)
print ("channel: ", line.channel)
print ("bp_t2r: ", line.bp_t2r)

print (next(r, None))
print (next(r, None))
print (next(r, None))

for line in r:
    print (type(line))
    print (line)
print (type(line))
print (line)


exit ()

for i in files_dic:
    print ("\n PRINTING file: ", files_dic[i].filename, " length: ", len(log_dic[i]), "\n")
    for j, log_data in enumerate (log_dic[i]):
        print (log_data)
        if j>3:
            break
##################################################
# end of scratch
##################################################
"""

"""
##################################################
# IN_SERVICE state machine
##################################################

STATE == IN_SERVICE
    transition to OUT_OF_SERVICE
    if (
        (occupancy_monitor_working && (occupancy > 10)) 
        || 
        (est_bp_t2r > 80ms)) 
        && (3 channels in IN_SERVICE_STATES 
        ||
     	// only one channel besides this one is IN_SERVICE
     	Remaining channel has been in service for at least 5ms)
    )
STATE == OUT_OF_SERVICE
    transition to IN_SERVICE 
    if (
        (!occupancy_monitor_working_correctly || (occupancy < 5))
        &&
        ((est_bp_t2r < 50ms))
   )
"""

"""
#
# initialize states to be out of service
#
# current_state 
OUT_OF_SERVICE = 0
IN_SERVICE = 1
# transition reason
# 1 if t2r, 2 if queue size, 3 if both, 0 if undefined 
computed_service_state_fields = namedtuple ("computed_service_state_fields", 
    "current_state, transition_TS, transition_reason")
computed_service_state = {"att": (OUT_OF_SERVICE, 0, 0),
                          "vz":  (OUT_OF_SERVICE, 0, 0),
                          "tm":  (OUT_OF_SERVICE, 0, 0)}

# while there are more entries in uplink and latency log to process
read_uplink_line = read_log_file (log_dic["uplink"], "queue_size_sample_TS")
read_latency_line = read_log_file (log_dic["latency"], "bp_t2r_receive_TS")

uplink_line = next (read_uplink_line, None)
latency_line = next (read_latency_line, None)

while (uplink_line is not None) and (latency_line is not None):
    if uplink_line.queue_size_sample_TS < latency_line.receive_TS:
        # process queue
        pass
    
    # obtain a valid feedback line 
    # if queue size feedback is earlier than t2r feedbback
        # process queue size feedback
        # increment uplink array index
    # else
        # process t2r feedback
        # increment service array index
    # if state change then record the event and the reason
    # find closest (smaller or bigger) state change in the service log
    # generate warning if state change is further than the threshold
    # generate warning if computed state does not match the expected state

for item in files_dic:
    print ("printing file: ", files_dic[item][0])
    for i in files_dic[item][2]:
        print (i)

latency_log = open ("latency.txt", "r")
latency_array = read_log_file (latency_log, files_dic["latency"][1])

print ("latency log", "id=", id(latency_array))
for i, line in enumerate(latency_array):
    print (i, latency_array[i])

# namedtuple_list = [latency_fields, probe_fields]
# print (namedtuple_list)

print ("latency log", "id=", id(latency_array))
for i, line in enumerate(latency_array):
    print(i, line.PktNum)

probe_log = open ("probe.txt", "r")
probe_array = read_log_file (probe_log)
print ("probe log", "id=", id(probe_array))
for line in probe_array:
    print (line)
"""