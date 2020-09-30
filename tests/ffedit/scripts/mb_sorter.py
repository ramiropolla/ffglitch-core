#!/usr/bin/env python

import itertools
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
                    mb_data = mb["data"]
                    unpacked = list(itertools.chain.from_iterable(mb_data))
                    unpacked.sort()
                    n = len(mb_data[0])
                    repacked = [unpacked[i:i + n] for i in range(0, len(unpacked), n)]
                    mb["data"] = repacked
    print json.dumps(data)
