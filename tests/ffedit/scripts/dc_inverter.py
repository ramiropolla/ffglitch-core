#!/usr/bin/env python

import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)
    # for each stream (normally there is only one stream of interest)
    for stream in data["streams"]:
        # for each frame in the stream
        for frame in stream["frames"]:
            # if ffedit exported quantized DC coefficients
            if "q_dc" in frame:
                dc_data = frame["q_dc"]["data"]
                # for all 3 planes (y, cb, cr)
                for plane in dc_data:
                    # for each row
                    for y in plane:
                        # for each block
                        for i,x in enumerate(y):
                            y[i] = -y[i]
    print json.dumps(data)
