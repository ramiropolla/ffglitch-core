#!/usr/bin/env python

# run script on frames from input file:
#  ffglitch -i <file> -f <features> -s <script> -o <file>

import argparse
import json
import os
import subprocess
import sys
import tempfile
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('-i', metavar="<file>",    dest='input',   required=True, help='input media file')
parser.add_argument('-f', metavar="<feature>", dest='feature', required=True, help='select feature to glitch')
parser.add_argument('-s', metavar="<file>",    dest='script',  required=True, help='script to glitch frames')
parser.add_argument('-o', metavar="<file>",    dest='output',  required=True, help='output media file')
parser.add_argument('-v', action="store_true", dest='verbose', required=False, help='verbose output')
parser.add_argument('-k', action="store_true", dest='keep',    required=False, help='do not delete temporary JSON file')
args = parser.parse_args()

# Handle verbosity
if args.verbose:
    stderr = sys.stdout
else:
    stderr = open(os.devnull, 'w')

# Generate input json file name
json_in = os.path.splitext(args.input)[0] + ".json"

# Check that input file name does not end in .json
if json_in == args.input:
    raise ValueError('Input file name must not end in .json')

# Function to get sha1sum of file
def calc_sha1sum(filename):
    h  = hashlib.sha1()
    b  = bytearray(128*1024)
    mv = memoryview(b)
    with open(filename, 'rb', buffering=0) as f:
        for n in iter(lambda : f.readinto(mv), 0):
            h.update(mv[:n])
    return h.hexdigest()

# Try to read input json file
json_root = None
try:
    with open(json_in, 'r') as f:
        print("[+] checking existing JSON file '%s'" % json_in)
        json_root = json.load(f)
        # check features
        features = json_root["features"]
        if len(features) != 1 or features[0] != args.feature:
            json_root = None
            print("[-] feature '%s' not found in '%s'" % (args.feature, json_in))
        # check sha1sum
        sha1sum = json_root["sha1sum"]
        if len(sha1sum) != 40 or sha1sum != calc_sha1sum(args.input):
            json_root = None
            print("[-] sha1sum mismatch for '%s' in '%s'" % (args.input, json_in))
        if json_root is not None:
            print("[+] OK")
except IOError:
    pass

ffedit_path = None
def run_ffedit(cmd):
    global ffedit_path
    if ffedit_path == None:
        dir_path = os.path.dirname(os.path.realpath(__file__))
        ffedit_path = [os.path.join(dir_path, "ffedit")]
    cmd = ffedit_path + cmd
    if args.verbose:
        print("[v] $ %s" % ' '.join(cmd))
    return subprocess.check_output(cmd, stderr=stderr)

# First pass (export data)
if json_root is None:
    print("[+] exporting feature '%s' to '%s'" % (args.feature, json_in))
    run_ffedit([args.input, "-f", args.feature, "-e", json_in])
    with open(json_in, 'r') as f:
        json_root = json.load(f)

# Run script on JSON data.
print("[+] running script '%s' on JSON data" % args.script)
execfile(args.script)
json_stream = None
json_frame = None
# for each stream (normally there is only one stream of interest)
for stream in json_root["streams"]:
    json_stream = stream
    # for each frame in the stream
    for frame in stream["frames"]:
        json_frame = frame
        # if ffedit exported motion vectors
        if args.feature in frame:
            glitch_frame(frame[args.feature])

# Dump modified JSON data to temporary file.
json_fd, json_out = tempfile.mkstemp(prefix='ffglitch_', suffix='.json')
json_fp = os.fdopen(json_fd, 'w')
print("[+] dumping modified data to '%s'" % json_out)
json_fp.write(json.dumps(json_root))
json_fp.close()

# Second pass (apply data).
print("[+] applying modified data to '%s'" % args.output)
ok = False
try:
    run_ffedit([args.input, "-f", args.feature, "-a", json_out, args.output])
    ok = True
except subprocess.CalledProcessError as grepexc:
    vstr = " (rerun FFglitch with '-v')" if not args.verbose else ""
    print("[-] something went wrong%s" % vstr)

# Remove temporary JSON file.
if ok and not args.keep:
    os.unlink(json_out)
else:
    print("[+] not deleting temporary file '%s'" % json_out)
