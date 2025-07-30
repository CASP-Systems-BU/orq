# Shuffle Pseudocode

```
shuffle(vector x):
	// generate a random permutation perm=perm_3(perm_2(perm_1(x)))
	// each party P_i holds perm_i and perm_{i+1}
	perm = generate random permutation over [len(x)]

	set y_0 = x
	for i=1 to 3:
		parties i and i-1 compute y_i = perm_i(y_{i-1})
		reshare y_i to party i+1

	return y_3
```


We need three new primitives:
- $\texttt{gen-perm(n)}$ - generate a random permutation of $n$ elements
	- requires $O(n \log n)$ random bits
	- can run in either $O(n)$ time (hard to parallelize) or $O(n \log n)$ time (very easy to parallelize)
- $\texttt{apply-perm(x, }\pi_i\texttt{)}$ - given an input vector $\vec{x}$ and a permutation $\pi_i$, apply the permutation to the input vector
	- entirely offline
- $\texttt{reshare(group, x)}$ - given an input vector $\vec{x}$ and a group $\texttt{group}$, the parties in the group rerandomize and distribute their shares to the remaining parties
	- this can be implemented once for all protocols

Analysis of shuffle protocol:
- $m$ rounds of communication for $m$ groups
- $O(n)$ local computation for generating and applying the permutation at each step
	- we will eventually use the $O(n \log n)$ permutation generation algorithm that is easier to parallelize
- $O(n \log n)$ random bits

Finally, here is a template for what the code should look like within the `shuffle.h` file.

```
void shuffle() {
	// protocol.getGroups()
	// for group in groups
		// if this party participates in the group
			// generate common randomness
			// gen_perm(size, randomness)
		// else continue
	// for group in groups
		// if in group
			// apply_perm(permutation)
		// reshare(group) - all parties participate, some send, some receive
}
```
