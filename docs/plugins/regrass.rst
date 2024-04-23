regrass
=======

.. dfhack-tool::
    :summary: Regrow surface grass and cavern moss.
    :tags: adventure fort armok animals map

This command can refresh the grass (and subterranean moss) growing on your map.
Operates on floors, stairs, and ramps. Also works underneath shrubs, saplings,
and tree trunks. Ignores furrowed soil, beaches, and tiles under buildings.

Usage
-----

::

    regrass [<options>]

Regrasses the entire map by default, for compatible tiles in map blocks that
had grass at some point.

Options
-------

``-m``, ``--max``
    Maxes out every grass type in the tile, giving extra grazing time.
    Not normal DF behavior. Tile will appear to be the first type of grass
    present in the map block until that is depleted, moving on to the next type.
    When this option isn't used, non-depleted grass tiles will have their existing
    type refilled, while grass-depleted soils will have a type selected randomly.
``-n``, ``--new``
    Adds biome-compatible grass types that were not originally present in the
    map block. Allows regrass to work in blocks that never had any grass to
    begin with. Will still fail in incompatible biomes.
``-f [<grass_id>]``, ``--force [<grass_id>]``
    Force a grass type on tiles with no compatible grass types. If ``grass_id``
    is not given, then a single random grass type will be selected from raws.
    ``grass_id`` is not case-sensitive, but must be enclosed in quotes if spaces
    exist. The ``--new`` option takes precidence for compatible biomes, otherwise
    such tiles will be forced instead.
``-a``, ``--ashes``
    Regrass tiles that've been burnt to ash.
``-u``, ``--mud``
    Converts non-smoothed, mud-spattered stone into grass. Valid for layer stone,
    obsidian, and ore.
``-p [<pos> [<pos>]]``, ``--point [<pos> [<pos>]]``
    Only regrass the tile at a given coord, or the tiles in a cuboid defined by
    two coords. The keyboard cursor will be used if no coords are given.
``-b [<pos>]``, ``--block [<pos>]``
    Only regrass the map block containing a given coord or the keyboard cursor.
    `devel/block-borders` can be used to visualize map blocks.

Examples
--------

``regrass``
    Regrass the entire map, refilling existing and depleted grass except on ashes
    and muddy stone.
``regrass -p``
    Regrass the selected tile, refilling existing and depleted grass except on
    ashes and muddy stone.
``regrass --ashes --mud --point 0,0,100 19,19,119``
    Regrass tiles in the 20x20x20 cube defined by the coords, refilling existing
    and depleted grass, and converting ashes and muddy stone (if respective blocks
    ever had grass.)
``regrass -b 10,10,100 -aunm``
    Regrass the block that contains the given coord, converting ashes and muddy
    stone, adding all compatible grass types, and filling each grass type to max.
``regrass -f UNDERLICHEN``
    Regrass the entire map, refilling existing and depleted grass, else filling
    with ``underlichen``. Ignore ashes and muddy stone.
``regrass -bn -f "DOG'S TOOTH GRASS"``
    Regrass the selected block, adding all compatible grass types to block data,
    adding ``dog's tooth grass`` if no compatible types exist, and ignoring ashes
    and muddy stone. Refill existing grass, else select one of the block's types
    for each tile.

Troubleshooting
---------------

``debugfilter set Debug regrass log`` can be used to figure out why regrass
is failing on a tile. (Avoid regrassing large parts of the map with this enabled,
as it will make the game unresponsive and flood the console for several minutes!)
Disable with ``debugfilter set Info regrass log``.
