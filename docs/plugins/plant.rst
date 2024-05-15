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
    Required. Creates a new plant of the specified type at ``pos`` or the cursor position. The target tile must be a dirt or grass floor. ``plant_id`` is not case-sensitive, but must be enclosed in quotes if spaces exist (no unmodded shrub or sapling IDs have spaces.) A numerical ID can also be used. Providing an empty string with "" will print all available IDs and skip plant creation.
``-a <value>``, ``--age <value>``
    Set the created plant to a specific age (in ticks.) ``value`` can be a non-negative integer, or the string ``tree`` to have saplings immediately grow into trees. Defaults to 0 if option is unused.

Grow
----
``-g``, ``--grow``
    Required. Grows saplings (including dead ones) into trees. Will default to all saplings on the map if no ``pos`` arguments are used. Saplings will die and fail to grow if they are blocked by another tree.
``-a <value>``, ``--age <value>``
    Define the age (in ticks) to set saplings to. ``value`` can be a non-negative integer, or the string ``tree``. Defaults to ``tree`` if option is unused. If a ``value`` larger than ``tree`` (equivalent to 120959) is used, it will make sure targeted trees have an age of at least the given value, allowing them to grow larger.
``-f <list>``, ``--filter <list>``
    Define a filter list of plant IDs to target, ignoring all other tree types. ``list`` should be a comma-separated list of strings and/or non-negative integers with no spaces in between them. Spaces are acceptable within strings as long as they are enclosed in quotes.
``-e <list>``, ``--exclude <list>``
    Same as ``--filter``, but target everything except these. Cannot be used with ``--filter``.
``-z``, ``--zlevel``
    Operate on a range of z-levels instead of default targeting. Will do all z-levels between ``pos`` arguments if both are given (instead of cuboid,) z-level of first ``pos`` if one is given (instead of single tile,) else z-level of current view if no ``pos`` is given (instead of entire map.)

Remove
------
``-r``, ``--remove``
    Required. Remove plants from the map (or area defined by ``pos`` arguments.) By default, only removes invalid plants that exist on non-plant tiles (`Bug 12868 <https://dwarffortressbugtracker.com/view.php?id=12868>`_.) The ``--shrubs`` and ``--saplings`` options allow normal plants to be targeted instead. Removal of fully-grown trees isn't supported.
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

``plant --create tower_cap``
    Create a Tower Cap sapling at the cursor.
``plant -c ""``
    List all valid shrub and sapling IDs.
``plant -c 200 -a tree``
    Create an Acacia sapling at the cursor, ready to mature into a tree.
``plant 70,70,140 -c 0``
    Create a Single-grain Wheat shrub at (70, 70, 140.)
``plant --grow``
    Attempt to grow all saplings on the map into trees.
``plant -gz -f maple,198,sand_pear``
    Attempt to grow all Maple, Oak, and Sand Pear saplings on the current z-level into trees.
``plant 0,0,100 19,19,119 -g -a 4032000``
    Set the age of all saplings and trees (with their sapling tile) in the defined 20x20x20 cube to 100 years.
``plant --remove``
    Remove all invalid plants from the map.
``plant here -rsp``
    Remove the shrub or sapling at the cursor.
``plant 0,0,49 0,0,51 -rpz -e nether_cap``
    Remove all saplings on z-levels 49 to 51, excluding Nether Cap.
