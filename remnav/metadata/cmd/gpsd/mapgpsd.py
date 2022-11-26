import os
import argparse
import math
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import random
import json
import folium
from folium import plugins


def read(logfilename):
    """Read raw gpsd log file (JSON for each packet)."""
    # Return pandas.DataFrame
    lat = []
    lon = []
    speed = []
    utc = []
    eph = []
    mode = []
    sky_map = {}
    with open(logfilename) as log:
        pos = set()
        for line in log:
            line_dict = json.loads(line)
            if line_dict["class"] == "TPV":
                if "lat" in line_dict:
                    pos.add((line_dict["lat"], line_dict['lon'], line_dict['speed']))
                    lat.append(line_dict['lat'])
                    lon.append(line_dict['lon'])
                    utc.append(line_dict['time'])
                    speed.append(line_dict['speed'])
                    eph.append(line_dict.get('eph', np.NaN))
                    mode.append(line_dict['mode'])
            elif line_dict["class"] == "SKY":
                if "hdop" in line_dict:
                    sky_map[line_dict['time']] = line_dict['hdop']
    hdop = []
    for t in utc:
        hdop.append(sky_map[t] if t in sky_map else np.NaN)
    df = pd.DataFrame({"utc": pd.to_datetime(utc), 
                       "lat": lat, "lon": lon, "eph": eph, "hdop": hdop,
                       "speed": speed, "mode": mode,
                       "log": os.path.basename(logfilename)})
    return df


def map_config(df):
    center_lat = (df["lat"].max() + df["lat"].min()) / 2
    center_lon = (df["lon"].max() + df["lon"].min()) / 2
    lon_scale = math.cos(math.pi * center_lat / 180)
    lat_range = df["lat"].max() - df["lat"].min()
    lon_range = (df["lon"].max() - df["lon"].min()) * lon_scale
    print(f'center lat,lon {center_lat:.7f},{center_lon:.7f}, lat range {lat_range:.7f}, lon range {lon_range:.7f}')
    map_range = max(lat_range, lon_range)
    raw = int(round(9. - math.log2(map_range)))
    zoom = max(1, min(18, raw))
    return (center_lat, center_lon), zoom
            

def html_map(df):
    """Make folium map of lat/lon in df."""
    center_latlon, zoom = map_config(df)
    m = folium.Map(location=center_latlon, zoom_start=zoom, max_zoom=19)
    #dt = [date ]
    for index, row in df.iterrows():
        tooltip = f'{row["utc"]} {row["speed"]:.1f} m/s, eph {row["eph"]:.1f} m, log {row["log"]}"'
        folium.Circle(
            radius=4,
            location=[row["lat"], row["lon"]],
            popup=tooltip,
            color="blue",
            fill=True,
        ).add_to(m)
    return m


def main():
    """Parse arguments, make HTML map."""
    # python mapgpsd.py ./gpsd.rn5.log
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs='+',
                        help="logged JSON output of GPSD")
    parser.add_argument("--html",
                        default="",
                        help="HTML output file")
    parser.add_argument("--csv",
                        default="",
                        help="csv output file")
    args = parser.parse_args()

    html_filename = args.html
    if not html_filename:
        root, _ = os.path.splitext(args.log[0])
        html_filename = root + ".html"

    csv_filename = args.csv
    if not csv_filename:
        root, _ = os.path.splitext(args.log[0])
        csv_filename = root + ".csv"

    df = pd.concat([read(log) for log in args.log])
    m = html_map(df)
    m.save(html_filename)
    
    df.to_csv(csv_filename, index=False)

if __name__ == "__main__":
    main()
            
