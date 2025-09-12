Random
======

.. include:: ../../include/core/random/README.md
   :parser: myst_parser.sphinx_

Permutations
------------
.. doxygenfile:: permutation_manager.h

.. doxygenclass:: orq::random::ShardedPermutation
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

.. doxygenclass:: orq::random::HMShardedPermutation
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

.. doxygenclass:: orq::random::DMShardedPermutation
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

.. doxygenclass:: orq::random::DMShardedPermutationGenerator
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

.. doxygenclass:: orq::random::DMDummyGenerator
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

.. doxygenclass:: orq::random::DMPermutationCorrelationGenerator
   :members:
   :protected-members:
   :undoc-members:
   :private-members:

Randomness Generators
---------------------

.. doxygenfile:: random_generator.h
.. doxygenfile:: prg_algorithm.h
.. doxygenfile:: common_prg.h
.. doxygenfile:: seeded_prg.h
.. doxygenfile:: zero_rg.h

Correlation Generators
-----------------------
.. doxygenfile:: correlation_generator.h
.. doxygenfile:: ole_generator.h
.. doxygenfile:: dummy_ole.h
.. doxygenfile:: zero_ole.h
.. doxygenfile:: silent_ot.h
.. doxygenfile:: gilboa_ole.h
.. doxygenfile:: beaver_triple_generator.h
.. doxygenfile:: dummy_auth_triple_generator.h
.. doxygenfile:: dummy_auth_random_generator.h
.. doxygenfile:: zero_sharing_generator.h
.. doxygenfile:: oprf.h

Utilities
---------
.. doxygenfile:: pooled_generator.h
.. doxygenfile:: manager.h



