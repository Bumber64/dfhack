plant
=====

.. dfhack-tool::
    :summary: Grow and remove shrubs or trees.
    :tags: adventure fort armok map plants

Grow and remove shrubs or trees. Sub-commands are ``grow``, ``create``, and
``remove``. ``grow`` adjusts the age of saplings and trees, allowing them
to grow up instantly. ``create`` allows the creation of new shrubs and
saplings. ``remove`` allows for the removal of existing shrubs and saplings.

Usage
-----

::

    plant [<pos> [<pos>]] [<options>]

Sample text.

``plant create <ID>``
    Creates a new plant of the specified type at the active cursor position.
    The cursor must be on a dirt or grass floor tile.
``plant grow``
    Grows saplings into trees. If the cursor is active, it only affects the
    sapling under the cursor. If no cursor is active, it affect all saplings
    on the map.

Examples
--------

``plant create TOWER_CAP``
    Create a Tower Cap sapling at the cursor position.
