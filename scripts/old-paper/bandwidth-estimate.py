#!/usr/bin/env python3

from humanize import naturalsize
import sys
from pathlib import Path
from rich import print
from rich.table import Table
import natsort

# See issue #572
# These are the communication costs, in bits, per bit of input, across all
# parties (i.e., do not multiply by N)
COSTS = {
    2: {
        "and_b": 4,
        "multiply_a": 4,
        "redistribute_shares_b": 2,
        "secret_share_a": 1,
        "secret_share_b": 1,
        "open_shares_a": 2,
        "open_shares_b": 2,
        "b2a_bit": 4,
        # permute & share (+ inverse)
        "applyperm": 2,
    },
    3: {
        "and_b": 3,
        "multiply_a": 3,
        "redistribute_shares_b": 8,
        "secret_share_a": 4,
        "secret_share_b": 4,
        "open_shares_a": 3,
        "open_shares_b": 3,
        "b2a_bit": 9,
        # cost is 3x reshare, which has cost 2
        "applyperm": 6,
    },
    4: {
        "and_b": 12,
        "multiply_a": 12,
        "redistribute_shares_b": 4,
        "secret_share_a": 9,
        "secret_share_b": 9,
        "open_shares_a": 4,
        "open_shares_b": 4,
        "b2a_bit": 16,
        # cost is 4x reshare, which has cost 6
        "applyperm": 24,
    },
    # # Dalskov 4PC
    # 4: {
    #     "and_b": 6,
    #     "multiply_a": 6,
    #     "redistribute_shares_b": 2,
    #     "secret_share_a": 9,
    #     "secret_share_b": 9,
    #     "open_shares_a": 4,
    #     "open_shares_b": 4,
    #     "b2a_bit": 8,
    #     "applyperm": 12,
    # },
}


# return bandwidth in bytes
def compute_costs(data, proto):
    reached_counts = False
    bandwidth = 0

    for line in data:
        if not (reached_counts or line.startswith("Op Counts")):
            continue

        line = line.strip()
        reached_counts = True

        if " | " not in line:
            continue

        counts = line.split("|")
        if len(counts) < 3:
            continue

        op = line.split()[0]
        input_bits = int(counts[2])

        comm_bits = COSTS[proto].get(op, 0) * input_bits

        # if op in ('applyperm', 'reshare'):
        #     sort_bw += comm_bits

        bandwidth += comm_bits

    bandwidth /= 8

    # return bytes
    # return f"{bw_str} ({per_str} per)"
    return bandwidth


dir = Path(sys.argv[1])

table = Table()

table.add_column("Query", justify="right")
table.add_column("2PC BW", justify="right")
table.add_column("3PC BW", justify="right")
table.add_column("4PC BW", justify="right")

all_bw = []


def parse_file(p):
    with open(p) as f:
        data = f.readlines()

        bw2 = compute_costs(data, 2)
        bw3 = compute_costs(data, 3)
        bw4 = compute_costs(data, 4)

        all_bw.append((p.stem, bw2, bw3, bw4))


# if you provide a file or files, read those in
if dir.is_file():
    for p in natsort.natsorted(sys.argv[1:]):
        parse_file(Path(p))
else:
    # or if you provide a directory, read all txt in the directory
    print("Reading directory", dir)
    for p in natsort.natsorted(Path.glob(dir, "*.txt")):
        parse_file(p)

# all_bw.sort(key=lambda x: x[1])

print("Bandwidth in MB")

for b in all_bw:
    n = [f"{x/1e6:.2f}" for x in b[1:]]
    # n = [naturalsize(x) for x in b[1:]]
    table.add_row(f"{b[0]}", *n)

print(table)
