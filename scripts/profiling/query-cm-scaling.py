#!/usr/bin/env python3
#
# Script to parse query census and estimate scaling time

import sys
from glob import glob
from pathlib import Path
from collections import defaultdict, namedtuple
from pprint import pprint
from math import log2 as lg
import numpy as np
import matplotlib.pyplot as plt

data_dir = Path(sys.argv[1])
file_match = "*census.txt"

# NUM_PARTIES = 4

# bitwidths for operations
PERM_BW = 32
STD_BW = 64
SORT_BW = 2 * STD_BW

ELEMENT_COUNT_MODEL = False
ROUND_COUNT_MODEL = False

TableData = namedtuple('TableData', ['k', 'n', 'a'])

op_dict = defaultdict(lambda: defaultdict(list))

SORT = 'S'
AGG = 'A'
DIV = 'D'
OVERALL = 'O'

for fn in glob(str(data_dir / file_match)):
    print("Parse", fn)
    with open(fn) as c:
        sf = None
        q = None

        for line in c:
            splt = line.split()
            if "SF" in line:
                sf = float(splt[2])
                q = splt[0]
            elif "[TABLE_AGG]" in line:
                k, n, a = (int(v.split('=')[1]) for v in splt[1:])
                op_dict[sf][q].append((AGG, TableData(k, n, a)))
            elif "[TABLE_SORT]" in line:
                k, n = (int(v.split('=')[1]) for v in splt[1:])
                op_dict[sf][q].append((SORT, TableData(k, n, None)))
            elif "[PRIV_DIV]" in line:
                n = int(splt[-1].split('=')[1])
                op_dict[sf][q].append((DIV, n))
            elif "Overall" in line:
                n = float(splt[-2])
                op_dict[sf][q].append((OVERALL, n))

        print(f"  ...{len(op_dict[sf][q])} events")

del k, n, a

def sort_cost(D):
    if ELEMENT_COUNT_MODEL:
        return D.n * D.k
    
    if ROUND_COUNT_MODEL:
        return D.k * lg(D.n) * lg(SORT_BW)
    
    # quicksort
    cmp_per_sort = D.n * lg(D.n)
    total_cmp = D.k * cmp_per_sort

    total_comm = total_cmp * lg(SORT_BW)

    # perms = D.k * (D.k + 9) / 2
    # comm_perm_per_party = perms * D.n
    # # add in 1 for gen perm time
    # total_comm += comm_perm_per_party * (2 * NUM_PARTIES + 1)

    return total_comm
    
# power-of-two rounding already included in the input data. no bit padding here.
def agg_cost(D):
    if ELEMENT_COUNT_MODEL:
        return D.n * D.k
    
    if ROUND_COUNT_MODEL:
        return D.k * lg(D.n) * lg(STD_BW)
    
    round_ops = lg(STD_BW) * D.k + D.a

    total_comm = round_ops * D.n * lg(D.n)
    # return D.k * D.n * lg(D.n) * lg(STD_BW) * STD_BW
    return total_comm

def div_cost(n):
    if ELEMENT_COUNT_MODEL:
        return n * STD_BW
    
    if ROUND_COUNT_MODEL:
        return (1 + STD_BW) * SORT_BW
    
    # NOTE: WAN will use PPA
    # division is Lstd rounds
    # each round calls multiplex (nL bit ops)
    # and RCA (n/Lsort * Lsort * Lsort = nL bit ops)
    # Then we do 2 extra RCA at the end
    return (1 + STD_BW) * 2 * n

def total_cost(data):
    cs = ca = cd = 0

    if not data:
        return 0
    
    d0 = data[0]
    if d0[0] == OVERALL:
        return d0[1]
 
    for t, d in data:
        # print(t, d)
        if t == SORT:
            cs += sort_cost(d)
        elif t == AGG:
            ca += agg_cost(d)
        elif t == DIV:
            cd += div_cost(d)
    
    # print(np.round(np.array([cs, ca, cd], dtype=int)))
    return cs + ca + cd

# for q in [6, 8, 11, 12, 21, 22]:
#     qq = f"Q{q}"
    
#     # compute SF1 cost
#     sf1_cost = round(total_cost(op_dict[1][qq]))
#     sf10_cost = round(total_cost(op_dict[10][qq]))

#     print(f"{qq:>3}  {sf1_cost:8.1f} => {sf10_cost:8.1f}", end='')
#     if sf1_cost > 0:
#         print(f"  [{sf10_cost/sf1_cost:>4.2f}]")
#     else:
#         print()

sf = np.arange(1, 20 + 1)

q6_cost = np.array(list(total_cost(op_dict[s]['Q6']) for s in sf), dtype=float)
q12_cost = np.array(list(total_cost(op_dict[s]['Q12']) for s in sf), dtype=float)

print(q12_cost)

q6_cost /= q6_cost[0]
q12_cost /= q12_cost[0]

print(q6_cost)
print(q12_cost)

plt.plot(sf, q6_cost, '.-', label='Q6')
plt.plot(sf, q12_cost, '.-', label='Q12')

# lines
nlogn = sf * np.log2(10e6 * sf) / np.log2(10e6)
plt.plot(sf, sf, color='gray', linestyle='--', label='Linear Ovh')
plt.plot(sf, nlogn, color='purple', linestyle='--', label='w.c. n log n')

xlim = plt.xlim()
ylim = plt.ylim()

plt.vlines(10, *ylim, color='gray', linestyle=(0, (1, 10)))
plt.hlines(11.5, *xlim, color='gray', linestyle=(0, (1, 10)))
plt.xlim(xlim)
plt.ylim(ylim)



plt.xlabel("SF")
plt.ylabel('Ovh over SF1')
plt.legend()

plt.locator_params(axis='x', nbins=5)

plt.show()

# print(NUM_PARTIES, "PC")
print("ELEM COUNT MODEL" if ELEMENT_COUNT_MODEL else "COMPLEX MODEL")