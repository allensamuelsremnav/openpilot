import math
import time
import sys
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy


from support import *

#
# main 
#
out_dir = "C:/Users/gopal/Downloads/analysis_output/"

capture_list = read_worklist ("C:/Users/gopal/Downloads/06_17_2023/file_list.txt")
# capture_list = read_worklist ("todo_list.txt")

for capture in capture_list:
    # read input logs
    files_dic, log_dic = create_dic (tx_infix = capture.tx_infix, rx_infix = capture.rx_infix, in_dir = capture.in_dir) 
    read_files (log_dic = log_dic, files_dic = files_dic)
    create_self_bp_list (log_dic)
    create_max_bp_list (log_dic)
    create_chrx_sorted_by_pkt_num (log_dic)
    remove_network_duplicates (log_dic)
    
    # output
    fout = open (out_dir + "seg3_" + capture.tx_infix + ".csv", "w")
    sys.stderr = open (out_dir + "seg3_" + capture.tx_infix + "_warnings"+".log", "w")

    print ("Running seg 3 analysis on file {f}".format (f=capture.tx_infix))
    
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
        if service_start_line.service_transition_TS == 1686705460014:
            print ("debug")
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
            else:
                break
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
        fout.write ("rq_bw,{b:.2f}, camera_B,{B}, camera_start_pkt{s1}, camera_stop_pkt,{s2},".format(
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

    fout.close ()
# for all files in the capture list

exit ()