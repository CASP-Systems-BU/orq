Operators
=========

.. include:: ../../include/core/operators/README.md
   :parser: myst_parser.sphinx_

Note: For documentation on the join functions, see :cpp:func:`orq::relational::EncodedTable::_join`.

Boolean Circuits
----------------

.. doxygenfile:: circuits.h

Shuffle
-------
.. doxygenfile:: shuffle.h

Sorting
-------
.. doxygenfile:: sorting.h
.. doxygenfile:: quicksort.h
.. doxygenfile:: radixsort.h

Merge
-----
.. doxygenfile:: merge.h

Relational
----------
.. doxygenfile:: aggregation.h
.. doxygenfile:: distinct.h
.. doxygenfile:: aggregation_selector.h

.. doxygenstruct:: orq::relational::JoinOptions
.. doxygenstruct:: orq::relational::AggregationOptions

Streaming
---------
.. doxygenfile:: streaming.h

Common
------
.. doxygenfile:: common.h