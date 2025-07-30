# Vector Lookup Performance Improvements

## Background
In profiling various Secrecy operations, we observed that a significant proportion of time was spent in doing Vector lookups and in particular in `VectorData::operator[]`. We hypothesized that this was due to `VectorData::operator[]` being a virtual method, with the overhead from vtable lookups adding up over billions of calls. The reason for having a virtual method was to enable support for different access patterns into vectors, where users can call an `<access-pattern>_subset_reference` method on a Vector and recieve a new Vector that points into a subset of the original Vector. Notably, the new Vector keeps a reference to, not a copy of, the original Vector's data, so modifying one modifies the other. Various Secrecy operators rely on this reference behavior. In order to create this behavior, Vectors stored an instance of a subclass of VectorDataBase which would override the `operator[]` virtual method to provide the relevant access pattern by converting the requested index into the subset reference Vector into the corresponding index in the original Vector. The `Vector::operator[]` method was then just a wrapper around the relevant `VectorDataBase::operator[]` override.

## Optimization
In order to avoid the use of virtual methods, we modified the Vector class to instead hold a reference to the underlying data and a table mapping indices of that Vector to indices of the underlying data. The `<access-pattern>_subset_reference` methods now generate a new Vector with the same data reference and a new index mapping table. The `Vector::operator[]` method now just looks up the given index in the mapping and then uses the resulting index in the underlying data.

## Improvements
This frontloads the work necessary to calcuate the relevant index mapping. This could be wasteful if we wanted to only access a few elements of a Vector, but in Secrecy this is almost never the case. It also allows us to optimize the generation of these mappings, which in some cases (Alternating and ReversedAlternating access patterns) results in a significant improvement by avoiding the need to repeatedly calcuate multiple hardware divisions per element accessed. Most importantly, it avoids the need for a vtable lookup on each Vector access. Between these two improvements, we see a significant performance gain, as shown in the table below. This change should also make it relatively easy to implement proper iterators over Vectors, which currently exist but do not function properly with access patterns, though because we have already optimized for the fact that Vectors are generally accessed sequentially and all at once, the performance gains from implementing iterators are expected to be small.

### Vector 1048576 x 31b, avg over 100 repetitions for primitives
| Operation                      | Before optimization (a075a7dc) | After optimization (0443e681) |
|--------------------------------|--------------------------------|-------------------------------|
| `EQUAL c = a == b`             | 0.256535                       | 0.189437                      |
| `EQUAL_NO_ASSIGN a == b`       | 0.253207                       | 0.184193                      |
| `AND_NO_ASSIGN a & b`          | 0.0305147                      | 0.0179047                     |
| `AND_EQ a &= b`                | 0.028917                       | 0.0140446                     |
| `GREATER c = a > b`            | 0.413234                       | 0.369749                      |
| `LEQ c = a <= b`               | 0.42625                        | 0.380602                      |
| `GREATER_NO_ASSIGN a > b`      | 0.40578                        | 0.366658                      |
| `RIPPLE_CARRY_ADDER c = a + b` | 0.366777                       | 0.269377                      |
| `RCA_NO_ASSIGN a + b`          | 0.375948                       | 0.26241                       |
| `RCA_SUB c = a - b`            | 0.423522                       | 0.283928                      |
| `DIV c = a / b (10k elements)` | 0.36646                        | 0.271265                      |
| `BITONIC_SORT`                 | 107.438                        | 67.409                        |

## Drawbacks
The primary drawback here is memory usage, with the additional data needed to store the index mappings resulting in an approximately 2-2.1x increase in memory usage. We expect at least a 2x increase, with the additional usage beyond that coming from the fact that subset references now also take a non-negligible amount of memory. At the moment this increase doesn't appear to be a problem, but if it becomes problematic, we could decrease memory usage by avoiding storing index mappings for Vectors with the trivial identity mapping, at the cost of a (hopefully small) performance hit for having to distinguish between Vectors with and without non-trivial access patterns applied.`
