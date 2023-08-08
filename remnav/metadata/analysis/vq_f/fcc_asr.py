import sys
import math
import time
from pathlib import Path
from operator import itemgetter
from bisect import bisect_left, bisect_right
from collections import namedtuple
from copy import deepcopy

degree_coord = namedtuple ("degree_cord", "degrees, minutes, seconds")

#
# main
#
out_dir = "C:/Users/gopal/Downloads/analysis_output/"
fout = open (out_dir+"south_bay_asr_download_out.csv", "w")
input_file_list = ["C:/Users/gopal/Downloads/south_bay_asr_download.txt"]

asr_dic = {}
for file in input_file_list:

    # RA section
    REGISTRATION_IDX = 4-1
    DATE_DISMANTLED_IDX = 14-1
    STREET_IDX = 24-1
    CITY_IDX = 25-1
    STATE_IDX = 26-1
    ZIP_IDX = 28-1
    OVERALL_HEIGHT_IDX = 31-1
    STRUCTURE_TYPE_IDX = 33-1

    dup_count = 0
    skip_count = 0
    fin = open (file, "r")
    ra_lines = (line for line in fin if line.startswith ("RA"))
    for i, line in enumerate (ra_lines):
        # read fileds
        ra_fields = line.split ("|")
        registration = ra_fields[REGISTRATION_IDX]
        address = f"{ra_fields[STREET_IDX]}-{ra_fields[CITY_IDX]}-{ra_fields[STATE_IDX]}-{ra_fields[ZIP_IDX]}".replace (',', ' ')
        type = ra_fields[STRUCTURE_TYPE_IDX]
        height = float (ra_fields[OVERALL_HEIGHT_IDX])
        # checks
        if registration in asr_dic: 
            print (f"Duplicate ASR registration {registration}.Skipping\n")
            dup_count += 1
            continue
        if not (ra_fields[DATE_DISMANTLED_IDX] ==  ''):
            # print (f"ASR {registration} has been dismantled. Skipping\n")
            skip_count += 1
            continue
        # if checks pass then add it to the dictonary
        asr_dic.update ({registration: [address, f"type,{type}", f"height,{height}"]})
    # for all RA lines
    print (f"Number of ASRs in RA section: {i}. {skip_count} Dismanteled entries. {dup_count} duplicate entries")

    # EN section
    ENTITY_NAME_IDX = 10-1
    skip_count = 0
    dup_count = 0
    np_count = 0
    en_list = []
    fin = open (file, "r")
    en_lines = (line for line in fin if line.startswith ("EN"))
    for i, line in enumerate (en_lines):
        # read relevant fields
        en_fields = line.split ("|")
        registration = en_fields[REGISTRATION_IDX]
        entity_name = en_fields[ENTITY_NAME_IDX].replace (',', ' ')
        # checks
        if not registration in asr_dic:
            skip_count += 1
            continue
        if registration in en_list:
            dup_count += 1
            # overwrite the current value as we have seen the second entry to be more current
            asr_dic[registration][-1] = entity_name
        else :
            # if checks pass then add entity name to the dictionary
            asr_dic[registration] += [entity_name]
            en_list += [registration]
        asr_dic_len = len (asr_dic[registration])
    # for all EN lines
    # check and fill in for not present in EN 
    for items in asr_dic:
        if len (asr_dic[items]) < asr_dic_len: # entity fields was missing
            asr_dic[items] += ['']
            np_count += 1
    print (f"EN section: {skip_count} entries missing in dic; {np_count} entries missing in EN. {dup_count} duplicates")
    
    # CO section
    LAT_DEGREES_IDX = 7-1
    LAT_MINUTES_IDX = 8-1
    LAT_SECONDS_IDX = 9-1
    LON_DEGREES_IDX = 12-1
    LON_MINUTES_IDX = 13-1
    LON_SECONDS_IDX = 14-1

    skip_count = 0
    dup_count = 0
    np_count = 0
    co_list = []
    fin = open (file, "r")
    co_lines = (line for line in fin if line.startswith ("CO"))
    for i, line in enumerate (co_lines):
        # read relevant fields
        co_fields = line.split ("|")
        registration = co_fields[REGISTRATION_IDX]
        # checks
        if registration in co_list:
            dup_count += 1
            continue
        if not registration in asr_dic: 
            skip_count += 1
            continue
        try: 
            lat = degree_coord (float (co_fields[LAT_DEGREES_IDX]), float (co_fields[LAT_MINUTES_IDX]), float(co_fields[LAT_SECONDS_IDX]))
        except: # missing or invalid value
            skip_count += 1
            continue
        try: 
            lon = degree_coord (float (co_fields[LON_DEGREES_IDX]), float (co_fields[LON_MINUTES_IDX]), float(co_fields[LON_SECONDS_IDX]))
        except: # missing or invalid value
            skip_count += 1
            continue
        # if checks pass then add lat/lon to the dictionary
        lat_decimal = f"lat, {lat.degrees + (lat.minutes + lat.seconds/60)/60:.4f}"
        lon_decimal = f"lon, {-1*(lon.degrees + (lon.minutes + lon.seconds/60)/60):.4f}"
        asr_dic[registration] += [lat_decimal, lon_decimal]
        co_list += [registration]
        asr_dic_len = len (asr_dic[registration])
    # for all CO lines
    for items in asr_dic:
        if len (asr_dic[items]) < asr_dic_len: # entity fields was missing
            asr_dic[items] += ['','']
            np_count += 1
    print (f"CO section: {skip_count} entries missing in dic; {np_count} entries missing in CO. {dup_count} duplicates")

    # OUTPUT
    # for items in asr_dic: print (f"{items}: {asr_dic[items]}")
    for items in asr_dic: 
        fout.write (f"{items},{','.join (asr_dic[items])}\n")

fout.close ()
# for all files in input file list
