#!/usr/bin/env python

import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)
    # for each stream (normally there is only one stream of interest)
    for stream in data["streams"]:
        # for each frame in the stream
        for frame in stream["frames"]:
            # if ffedit exported quantized DCT coefficients
            if "q_dct" in frame:
                dct_data = frame["q_dct"]["data"]
                # for all 3 planes (y, cb, cr)
                for plane in dct_data:
                    # for each row
                    for i,y in enumerate(plane):
                        for j,x in enumerate(y):
                            if not x:
                                continue
                            y_ac = sorted(y[j][1:])
                            y[j] = [ y[j][0] ] + y_ac
    print json.dumps(data)
