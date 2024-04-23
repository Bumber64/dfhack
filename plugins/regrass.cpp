#include "Debug.h"
#include "PluginManager.h"
#include "TileTypes.h"

#include "modules/Gui.h"
#include "modules/Maps.h"

#include "df/biome_type.h"
#include "df/block_square_event_grassst.h"
#include "df/block_square_event_material_spatterst.h"
#include "df/builtin_mats.h"
#include "df/map_block.h"
#include "df/map_block_column.h"
#include "df/plant.h"
#include "df/plant_raw.h"
#include "df/region_map_entry.h"
#include "df/world.h"

using std::string;
using std::vector;
using namespace DFHack;

DFHACK_PLUGIN("regrass");
REQUIRE_GLOBAL(world);

namespace DFHack
{
    DBG_DECLARE(regrass, log, DebugCategory::LINFO);
}

command_result df_regrass(color_ostream &out, vector<string> &parameters);

struct regrass_options
{
    bool max_grass = false;
    bool new_grass = false;
    bool force = false;
    bool ashes = false;
    bool mud = false;
    bool point = false;
    bool block = false;
    bool zlevel = false;
    int32_t forced_plant = -1;
};

static bool valid_tile(df::map_block *block, int x, int y, regrass_options options)
{   // Is valid tile for regrass
    auto des = block->designation[x][y];
    auto tt = block->tiletype[x][y];
    auto shape = tileShape(tt);
    auto mat = tileMaterial(tt);
    auto spec = tileSpecial(tt);

    if (mat == tiletype_material::GRASS_LIGHT ||
        mat == tiletype_material::GRASS_DARK ||
        mat == tiletype_material::PLANT) // Shrubs and saplings can have grass underneath
    {   // Refill existing grass
        DEBUG(log).print("Valid tile: Grass|Shrub|Sapling\n");
        return true;
    }
    else if (tt == tiletype::TreeTrunkPillar)
    {   // Trees can have grass under main tile
        auto p = df::coord(block->map_pos.x + x, block->map_pos.y + y, block->map_pos.z);
        auto mbc = world->map.column_index[(block->map_pos.x / 48)*3][(block->map_pos.y / 48)*3];
        if (!mbc)
        {
            Core::printerr("No MLT block column for tile at (%d, %d, %d)!\n", p.x, p.y, p.z);
            return false;
        }

        for (auto plant : mbc->plants)
        {
            if (plant->pos == p)
            {
                DEBUG(log).print("Valid tile: Tree\n");
                return true; // Is main tile
            }
        }

        DEBUG(log).print("Invalid tile: Tree\n");
        return false; // Not main tile
    }
    else if (des.bits.flow_size > (des.bits.liquid_type == tile_liquid::Magma ? 0 : 3))
    {   // Under water/magma (df::plant_raw::shrub_drown_level is usually 4)
        DEBUG(log).print("Invalid tile: Liquid\n");
        return false;
    }
    else if (shape != tiletype_shape::FLOOR &&
        shape != tiletype_shape::RAMP &&
        shape != tiletype_shape::STAIR_UP &&
        shape != tiletype_shape::STAIR_DOWN &&
        shape != tiletype_shape::STAIR_UPDOWN)
    {
        DEBUG(log).print("Invalid tile: Shape\n");
        return false;
    }
    else if (block->occupancy[x][y].bits.building != tile_building_occ::None)
    {
        DEBUG(log).print("Invalid tile: Building\n");
        return false;
    }
    else if (!options.force && block->occupancy[x][y].bits.no_grow)
    {
        DEBUG(log).print("Invalid tile: no_grow\n");
        return false;
    }
    else if (mat == tiletype_material::SOIL)
    {
        if (spec == tiletype_special::FURROWED || spec == tiletype_special::WET)
        {   // Dirt road or beach
            DEBUG(log).print("Invalid tile: Furrowed|Wet\n");
            return false;
        }

        DEBUG(log).print("Valid tile: Soil\n");
        return true;
    }
    else if (options.ashes && mat == tiletype_material::ASHES)
    {
        DEBUG(log).print("Valid tile: Ashes\n");
        return true;
    }
    else if (options.mud)
    {
        if (spec == tiletype_special::SMOOTH || spec == tiletype_special::TRACK)
        {   // Don't replace smoothed stone
            DEBUG(log).print("Invalid tile: Smooth (mud check)\n");
            return false;
        }
        else if (mat != tiletype_material::STONE &&
            mat != tiletype_material::LAVA_STONE &&
            mat != tiletype_material::MINERAL)
        {   // Not non-feature stone
            DEBUG(log).print("Invalid tile: Wrong mat (mud check)\n");
            return false;
        }

        for (auto blev : block->block_events)
        {
            if (blev->getType() != block_square_event_type::material_spatter)
                continue;

            auto &ms_ev = *(df::block_square_event_material_spatterst *)blev;
            if (ms_ev.mat_type == builtin_mats::MUD)
            {
                if (ms_ev.amount[x][y] > 0)
                {
                    DEBUG(log).print("Valid tile: Muddy stone\n");
                    return true;
                }
                else
                {
                    DEBUG(log).print("Invalid tile: Non-muddy stone\n");
                    return false;
                }
            }
        }
    }

    DEBUG(log).print("Invalid tile: No success\n");
    return false;
}

