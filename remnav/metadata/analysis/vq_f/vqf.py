import sys
import math
import time
from operator import itemgetter
import bisect
from collections import namedtuple
from copy import deepcopy

def read_log_file (filename, tuplename):
    """ returns an array (list) of specified namedtuples from the data in the filename"""
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

    return array

def read_array_line (array, TS_field): 
    """ returns a valid line from the array by doing sanity checking of TS_field"""
    for i, line in enumerate (array):
        if line.TS_field == 0:
            pass
        else: 
            yield line

def read_line (array):
    i = 0
    while i < 3:
        yield (array[i]) 
        i += 1

indir = "C:/Users/gopal/Downloads/05_12_2023/"
outdir = "C:/Users/gopal/Downloads/analysis_output/"

# tx and rx file prefix
tx_namepart = "2023_05_12_14_31_29_v11_8_4"
rx_namepart = "2023_05_12_14_31_34_v_9_7_3_online"

#
# log and csv file structures
#
files_dic_fields = namedtuple ("files_dic_fields", "filename, fields")
files_dic = {}
log_dic = {}

# uplink log
# uplink_queue. ch: 1, timestamp: 1681947064182, queue_size: 316, elapsed_time_since_last_queue_update: 9, actual_rate: 0
uplink_fields = namedtuple ("uplink_fields", "channel, queue_size_sample_TS, queue_size, \
                             elapsed_time_since_last_queue_update, actual_rate")
files_dic.update ({"uplink":  files_dic_fields._make ([indir+"uplink_queue_"+tx_namepart+".log", uplink_fields])})
uplink_array = []
log_dic.update ({"uplink": uplink_array})

# latency log
# ch: 0, received a latency, numCHOut:2, packetNum: 4294967295, latency: 40, time: 1681947064236, sent from ch: 0
# receive_TS is the time when the back propagated t2r info is received by the vehicle
latency_fields = namedtuple ("latency_fields", "receiving_channel, numCHOut, PktNum, bp_t2r, bp_t2r_receive_TS, reporting_channel")
files_dic.update ({"latency": files_dic_fields._make ([indir+"latency_"+tx_namepart+".log", latency_fields])})
latency_array = []
log_dic.update ({"latency": latency_array})

# service log
# CH: 2, change to out-of-service state, latency: 0, latencyTime: 0, estimated latency: 2614851439, stop_sending flag: 0 , uplink queue size: 0, zeroUplinkQueue: 0, service flag: 0, numCHOut: 1, Time: 1681947064175, packetNum: 0
service_fields = namedtuple ("service_fields", "channel, bp_t2r, bp_t2r_receive_TS, est_t2r, stop_sending_flag, \
                             uplink_queue_size, zeroUplinkQueue, service_flag, numCHOut, service_transition_TS, bp_t2r_packetNum")
files_dic.update ({"service": files_dic_fields._make ([indir+"service_"+tx_namepart+".log", service_fields])})
service_array = []
log_dic.update ({"service": service_array})

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
files_dic.update ({"probe":   files_dic_fields._make ([indir+"probe_"+rx_namepart+".log",probe_fields])})
# print (files_dic)
probe_array = []
log_dic.update ({"probe": probe_array})

# carrier csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 ch	 latency
# 0	             8.61157E+14	     1.68195E+12	     1384	             1	              0	             0	         0	                 0	         1.68195E+12	     0	     0	 330

# dedup csv
# packe_number	 sender_timestamp	 receiver_timestamp	 video_packet_len	 frame_start	 frame_number	 frame_rate	 frame_resolution	 frame_end	 camera_timestamp	 retx	 ch	 latency
# 0	             8.61156E+14	     1.68195E+12	     1384	             1	             0	             0	         0	                 0	         1.68195E+12	     0	     2	    37
"""

# check that files_dic and log_dic are consistent
files_dic_keys = set (list (files_dic.keys()))
log_dic_keys = set (list (log_dic.keys()))
if (files_dic_keys != log_dic_keys): 
    print ("keys don't match")
    print ("files_dic_keys: ", files_dic_keys)
    print ("log_dic keys:   ", log_dic_keys)
else:
    print ("filename dictionary is consistent with dictionary containing the actual logs")

# read all the log data files. Each log is stored as list of named tupel defe
fout = open (outdir+"delete_me_test.csv", "w")
for item in files_dic:
    print ("reading file: ", files_dic[item].filename)
    log_dic[item] = read_log_file (files_dic[item].filename, files_dic[item].fields)
    print ("\t file lenght = {}".format (len(log_dic[item])))

# clean up latency file to retain channel to channel communication only
print ("Removing unnecessary lines from the latency file")
print ("Original latency file length: {}".format (len(log_dic["latency"])))
new_list = []
for i, line in enumerate (log_dic["latency"]):
    if i % 100_000 == 0: 
        print ("processing line {}".format (i))
    if line.receiving_channel == line.reporting_channel: 
        new_list += [(line)]
log_dic["latency"] = new_list
print ("After removing unnecessary lines, latency file lenght: {}".format (len(log_dic["latency"])))

# In_service state machine states
IN_SERVICE = 1
OUT_OF_SERVICE = 0
WAITING_TO_GO_OUT_OF_SERVICE = 2

# channel quality state machine states
GOOD_QUALITY = 0
POOR_QUALITY = 2
WAITING_TO_GO_GOOD = 3

# set epoch times and other initial conditions 
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

itr_count = 0
while latency_index < len(log_dic["latency"]) and uplink_index < len(log_dic["uplink"]) and service_index < len(log_dic["service"]):
    
    #
    # set up next clock tick when the state transition should be evaluated
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
            bp_t2r[latency_line.receiving_channel] = latency_line.bp_t2r
            bp_t2r_receive_TS[latency_line.receiving_channel] = latency_line.bp_t2r_receive_TS
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
    # synthesized inputs
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

    ########################################################################################
    # In_service state machine
    ########################################################################################
    next_channel_service_state = [st for st in channel_service_state]
    service_state_transition = [0]*3
    for i in range (3):

        num_of_channels_in_service = sum (int (x != OUT_OF_SERVICE) for x in next_channel_service_state)

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
    # for In_service state machine of each channel
    channel_service_state = [st for st in next_channel_service_state]

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

fout.close ()
exit ()



"""
for i, line in enumerate (log_dic["latency"]):
    print (line)
    print (new_list[i])
    if i == 3:
        break

fout = open (outdir+"delete_me_test.csv", "w")
for line in log_dic["latency"]:
    fout.write("receving_channel, {}, reporting_chanel, {}, PktNum, {}, receive_TS, {}\n".format ( \
        line.receiving_channel, line.reporting_channel, line.PktNum, line.receive_TS))
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