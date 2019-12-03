.. _RSA:

================================================================================
Reference Sample Archive
================================================================================

While there is some test coverage via unit tests, the major bulk of testing
is achieved via integration tests over some sample set.

.. _sampleset:

What is considered a sample set
-------------------------------

Here and onwards, a sample set is just a directory with samples, and two special
files. There should be a ``timestamp.txt`` containing an
`Unix time <Unix_time_>`_ (presumably, of when the set was last updated).
Most importantly, it **must** also contain ``filelist.sha1`` file in the
top-level directory, which is used as a digest to the contents of said sample
set. Said file **must** be a valid sha1sum_ output, with format:

::

  <40-char SHA1><space><asterisk><filename>

.. _Unix_time: https://en.wikipedia.org/wiki/Unix_time

.. _sha1sum: https://manpages.debian.org/unstable/coreutils/sha1sum.1.en.html

Canonical Sample Set
--------------------

The canonical raw sample data set is `raw.pixls.us <RPU_>`_.
It is freely licensed - all new samples are in Public Domain under
`CC0 1.0 <CC0_>`_ license (85+% of samples and counting),
however some older samples are still under more restrictive
`CC BY-NC-SA 4.0 <BYNCSA40_>`_ license.

**Please read** `this <rpu-post_>`_ **for more info on how to contribute samples!**

.. _RPU: https://raw.pixls.us/

.. _CC0: https://creativecommons.org/publicdomain/zero/1.0/

.. _BYNCSA40: http://creativecommons.org/licenses/by-nc-sa/4.0/

.. _rpu-post: https://discuss.pixls.us/t/raw-samples-wanted/5420?u=lebedevri

Full sample set
~~~~~~~~~~~~~~~

The complete set, that includes every sample available, and thus has as good
coverage as we can get, but as downside it is *quite* bulky - |rpu-button-size|
total, spanning |rpu-button-samples|.

.. |rpu-button-cameras| image:: https://raw.pixls.us/button-cameras.svg
    :target: https://raw.pixls.us/

.. |rpu-button-samples| image:: https://raw.pixls.us/button-samples.svg
    :target: https://raw.pixls.us/

It is accessible at: https://raw.pixls.us/data/

Masterset
~~~~~~~~~

But there is also a masterset, with just a handful hand-picked samples that
provide reasonable-ish coverage while spanning only ~ :math:`1/22`'th of the
disk footprint and ~ :math:`1/44`` sample count of the full set.

.. CAUTION::
   Unless you want to perform rigorous regression testing
   the masterset is strongly recommended!

.. TIP::
   Masterset **only** contains samples that are in `public domain <CC0_>`_.

It is accessible at: https://raw.pixls.us/data-unique/

.. |rpu-button-size| image:: https://raw.pixls.us/button-size.svg

.. _rpu_rsync:

Acquiring Canonical Sample Set
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pick which sample set you will want to acquire. Be wary of disk footprint!
Probably the easiest way to fetch it is via rsync_, for example:

::

   $ rsync -vvrLtW --preallocate --delete --compress --compress-level=1 --progress \
           rsync://raw.pixls.us/data-unique/ ~/raw-camera-samples/raw.pixls.us-unique/
   $ # it might be a good idea to verify consistency afterwards:
   $ sha1sum -c --strict ~/raw-camera-samples/raw.pixls.us-unique/filelist.sha1

.. _rsync: https://manpages.debian.org/unstable/rsync/rsync.1.en.html