static vector<int32_t> grasses_for_tile(df::map_block *block, int x, int y)
{   // Return sorted vector of valid grass ids
    vector<int32_t> grasses;

    if (block->occupancy[x][y].bits.no_grow)
    {
        DEBUG(log).print("Skipping grass collection: no_grow.\n");
        return grasses;
    }

    DEBUG(log).print("Collecting grasses...\n");
    if (block->designation[x][y].bits.subterranean)
    {
        for (auto p_raw : world->raws.plants.grasses)
        {   // Sorted by df::plant_raw::index
            if (p_raw->flags.is_set(plant_raw_flags::BIOME_SUBTERRANEAN_WATER))
            {
                DEBUG(log).print("Cave grass: %s\n", p_raw->id.c_str());
                grasses.push_back(p_raw->index);
            }
        }
    }
    else // Above ground
    {
        auto rgn_pos = Maps::getBlockTileBiomeRgn(block, df::coord2d(x, y)); // x&15 is okay

        if (!rgn_pos.isValid())
        {   // No biome (happens in sky)
            DEBUG(log).print("No grass: No biome region!\n");
            return grasses;
        }

        auto &biome_info = *Maps::getRegionBiome(rgn_pos);
        auto plant_biome_flag = ENUM_ATTR(biome_type, plant_raw_flag, Maps::getBiomeType(rgn_pos.x, rgn_pos.y));

        bool good = (biome_info.evilness < 33);
        bool evil = (biome_info.evilness > 65);
        bool savage = (biome_info.savagery > 65);

        for (auto p_raw : world->raws.plants.grasses)
        {   // Sorted by df::plant_raw::index
            auto &flags = p_raw->flags;
            if (flags.is_set(plant_biome_flag) &&
                (good || !flags.is_set(plant_raw_flags::GOOD)) &&
                (evil || !flags.is_set(plant_raw_flags::EVIL)) &&
                (savage || !flags.is_set(plant_raw_flags::SAVAGE)))
            {
                DEBUG(log).print("Surface grass: %s\n", p_raw->id.c_str());
                grasses.push_back(p_raw->index);
            }
        }
    }

    DEBUG(log).print("Done collecting grasses.\n");
    return grasses;
}

