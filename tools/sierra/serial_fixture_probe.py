#!/usr/bin/env python2.7
"""One offline synthetic JPEG probe for local xhyve serial bridge. No device I/O."""
from __future__ import print_function
import argparse
import os
import stat
import time
import urllib2

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--jpeg",required=True)
    p.add_argument("--token-file",required=True)
    p.add_argument("--url",default="http://127.0.0.1:8383/api/scratch/ingest")
    a=p.parse_args()
    if a.url!="http://127.0.0.1:8383/api/scratch/ingest":
        p.error("Only local loopback permitted")
    info=os.stat(a.token_file)
    if stat.S_IMODE(info.st_mode)&0o077:
        p.error("Private credential file must not be shared")
    with open(a.token_file,"rb") as f:
        token=f.read().strip()
    with open(a.jpeg,"rb") as f:
        jpeg=f.read()
    if not (4 <= len(jpeg) <= 262144 and jpeg[:2]=="\xff\xd8" and jpeg[-2:]=="\xff\xd9"):
        p.error("Invalid bounded fixture")
    h={
        "Content-Type":"image/jpeg",
        "Authorization":"Bearer "+token,
        "X-Fountain-Origin":"synthetic",
        "X-Fountain-PTS-Ns":"123456790",
        "X-Fountain-Width":"4",
        "X-Fountain-Height":"2",
        "X-Fountain-Stream-ID":"silver-paced-fixture"
    }
    req=urllib2.Request(a.url,jpeg,h)
    start=time.time()
    try:
        r=urllib2.urlopen(req,timeout=40)
        status=r.getcode()
        result=r.read(512)
    except urllib2.HTTPError as ex:
        status=ex.code
        result=ex.read(512)
    print("stage=serial_silver_fixture status=%d elapsed_s=%.3f bytes=%d response=%s" %
          (status,time.time()-start,len(jpeg),result))
    return 0 if status==201 else 1

if __name__=="__main__":
    raise SystemExit(main())
