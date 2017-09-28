#!/usr/bin/env python

import random
import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)
    # for each stream (normally there is only one stream of interest)
    for stream in data["streams"]:
        # for each frame in the stream
        for frame in stream["frames"]:
            # if cas9 exported motion vectors
            if "mv" in frame:
                mv = frame["mv"]
                if "forward" in mv:
                    fwd = mv["forward"]
                    # for each row
                    for y in fwd:
                        for i,x in enumerate(y):
                            y[i][0] = 0
    print json.dumps(data)