static bool regrass_events(df::map_block *block, int x, int y, regrass_options options)
{   // Modify grass block events
    if (!valid_tile(block, x, y, options))
        return false;

    bool success = false;
    for (auto blev : block->block_events)
    {   // Try to refill existing events
        if (blev->getType() != block_square_event_type::grass)
            continue;

        auto &gr_ev = *(df::block_square_event_grassst *)blev;

        if (options.max_grass)
        {   // Refill all
            gr_ev.amount[x][y] = 100;
            success = true;
        }
        else if (gr_ev.amount[x][y] > 0)
        {   // Refill first non-zero grass
            gr_ev.amount[x][y] = 100;
            DEBUG(log).print("Refilled existing grass.\n");
            return true;
        }
    }

    auto valid_grasses = grasses_for_tile(block, x, y);
    if (options.force && valid_grasses.empty())
    {
        DEBUG(log).print("Forcing grass.\n");
        grasses.push_back(options.forced_plant);
        block->occupancy[x][y].bits.no_grow = false;
    }
    
    if (options.force || (options.new_grass && !valid_grasses.empty()))
    {
        DEBUG(log).print("Adding missing grasses...\n");
        auto new_grasses(valid_grasses); // Copy vector
        for (auto blev : block->block_events)
        {   // Find which grasses we're missing
            if (blev->getType() == block_square_event_type::grass)
                erase_from_vector(new_grasses, ((df::block_square_event_grassst *)blev)->plant_index);
        }

        for (auto id : new_grasses)
        {   // Add new grass events
            DEBUG(log).print("Adding grass with plant index %d\n", id);
            auto gr_ev = df::allocate<df::block_square_event_grassst>();
            block->block_events.push_back(gr_ev);
            gr_ev->plant_index = id;

            if (options.max_grass)
            {   // Initialize tile as full
                gr_ev->amount[x][y] = 100;
                success = true;
            }
        }
        DEBUG(log).print("Done adding grasses.\n");
    }

    if (options.max_grass)
    {
        DEBUG(log).print("Tile grasses maxed.\n");
        return success;
    }

    vector<df::block_square_event_grassst *> temp;
    for (auto blev : block->block_events)
    {   // Gather all valid grass events
        if (blev->getType() != block_square_event_type::grass)
            continue;

        auto gr_ev = (df::block_square_event_grassst *)blev;
        if (vector_contains(valid_grasses, gr_ev->plant_index))
            temp.push_back(gr_ev);
    }

    auto gr_ev = vector_get_random(temp);
    if (gr_ev)
    {
        gr_ev->amount[x][y] = 100;
        DEBUG(log).print("Random regrass plant index %d\n", gr_ev->plant_index);
        return true;
    }

    DEBUG(log).print("Tile doesn't support grass! new_grass = %s\n", options.new_grass ? "true" : "false");
    return false;
}

int regrass_tile(df::map_block *block, int x, int y, regrass_options options)
{   // Regrass single tile. Return 1 if tile success, else 0
    DEBUG(log).print("Regrass tile (%d, %d, %d)\n", block->map_pos.x + x, block->map_pos.y + y, block->map_pos.z);
    if (!regrass_events(block, x, y, options))
        return 0;

    auto tt = block->tiletype[x][y];
    auto mat = tileMaterial(tt);
    auto shape = tileShape(tt);

    if (mat == tiletype_material::GRASS_LIGHT ||
        mat == tiletype_material::GRASS_DARK ||
        mat == tiletype_material::PLANT ||
        tt == tiletype::TreeTrunkPillar)
    {   // Already appropriate tile
        DEBUG(log).print("Tiletype no change.\n");
        return 1;
    }
    else if (mat == tiletype_material::STONE ||
        mat == tiletype_material::LAVA_STONE ||
        mat == tiletype_material::MINERAL)
    {   // Muddy non-feature stone
        for (auto blev : block->block_events)
        {   // Remove mud spatter
            if (blev->getType() != block_square_event_type::material_spatter)
                continue;

            auto &ms_ev = *(df::block_square_event_material_spatterst *)blev;
            if (ms_ev.mat_type == builtin_mats::MUD)
            {
                ms_ev.amount[x][y] = 0;
                DEBUG(log).print("Removed tile mud.\n");
                break;
            }
        }
    }

    if (shape == tiletype_shape::FLOOR)
    {   // Handle random variant, ashes
        DEBUG(log).print("Tiletype to random grass floor.\n");
        block->tiletype[x][y] = findRandomVariant((rand() & 1) ? tiletype::GrassLightFloor1 : tiletype::GrassDarkFloor1);
    }
    else
    {
        auto new_mat = (rand() & 1) ? tiletype_material::GRASS_LIGHT : tiletype_material::GRASS_DARK;
        auto new_tt = findTileType(shape, new_mat, tiletype_variant::NONE, tiletype_special::NONE, nullptr);
        DEBUG(log).print("Tiletype to %s.\n", ENUM_KEY_STR(tiletype, new_tt).c_str());
        block->tiletype[x][y] = new_tt;
    }

    return 1;
}

