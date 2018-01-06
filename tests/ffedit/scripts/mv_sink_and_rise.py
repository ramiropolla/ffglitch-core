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
            # if ffedit exported motion vectors
            if "mv" in frame:
                mv = frame["mv"]
                if "forward" in mv:
                    fwd = mv["forward"]
                    # for each row
                    for y in fwd:
                        for i,x in enumerate(y):
                            if not x:
                                continue
                            if isinstance(x[0], (int, long)):
                                # one mv per mb
                                y[i][0] = 0
                            else:
                                # more than one mv per mb
                                for j,b in enumerate(x):
                                    x[j][0] = 0
    print json.dumps(data)
