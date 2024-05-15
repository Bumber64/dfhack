plant
=====

.. dfhack-tool::
    :summary: Grow and remove shrubs or trees.
    :tags: adventure fort armok map plants

Grow and remove shrubs or trees. Primary options are ``--create``, ``--grow``, and ``--remove``. ``--create`` allows the creation of new shrubs and saplings. ``--grow`` adjusts the age of saplings and trees, allowing them to grow instantly. ``--remove`` can remove existing shrubs and saplings.

Usage
-----

::

    plant [<pos> [<pos>]] [<options>]

The ``pos`` argument can limit operation of ``--grow`` or ``--remove`` to a single tile or a cuboid. ``pos`` should normally be in the form ``0,0,0``, without spaces. The string ``here`` can be used in place of numeric coordinates to use the position of the keyboard cursor, if active. ``--grow`` and ``--remove`` will operate on the entire map if no ``pos`` is provided. ``--create`` always operates on a single tile and cannot accept a second ``pos``. If no ``pos`` is provided to ``--create``, the keyboard cursor will be used by default.

Create
------
``-c <plant_id>``, ``--create <plant_id>``
    Creates a new plant of the specified type at ``pos`` or the cursor position. The target tile must be a dirt or grass floor. ``plant_id`` is not case-sensitive, but must be enclosed in quotes if spaces exist. A numerical ID can also be used. Providing an empty string with "" will print all available IDs and skip plant creation.
``-a <value>``, ``--age <value>``
    Set the created plant to a specific age (in ticks.) ``value`` can be a non-negative integer, or the string ``tree`` to have saplings immediately grow into trees. Defaults to 0 if option is unused.

Grow
----
``-g``, ``--grow``
    Grows saplings (including dead ones) into trees. Will default to all saplings on the map if no ``pos`` arguments are used. Saplings will fail to grow and instead die if they are blocked by another tree.
``-a <value>``, ``--age <value>``
    Define the age (in ticks) to set saplings to. ``value`` can be a non-negative integer, or the string ``tree``. Defaults to ``tree`` if option is unused. If a ``value`` larger than ``tree`` (equivalent to 120959) is used, it will make sure selected trees have an age of at least the given value, allowing them to grow larger.
``-f <list>``, ``--filter <list>``
    Define a filter list of plant IDs to target, ignoring all other tree types. ``list`` should be a comma-separated list of strings and/or non-negative integers with no spaces in between them. Spaces are acceptable within strings as long as they are enclosed in quotes.
``-e <list>``, ``--exclude <list>``
    Same as ``--filter``, but target everything except these. Cannot be used with ``--filter``.
``-z``, ``--zlevel``
    Operate on a range of z-levels instead of default targeting. Will do all z-levels between ``pos`` arguments if both are given (instead of cuboid,) z-level of first ``pos`` if one is given (instead of single tile,) else z-level of current view if no ``pos`` is given (instead of entire map.)

Remove
------
``-r``, ``--remove``
    Remove plants from the map (or area defined by ``pos`` arguments.) By default, only removes invalid plants that exist on non-plant tiles (`Bug 12868 <https://dwarffortressbugtracker.com/view.php?id=12868>`_.) The ``--shrubs`` and ``--saplings`` options allow normal plants to be targeted instead. Removal of fully-grown trees isn't supported.
``-s``, ``--shrubs``
    Target shrubs for removal.
``-p``, ``--saplings``
    Target saplings for removal.
``-d``, ``--dryrun``
    Don't actually remove plants. Just print the total number of plants that would be removed.
``-f <list>``, ``--filter <list>``
    Define a filter list of plant IDs to target, ignoring all other plant types. This applies after ``--shrubs`` and ``--saplings`` are targeted, and has no effect if neither are used. ``list`` should be a comma-separated list of strings and/or non-negative integers with no spaces in between them. Spaces are acceptable within strings as long as they are enclosed in quotes.
``-e <list>``, ``--exclude <list>``
    Same as ``--filter``, but target everything except these. Cannot be used with ``--filter``.
``-z``, ``--zlevel``
    Operate on a range of z-levels instead of default targeting. Will do all z-levels between ``pos`` arguments if both are given (instead of cuboid,) z-level of first ``pos`` if one is given (instead of single tile,) else z-level of current view if no ``pos`` is given (instead of entire map.)

Examples
--------

``plant create TOWER_CAP``
    Create a Tower Cap sapling at the cursor position.
