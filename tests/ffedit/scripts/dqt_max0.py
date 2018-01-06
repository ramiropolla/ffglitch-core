#!/usr/bin/env python

import json
import sys

with open(sys.argv[1], 'r') as f:
    data = json.load(f)
    # for each stream (normally there is only one stream of interest)
    for stream in data["streams"]:
        # for each frame in the stream
        for frame in stream["frames"]:
            # if ffedit exported DQT
            if "dqt" in frame:
                dqt = frame["dqt"]["data"]
                dqt[0][0] = 63
    print json.dumps(data)
