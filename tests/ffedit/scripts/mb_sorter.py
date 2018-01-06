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
            # if ffedit exported macroblocks
            if "mb" in frame:
                mb = frame["mb"]
                if "data" in mb:
                    mb["data"].sort()
    print json.dumps(data)
