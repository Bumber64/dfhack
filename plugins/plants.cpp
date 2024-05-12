// Grow shrubs or trees.

#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <vector>
#include <string>

#include "Debug.h"
#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "LuaTools.h"
#include "PluginManager.h"
#include "TileTypes.h"

#include "modules/Gui.h"
#include "modules/MapCache.h"
#include "modules/Maps.h"

#include "df/map_block.h"
#include "df/map_block_column.h"
#include "df/plant.h"
#include "df/plant_raw.h"
#include "df/world.h"

using std::vector;
using std::string;
using namespace DFHack;

DFHACK_PLUGIN("plants");
REQUIRE_GLOBAL(world);

namespace DFHack
{
    DBG_DECLARE(plants, log, DebugCategory::LINFO);
}

struct plants_options
{
    bool grow = false; // Grow saplings into trees
    bool create = false; // Create a plant
    bool del = false; // Remove plants
    bool shrubs = false; // Remove shrubs
    bool saplings = false; // Remove saplings
    bool trees = false; // Remove grown trees
    bool dry_run = false; // Don't actually remove anything
    bool filter_ex = false; // Filter vector excludes if true, else requires its plants
    bool zlevel = false; // Operate on entire z-levels

    int32_t plant_idx = -1; // Plant raw index of plant to create; -2 means print all non-grass ids

    static struct_identity _identity;
};
static const struct_field_info plants_options_fields[] =
{
    { struct_field_info::PRIMITIVE, "grow",      offsetof(plants_options, grow),      &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "create",    offsetof(plants_options, create),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "del",       offsetof(plants_options, del),       &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "shrubs",    offsetof(plants_options, shrubs),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "saplings",  offsetof(plants_options, saplings),  &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "trees",     offsetof(plants_options, trees),     &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "dry_run",   offsetof(plants_options, dry_run),   &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "filter_ex", offsetof(plants_options, filter_ex), &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "zlevel",    offsetof(plants_options, zlevel),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "plant_idx", offsetof(plants_options, plant_idx), &df::identity_traits<int32_t>::identity, 0, 0 },
    { struct_field_info::END }
};
struct_identity plants_options::_identity(sizeof(plants_options), &df::allocator_fn<plants_options>, NULL, "plants_options", NULL, plants_options_fields);

const uint32_t sapling_to_tree_threshold = 120 * 28 * 12 * 3 - 1; // 3 years minus 1 - let the game handle the actual growing-up

command_result df_grow(color_ostream &out, df::coord pos, vector<int32_t> *filter = nullptr, bool filter_ex = false) // TODO: take cuboid
{
    bool do_filter = filter && !filter->empty();
    if (do_filter) // Sort filter vector
        std::sort(filter->begin(), filter->end());

    if (pos.isValid())
    {   // Single sapling; TODO: unify code
        auto tt = Maps::getTileType(pos);
        if (tt && (*tt == tiletype::Sapling || *tt == tiletype::SaplingDead))
        {
            auto plant = Maps::getPlantAtTile(pos);
            if (plant && !plant->tree_info && !plant->flags.bits.is_shrub)
            {
                if (*tt == tiletype::SaplingDead)
                    *tt = tiletype::Sapling; // Revive sapling

                plant->damage_flags.bits.is_dead = false;
                plant->grow_counter = sapling_to_tree_threshold;

                out.print("Sapling will grow if possible.\n");
                return CR_OK;
            }

            out.printerr("Tiletype was sapling, but invalid plant!\n");
            return CR_FAILURE;
        }

        out.printerr("No sapling at pos!\n");
        return CR_FAILURE;
    }

    int grown = 0;
    for (auto plant : world->plants.all)
    {   // Do entire map
        if (plant->tree_info || plant->flags.bits.is_shrub)
            continue; // Not sapling
        else if (do_filter && (vector_contains(*filter, (int32_t)plant->material) == filter_ex))
            continue; // Filtered out

        auto tt = Maps::getTileType(plant->pos);
        if (!tt || (*tt != tiletype::Sapling && *tt != tiletype::SaplingDead))
        {
            out.printerr("Invalid sapling tiletype at (%d, %d, %d): %s!\n",
                plant->pos.x, plant->pos.y, plant->pos.z,
                tt ? ENUM_KEY_STR(tiletype, *tt).c_str() : "No map block!");
            continue; // Bad tiletype
        }
        else if (*tt == tiletype::SaplingDead)
            *tt = tiletype::Sapling; // Revive sapling

        plant->damage_flags.bits.is_dead = false;
        plant->grow_counter = sapling_to_tree_threshold;
        grown++;
    }

    out.print("%d plants set to grow.\n", grown);
    return CR_OK;
}