int regrass_block(df::map_block *block, regrass_options options)
{   // Regrass single block
    int count = 0;
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 16; x++)
            count += regrass_tile(block, x, y, options);
    }

    return count;
}

int regrass_map(regrass_options options)
{   // Regrass entire map
    int count = 0;
    for (auto block : world->map.map_blocks)
        count += regrass_block(block, options);

    return count;
}

int regrass_zlevel(regrass_options options)
{   // Regrass current z-level
    return 0;
}

int regrass_cuboid(df::coord pos_1, df::coord pos_2, regrass_options options)
{   // Regrass cuboid bounded by pos_1, pos_2
    return 0;
}

DFhackCExport command_result plugin_init(color_ostream &out, vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "regrass",
        "Regrow surface grass and cavern moss.",
        df_regrass));
    return CR_OK;
}

command_result df_regrass(color_ostream &out, vector<string> &parameters)
{
    regrass_options options;
    df::coord pos_1, pos_2;
    bool show_help = false;

    if (!Lua::CallLuaModuleFunction(out, "plugins.regrass", "parse_commandline", parameters, 1,
            [&](lua_State *L) { show_help = !lua_toboolean(L, -1);}))
    {
        out.printerr("Failed Lua call!\n");
        return CR_FAILURE;
    }

    if (show_help ||
        options.point && options.block ||
        options.point && options.zlevel ||
        options.block && options.zlevel)
    {
        return CR_WRONG_USAGE;
    }

    if (!Core::getInstance().isMapLoaded())
    {
        out.printerr("Map not loaded!\n");
        return CR_FAILURE;
    }

    if (options.force && options.forced_plant < 0)
    {
        out.printerr("Plant raw not found for force regrass!");
        return CR_FAILURE;
    }

    CoreSuspender suspend;

    int count = 0;
    if (options.point || options.block)
    {
        if (!pos_1.isValid())
        {
            pos_1 = Gui::getCursorPos();
            if (!pos_1.isValid())
            {
                out.printerr("Keyboard cursor required!\n");
                return CR_FAILURE;
            }
        }

        if (pos_2.isValid())
        {
            DEBUG(log).print("Regrassing cuboid...\n");
            count = regrass_cuboid(pos_1, pos_2, options);
        }
        else
        {
            auto b = Maps::getTileBlock(pos_1);
            if (!b)
            {
                out.printerr("No map block at position!\n");
                return CR_FAILURE;
            }

            if (options.block)
            {
                DEBUG(log).print("Regrassing block...\n");
                count = regrass_block(b, options);
            }
            else
            {
                DEBUG(log).print("Regrassing single tile...\n");
                count = regrass_tile(b, pos_1.x&15, pos_1.y&15, options);
            }
        }
    }
    else if (options.zlevel)
    {
        DEBUG(log).print("Regrassing z-level...\n");
        count = regrass_zlevel(options);
    }
    else
    {
        DEBUG(log).print("Regrassing map...\n");
        count = regrass_map(options);
    }

    out.print("Regrew %d tiles of grass.\n", count);

    return CR_OK;
}
