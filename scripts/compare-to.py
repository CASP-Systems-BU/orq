#!/usr/bin/env python3
#
# Compare two branches. Assume cmake is already run.

import argparse
import subprocess
import sys

parser = argparse.ArgumentParser(description="Compare a test over two branches")
parser.add_argument('--branch', '-b', type=str, required=True, help="name of the other branch or git ref")
parser.add_argument('--exec', '-x', type=str, required=True, help="program to run")
parser.add_argument('-N', type=int, help="protocol", default=3)
parser.add_argument('--threads', '-t', type=int, help="number of threads", default=1)

args = parser.parse_args()

exec = args.exec
other_branch = args.branch
this_branch = subprocess.getoutput("git rev-parse --abbrev-ref HEAD")

print(f"=== Run {args.exec} on this branch ({this_branch})")
subprocess.run(f"make {exec}".split())
subprocess.run(f"mpirun -n {args.N} {exec} {args.threads}".split())

ret = subprocess.run(f"git checkout {other_branch}".split())
if ret.returncode:
    sys.exit(1)

print(f"=== Run {args.exec} on other branch ({other_branch})")
subprocess.run(f"make {exec}".split())
subprocess.run(f"mpirun -n {args.N} {exec} {args.threads}".split())

print(f"=== Switching back to original branch")
subprocess.run(f"git checkout -".split())