command_result df_createplant(color_ostream &out, df::coord pos, int32_t plant_idx)
{
    auto block = Maps::getTileBlock(pos);
    auto col = Maps::getBlockColumn((pos.x / 48) * 3, (pos.y / 48) * 3);
    if (!block || !col)
    {
        out.printerr("No map block at pos!\n");
        return CR_FAILURE;
    }

    auto tt = Maps::getTileType(pos);
    if (!tt || tileShape(*tt) != tiletype_shape::FLOOR)
    {
        out.printerr("Plants can only be placed on floors!\n");
        return CR_FAILURE;
    }
    else // Check tile mat and building occ
    {
        auto mat = tileMaterial(*tt);
        if (mat != tiletype_material::SOIL &&
            mat != tiletype_material::GRASS_DARK &&
            mat != tiletype_material::GRASS_LIGHT)
        {
            out.printerr("Plants can only be placed on dirt or grass!\n");
            return CR_FAILURE;
        }

        auto occ = Maps::getTileOccupancy(pos);
        if (occ && occ->bits.building > tile_building_occ::None)
        {
            out.printerr("Building present at pos!\n");
            return CR_FAILURE;
        }
    }

    auto p_raw = vector_get(world->raws.plants.all, plant_idx);
    if (!p_raw)
    {
        out.printerr("Plant raw not found!\n");
        return CR_FAILURE;
    }

    auto plant = df::allocate<df::plant>();
    if (p_raw->flags.is_set(plant_raw_flags::TREE))
        plant->hitpoints = 400000;
    else
    {
        plant->hitpoints = 100000;
        plant->flags.bits.is_shrub = true;
    }

    // This is correct except for RICE, DATE_PALM, and underground plants
    // near pool/river/brook. These have both WET and DRY flags.
    // Should more properly detect if near surface water feature
    if (!p_raw->flags.is_set(plant_raw_flags::DRY))
        plant->flags.bits.watery = true;

    plant->material = plant_idx;
    plant->pos = pos;
    plant->update_order = rand() % 10;

    world->plants.all.push_back(plant);
    switch (plant->flags.whole & 3) // watery, is_shrub
    {
        case 0: world->plants.tree_dry.push_back(plant); break;
        case 1: world->plants.tree_wet.push_back(plant); break;
        case 2: world->plants.shrub_dry.push_back(plant); break;
        case 3: world->plants.shrub_wet.push_back(plant); break;
    }

    col->plants.push_back(plant);
    if (plant->flags.bits.is_shrub)
        *tt = tiletype::Shrub;
    else
        *tt = tiletype::Sapling;

    return CR_OK;
}

