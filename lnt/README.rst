.. _my-label: lnt

.. _lnt:

=================================
LLVM LNT / Test-Suite Integration
=================================

Recommended reading materials
-----------------------------
  * https://blog.llvm.org/posts/2016-06-15-using-lnt-to-track-performance/
  * https://llvm.org/docs/lnt/quickstart.html
  * https://llvm.org/docs/TestSuiteGuide.html

Prerequisites
-------------
  * all of the normal prerequisites for building **development** version of RawSpeed.
  * python's `virtualenv <https://packages.debian.org/unstable/virtualenv>`_
  * `llvm-size`, `llvm-lit <https://llvm.org/docs/CommandGuide/lit.html>`_ (from
    `llvm <https://packages.debian.org/unstable/llvm-9>`_,
    `llvm-tools <https://packages.debian.org/unstable/llvm-9-tools>`_ packages)
  * A checkout of raw sample archive of you choice. It is suggested to use
    `https://raw.pixls.us <https://raw.pixls.us>`_ masterset.
    Please see :ref:`RSA` page for details.
  * Reference hashes for the raws in the sampleset.

    Generate them via ``$ ninja rstest-create`` from your **trusted** (!) dev build.
    Please see :ref:`integration_testing` and
    :ref:`producing_trusted_reference_hashes` pages for details.

Getting it done
---------------
.. code:: bash

  # Get external dependencies.
  git clone https://github.com/llvm/llvm-lnt.git /build/lnt
  git clone https://github.com/llvm/llvm-test-suite.git /build/test-suite
  # 'Add' RawSpeed into test-suite.
  ln -s /home/lebedevri/rawspeed/lnt /build/test-suite/RawSpeed
  # Prepare 'chroot'.
  export SANDBOX=/tmp/mysandbox
  export BUILDSANDBOX=/tmp/mybuildsandbox
  export PERFDB=/tmp/myperfdb
  virtualenv $SANDBOX
  # Setup 'chroot'.
  $SANDBOX/bin/python /build/lnt/setup.py develop
  $SANDBOX/bin/lnt create $PERFDB
  # Benchmarking time!
  $SANDBOX/bin/lnt runtest test_suite --test-suite /build/test-suite \
      --sandbox $BUILDSANDBOX \
      --cc clang --cxx clang++ \
      --cmake-define CMAKE_BUILD_TYPE=Release \
      --cmake-define TEST_SUITE_SUBDIRS="RawSpeed" \
      [--benchmarking-only] \
      [--use-perf profile] \
      --build-threads 8 --threads 1 \
      --submit $PERFDB
  # View results.
  $SANDBOX/bin/lnt runserver $PERFDB


.. seealso::

  * https://llvm.org/docs/TestSuiteGuide.html#common-configuration-options
  * ``$ $SANDBOX/bin/lnt runtest test_suite --help``
  * ``$ $SANDBOX/bin/lnt --help``
