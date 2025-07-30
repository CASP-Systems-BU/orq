#!/usr/bin/env python3
# Script to double-check accuracy of bandwidth estimates

import os
import subprocess
from pathlib import Path
import sys
import tempfile
import time

Q = sys.argv[1]

tmp = Path(tempfile.mkdtemp())

os.mkdir(tmp / "comm")


def exec(cmd):
    subprocess.check_call(cmd, shell=True, text=True, bufsize=1)


def get_shell(s):
    s = subprocess.check_output(s, shell=True, stderr=subprocess.STDOUT, text=True)
    return s


os.chdir(Path(get_shell("git rev-parse --show-toplevel").strip()) / "build")

size = 0.1 if Q.startswith("q") else (1 << 20)

exec('cmake .. -DPROTOCOL=1 -DEXTRA="-DQUERY_PROFILE;-DSINGLE_EXECUTION=1"')
exec(f"make -j {Q}")
out = tmp / (Q + "-1pc.txt")
exec(f"stdbuf --output=L ./{Q} 1 1 -1 {size} | tee {out}")

exec('cmake .. -DPROTOCOL=0 -DEXTRA="-DQUERY_PROFILE;-DSINGLE_EXECUTION=1"')
exec(f"make -j {Q}")
out = tmp / (Q + "-0pc.txt")
exec(f"stdbuf --output=L ./{Q} 1 1 -1 {size} | tee {out}")


def run_mpc(n):
    Path.unlink(Path(Q), missing_ok=True)

    exec(
        f'cmake .. -DPROTOCOL={n} -DEXTRA="-DQUERY_PROFILE;-DPRINT_COMMUNICATOR_STATISTICS"'
    )
    # Sometimes the wrong executable is built. Race condition? Not sure if this
    # is actually helping.
    time.sleep(1)
    exec(f"make -j {Q}")
    d = tmp / "comm" / f"{n}.txt"
    exec(f"stdbuf --output=L mpirun -n {n} ./{Q} 1 1 -1 {size} | tee {d}")
    bw = sum(
        map(
            lambda f: int(f.split()[-1]), get_shell(f'grep "P. Total" {d}').splitlines()
        )
    )

    return bw


bw = {}

for i in [2, 3, 4]:
    bw[i] = run_mpc(i)

for n, bw in bw.items():
    print(f"{n}PC: {bw / 1e6:.2f} MB")

exec(f"../scripts/osdi25/bandwidth-estimate.py {tmp}")
