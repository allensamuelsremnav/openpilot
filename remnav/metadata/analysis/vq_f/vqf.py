import math
import time
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy
from support import *
import sys

#
# main 
#
out_dir = "C:/Users/gopal/Downloads/analysis_output/"
sys.stderr = open ("vqf_warnings.log", "w")
capture_list = read_worklist ("C:/Users/gopal/Downloads/07_20_2023/readme.md")
sys.stderr = sys.__stderr__

do_spike_analysis = 0
do_resume_algo_checks = 1
do_brm_algo_checks = 0
do_skip_effecitiveness_checks = 0
do_max_burst_check = 0

for capture in capture_list:
    sys.stderr = open ("vqf_warnings.log", "a")
    # read log/csv files and cleanse the data
    files_dic, log_dic = create_dic (tx_infix=capture.tx_infix, rx_infix=capture.rx_infix, in_dir=capture.in_dir) 
    read_files (log_dic=log_dic, files_dic=files_dic)
    create_self_bp_list (log_dic)
    create_max_bp_list (log_dic)
    create_chrx_sorted_by_pkt_num (log_dic)
    remove_network_duplicates (log_dic)
    sys.stderr = sys.__stderr__ # each check will open its own warning log file
    
    if (do_spike_analysis): spike_analysis (log_dic=log_dic, capture=capture, out_dir=out_dir)
    if (do_resume_algo_checks): resume_algo_check (log_dic=log_dic, capture=capture, out_dir=out_dir)
    if (do_brm_algo_checks): brm_algo_check (log_dic=log_dic, capture=capture, out_dir=out_dir)
    if (do_skip_effecitiveness_checks): skip_effectiveness_check (log_dic=log_dic, capture=capture, out_dir=out_dir)
    if (do_max_burst_check): max_burst_check (log_dic=log_dic, capture=capture, out_dir=out_dir)
    
exit ()