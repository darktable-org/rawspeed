.. _integration_testing:

================================================================================
Integration Testing
================================================================================

.. seealso::

   :ref:`RSA`

As a first step, you *need* to acquire the sample archive you will want to use,
see e.g. :ref:`rpu_rsync`.

Due to the specifics of the the domain, just having the samples you want to use
for integration testing is not sufficient. Given *just* the samples, it is not
possible to verify anything in an automatic manner.

You can, of course, load the samples into some software that uses the
`RawSpeed <rawspeed_>`_ library, for example into darktable_, and see that they
decoded into some meaningful image, but that is indirect and tests much more
than just the library.

.. _rawspeed: https://github.com/darktable-org/rawspeed
.. _darktable: https://github.com/darktable-org/darktable

So instead, we want to document (record, called `a hash` onwards) how the
samples decode 'currently' (in a trusted, known-good hardware/software/compiler
stack/compilation options etc), store this per-image info, and then just check
against it afterwards (after modifying the library, or anything else really).

.. _producing_trusted_reference_hashes:

Producing Trusted reference Hashes
----------------------------------

Optionally, it may or may not be a good idea to first manually inspect the
samples (via e.g. darktable_), make a note which are seemingly currently decoded
correctly, and which are not.

For best results the Trusted Hashes should be produced in most mundane
environment - stable mainstream hardware (little-endian, x86; no overclocking),
stable software stack, and most importantly a trusted compiler. You also
shouldn't use ``-Oomg-optimize -fmoar-performance`` compilation flags for this.

.. WARNING::
   Trusted baseline hashes are a the very foundation for any further integration
   testing. It is always important to have good, stable foundation. It will not
   be productive if those hashes are produced incorrectly, be it either because
   the hardware is faulty (RAM/disk bit flips), or the library was miscompiled.

Other than that, generating said hashes is pretty trivial.

Specifying location of Reference Sample Archive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to make use of build system integration of integration testing,
we must first tell it where the :ref:`sample set<sampleset>` is located,
for example:

::

  $ cmake -DRAWSPEED_ENABLE_SAMPLE_BASED_TESTING:BOOL=ON \
          -DRAWSPEED_REFERENCE_SAMPLE_ARCHIVE:PATH="~/raw-camera-samples/raw.pixls.us-unique/" \
          <path to rawspeed repo checkout>

.. NOTE::

  The location of the samples must be writable if you intend to produce hashes.

Other required CMake flags
~~~~~~~~~~~~~~~~~~~~~~~~~~

We also need to build the code that is actually responsible for these
integration tests:

::

  $ cmake -DBUILD_TOOLS:BOOL=ON \
          <path to rawspeed repo checkout>

Producing hashes
~~~~~~~~~~~~~~~~

After that is done, we can finally create the hashes, and for that there is
a ``rstest-create`` build target:

::

  $ cmake --build . -- rstest-clean # get rid of any pre-existing hashes, just in case.
  [1/1] Running utility command for rstest-clean
  $ cmake --build . -- rstest-create
  <maybe actually building library's sources and rstest if you didn't build it yet>
  [0/1] Running utility command for rstest-create
  <full path to a sample>: starting decoding ...
  <full path to a sample>:  <> MB / <> ms

  Total decoding time: <>s

  All good, all hashes created!

And that's it, we've got the hashes! They were placed next to the samples in
the archive, with ``.hash`` suffix appended. Maybe you want to use some kind of
layered file system (overlayfs_ e.g.) to separate those from the actual samples,
up to you.

.. _overlayfs: https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt

Performing Integration Testing
------------------------------

.. IMPORTANT::

  Do ensure that the library is actually re-compiled with the changes you want
  to test. To err on the safe side, sometimes it is useful to remove the entire
  build directory and make a fresh build!

After you have performed the changes you wanted to - modified the library,
or changed hardware/software/compiler/compiler flags - and you want to validate
that those changes did not cause any regressions in the sample set, it is time
to actually make use of the Trusted Reference Hashes that we have created
previously.

For that, there is a ``rstest-test`` build target.
If everything is good you may see:

::

  $ cmake --build . -- rstest-test
  [0/1] Running utility command for rstest-test
  <full path to a sample>: starting decoding ...
  <full path to a sample>:  <> MB / <> ms
  Total decoding time: <>s

  All good, no tests failed!

Or, if there are issues, you may see:

::

  $ cmake --build . -- rstest-test
  [0/1] Running utility command for rstest-test
  <full path to a sample>: starting decoding ...
  <full path to a sample>:  <> MB / <> ms
  <full path to a sample> failed: hash/metadata mismatch
  Total decoding time: <>s

  WARNING: the following <> tests have failed:
  <full path to a sample> failed: hash/metadata mismatch
  See rstest.log for details.
  <...>
  ninja: build stopped: subcommand failed.

Unless the process crashed, it should have created
``<full path to a sample>.hash.failed``, and outputted the diff_ between
the existing ``<full path to a sample>.hash`` Trusted Hash and the actual result
``<full path to a sample>.hash.failed`` into ``rstest.log`` file in root of the
build dir.

.. _diff: https://manpages.debian.org/unstable/diffutils/diff.1.en.html

.. seealso::

   :ref:`lnt`
