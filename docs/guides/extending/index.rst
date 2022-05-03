.. _guides_extending:

Extending MicroPython
=====================

This chapter describes options for implementing additional functionality in C, but from code
written outside of the main MicroPython repository. The first approach is useful for building
your own custom firmware with some project-specific additional modules or functions that can
be accessed from Python. The second approach is for building modules that can be loaded at runtime.

Please see the :ref:`library section <guides_extending_library>` for more
information on building core modules that live in the main MicroPython
repository.

.. toctree::
    :maxdepth: 1

    cmodules
    library
    natmod
    publiccapi
