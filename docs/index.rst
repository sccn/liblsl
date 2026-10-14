liblsl documentation
====================

liblsl is the C/C++ library at the core of the
`Lab Streaming Layer <https://labstreaminglayer.readthedocs.io>`__ (LSL).
Every LSL application and language binding uses it to publish streams (outlets),
find them on the network (resolvers) and receive them (inlets) with synchronized time stamps.

This site documents liblsl itself: how to get, build and link it, how it works on the
network, and its C/C++ API. For LSL in general (what it is, supported devices, apps,
other languages and recording) see the :doc:`LSL documentation <lsl:index>`.

.. toctree::
   :maxdepth: 2
   :caption: Using liblsl

   guide/getting
   guide/linking
   guide/building
   guide/configuration

.. toctree::
   :maxdepth: 2
   :caption: How liblsl works

   internals/overview
   internals/discovery
   internals/streamfeed
   internals/time_sync
   internals/recovery

.. toctree::
   :maxdepth: 1
   :caption: API reference
   :glob:

   ref/*

.. toctree::
   :maxdepth: 1
   :caption: Maintainers

   guide/maintaining


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