command_result df_plant(color_ostream &out, vector <string> &parameters)
{
    plants_options options;
    df::coord pos_1, pos_2;
    vector<int32_t> filter; // Unsorted

    CoreSuspender suspend;

    if (!Lua::CallLuaModuleFunction(out, "plugins.plants", "parse_commandline",
        std::make_tuple(&options, &pos_1, &pos_2, &filter, parameters)))
    {
        return CR_WRONG_USAGE;
    }
    else if (!(options.grow || options.create || options.del))
    {   // No sub-command
        out.printerr("Choose a sub-command (grow/create/remove)\n");
        return CR_WRONG_USAGE;
    }
    else if (options.grow && options.create ||
        options.grow && options.del ||
        options.create && options.del)
    {   // Sub-commands are mutually exclusive
        out.printerr("Choose only one of grow/create/remove!\n");
        return CR_WRONG_USAGE;
    }
    else if (!options.del && (options.shrubs || options.saplings || options.trees || options.dry_run))
    {   // Don't use remove options outside remove
        out.printerr("Can't use remove's options without --remove!\n");
        return CR_WRONG_USAGE;
    }
    else if (options.trees)
    {   // TODO: implement
        out.printerr("Tree removal not implemented!\n");
        return CR_NOT_IMPLEMENTED;
    }
    else if (options.plant_idx == -2)
    {   // Print all non-grass raw ids
        out.print("--- Shrubs ---\n");
        for (auto p_raw : world->raws.plants.bushes)
            out.print("%d: %s\n", p_raw->index, p_raw->id.c_str());

        out.print("--- Saplings ---\n");
        for (auto p_raw : world->raws.plants.trees)
            out.print("%d: %s\n", p_raw->index, p_raw->id.c_str());

        return CR_OK;
    }

    DEBUG(log, out).print("pos_1 = (%d, %d, %d)\npos_2 = (%d, %d, %d)\n",
        pos_1.x, pos_1.y, pos_1.z, pos_2.x, pos_2.y, pos_2.z);

    if (!Core::getInstance().isMapLoaded())
    {
        out.printerr("Map not loaded!\n");
        return CR_FAILURE;
    }
    else if (!pos_1.isValid())
    {   // Attempt to use cursor for pos if active
        Gui::getCursorCoords(pos_1);
        DEBUG(log, out).print("Try to use cursor (%d, %d, %d) for pos_1.\n",
            pos_1.x, pos_1.y, pos_1.z);
    }

    if (options.create)
    {   // Check filter, plant raw, and pos
        if (!filter.empty())
        {
            out.printerr("Cannot use filter/exclude with --create!\n");
            return CR_FAILURE;
        }

        DEBUG(log, out).print("plant_idx = %d\n", options.plant_idx);
        auto p_raw = vector_get(world->raws.plants.all, options.plant_idx);
        if (p_raw)
        {
            DEBUG(log, out).print("Plant raw: %s\n", p_raw->id.c_str());
            if (p_raw->flags.is_set(plant_raw_flags::GRASS))
            {
                out.printerr("Plant raw was grass: %d (%s)\n", options.plant_idx, p_raw->id.c_str());
                return CR_FAILURE;
            }
        }
        else
        {
            out.printerr("Plant raw not found for --create: %d\n", options.plant_idx);
            return CR_FAILURE;
        }

        if (pos_2.isValid())
        {
            out.printerr("Can't accept second pos for --create!\n");
            return CR_WRONG_USAGE;
        }
        else if (!pos_1.isValid())
        {
            out.printerr("Invalid pos for --create!\n");
            return CR_WRONG_USAGE;
        }
    }
    else if(!filter.empty())
    {   // Validate filter plant raws
        for (auto idx : filter)
        {
            DEBUG(log, out).print("Filter/exclude test idx: %d\n", idx);
            auto p_raw = vector_get(world->raws.plants.all, idx);
            if (p_raw)
            {
                DEBUG(log, out).print("Filter/exclude raw: %s\n", p_raw->id.c_str());
                if (p_raw->flags.is_set(plant_raw_flags::GRASS))
                {
                    out.printerr("Filter/exclude plant raw was grass: %d (%s)\n", idx, p_raw->id.c_str());
                    return CR_FAILURE;
                }
                else if (options.grow && !p_raw->flags.is_set(plant_raw_flags::SAPLING))
                {   // User might copy-paste filters between grow and remove, so just log this
                    DEBUG(log, out).print("Filter/exclude shrub with --grow: %d (%s)\n", idx, p_raw->id.c_str());
                }
            }
            else
            {
                out.printerr("Plant raw not found for filter/exclude: %d\n", idx);
                return CR_FAILURE;
            }
        }
    }

    if (options.grow)
        return df_grow(out, pos_1, &filter, options.filter_ex); // TODO: give cuboid
    else if (options.create)
        return df_createplant(out, pos_1, options.plant_idx);
    else if (options.del) // TODO: ignore filter if not shrub/sapling/tree
        return CR_OK;

    return CR_WRONG_USAGE;
}

DFhackCExport command_result plugin_init(color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "plant",
        "Grow shrubs or trees.",
        df_plant));

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    return CR_OK;
}
