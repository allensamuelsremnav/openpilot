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

do_spike_analysis = 1

out_dir = "C:/Users/gopal/Downloads/analysis_output/"
capture_list = read_worklist ("C:/Users/gopal/Downloads/06_17_2023/file_list.txt")

for capture in capture_list:
    # read input logs
    files_dic, log_dic = create_dic (tx_infix = capture.tx_infix, rx_infix = capture.rx_infix, in_dir = capture.in_dir) 
    read_files (log_dic = log_dic, files_dic = files_dic)
    create_self_bp_list (log_dic)
    create_max_bp_list (log_dic)
    create_chrx_sorted_by_pkt_num (log_dic)
    remove_network_duplicates (log_dic)
    
    if (do_spike_analysis): spike_analysis (log_dic=log_dic, capture=capture, out_dir=out_dir)

# for all files in the capture list

exit ()