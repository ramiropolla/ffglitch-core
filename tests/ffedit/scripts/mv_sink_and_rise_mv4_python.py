def glitch_frame(frame, stream):
    # bail out if we have no motion vectors
    if not "mv" in frame:
        return
    mvs = frame["mv"];
    # bail out if we have no forward motion vectors
    if not "forward" in mvs:
        return
    fwd_mvs = mvs["forward"];

    # clear horizontal element of all motion vectors
    for row in fwd_mvs:
        for mv in row:
            if not mv:
                continue
            if isinstance(mv[0], int):
                # one mv per mb
                mv[0] = 0
            else:
                # more than one mv per mb
                for submv in mv:
                    submv[0] = 0
