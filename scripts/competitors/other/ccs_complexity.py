import sys

l_sigma = 32
l = int(sys.argv[1])

# calculate theirs
theirs = (20 * (l - 1) * l_sigma) + (4 * l_sigma * l_sigma) + (6 * (l - 1))
theirs_rounds = 18 * l - 14

# calculate ours
ours = (13 * (l + 1) * l_sigma) + (6 * l * l) + (4 * l_sigma * l_sigma) - (6 * l)
ours_rounds = 11 * l + 7

print(f"Theirs: {theirs}  ({theirs_rounds})")
print(f"Ours  :   {ours}  ({ours_rounds})")