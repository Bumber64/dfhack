// Grow and remove shrubs or trees.

#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <vector>
#include <string>

//#include "DataDefs.h"
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

#include "df/block_square_event_grassst.h"
#include "df/map_block.h"
#include "df/map_block_column.h"
#include "df/plant.h"
#include "df/plant_raw.h"
#include "df/world.h"

using std::vector;
using std::string;
using namespace DFHack;

DFHACK_PLUGIN("plant");
REQUIRE_GLOBAL(world);

namespace DFHack
{
    DBG_DECLARE(plant, log, DebugCategory::LINFO);
}

struct cuboid
{
    int16_t x_min = -30000;
    int16_t x_max = -30000;
    int16_t y_min = -30000;
    int16_t y_max = -30000;
    int16_t z_min = -30000;
    int16_t z_max = -30000;

    bool isValid()
    {   // False if any bound is < 0
        return x_min >= 0 && x_max >= 0 &&
            y_min >= 0 && y_max >= 0 &&
            z_min >= 0 && z_max >= 0;
    }

    bool addPos(int16_t x, int16_t y, int16_t z)
    {   // Expand cuboid to include point. Return false if point invalid
        if (x < 0 || y < 0 || z < 0)
            return false;

        x_min = (x_min < 0 || x < x_min) ? x : x_min;
        x_max = (x_max < 0 || x > x_max) ? x : x_max;

        y_min = (y_min < 0 || y < y_min) ? y : y_min;
        y_max = (y_max < 0 || y > y_max) ? y : y_max;

        z_min = (z_min < 0 || z < z_min) ? z : z_min;
        z_max = (z_max < 0 || z > z_max) ? z : z_max;

        return true;
    }
    inline bool addPos(df::coord pos) { return addPos(pos.x, pos.y, pos.z); }

    bool testPos(int16_t x, int16_t y, int16_t z)
    {   // Return true if point inside cuboid. Make sure cuboid is valid first!
        return x >= x_min && x <= x_max &&
            y >= y_min && y <= y_max &&
            z >= z_min && z <= z_max;
    }
    inline bool testPos(df::coord pos) { return testPos(pos.x, pos.y, pos.z); }
};

struct plant_options
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
    int32_t age = -1; // Set plant to this age for grow/create; -1 for default

    static struct_identity _identity;
};
static const struct_field_info plant_options_fields[] =
{
    { struct_field_info::PRIMITIVE, "grow",      offsetof(plant_options, grow),      &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "create",    offsetof(plant_options, create),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "del",       offsetof(plant_options, del),       &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "shrubs",    offsetof(plant_options, shrubs),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "saplings",  offsetof(plant_options, saplings),  &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "trees",     offsetof(plant_options, trees),     &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "dry_run",   offsetof(plant_options, dry_run),   &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "filter_ex", offsetof(plant_options, filter_ex), &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "zlevel",    offsetof(plant_options, zlevel),    &df::identity_traits<bool>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "plant_idx", offsetof(plant_options, plant_idx), &df::identity_traits<int32_t>::identity, 0, 0 },
    { struct_field_info::PRIMITIVE, "age",       offsetof(plant_options, age),       &df::identity_traits<int32_t>::identity, 0, 0 },
    { struct_field_info::END }
};
struct_identity plant_options::_identity(sizeof(plant_options), &df::allocator_fn<plant_options>, NULL, "plant_options", NULL, plant_options_fields);

const int32_t sapling_to_tree_threshold = 120 * 28 * 12 * 3 - 1; // 3 years minus 1; let the game handle the actual growing-up

command_result df_grow(color_ostream &out, const cuboid &bounds, int32_t target_age = -1, vector<int32_t> *filter = nullptr, bool filter_ex = false)
{
    if (!bounds.isValid())
    {
        out.printerr("Invalid cuboid! (%d:%d, %d:%d, %d:%d)\n",
            bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.z_min, bounds.z_max);
        return CR_FAILURE;
    }

    bool do_filter = filter && !filter->empty();
    if (do_filter) // Sort filter vector
        std::sort(filter->begin(), filter->end());

    int32_t age = target_age < 0 ? sapling_to_tree_threshold : target_age;
    bool do_trees = age > sapling_to_tree_threshold;

    int grown = 0;
    for (auto plant : world->plants.all)
    {
        if (plant->flags.bits.is_shrub)
            continue; // Shrub
        else if (!bounds.testPos(plant->pos))
            continue; // Outside cuboid
        else if (do_filter && (vector_contains(*filter, (int32_t)plant->material) == filter_ex))
            continue; // Filtered out
        else if (plant->tree_info)
        {   // Tree
            if (do_trees && !plant->damage_flags.bits.is_dead && plant->grow_counter < age) // TODO: bits.dead
                plant->grow_counter = age;
            continue; // Next plant
        }

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

        plant->damage_flags.bits.is_dead = false; // TODO: bits.dead
        plant->grow_counter = age;
        grown++;
    }

    out.print("%d saplings set to grow.\n", grown);
    return CR_OK;
}

command_result df_createplant(color_ostream &out, df::coord pos, int32_t plant_idx, int32_t age = -1)
{
    auto col = Maps::getBlockColumn((pos.x / 48)*3, (pos.y / 48)*3);
    if (!Maps::getTileBlock(pos) || !col)
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
    plant->grow_counter = age < 0 ? 0 : age;
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

static bool uncat_plant(df::plant *plant)
{   // Remove plant from extra vectors
    vector<df::plant *> *vec = NULL;
    switch (plant->flags.whole & 3) // watery, is_shrub
    {
        case 0: vec = &world->plants.tree_dry; break;
        case 1: vec = &world->plants.tree_wet; break;
        case 2: vec = &world->plants.shrub_dry; break;
        default: vec = &world->plants.shrub_wet; break;
    }

    for (vector<df::plant *>::iterator it = vec->end() - 1; it > vec->begin(); it--)
    {   // Not sorted, but more likely near end
        if (*it == plant)
        {
            vec->erase(it);
            break;
        }
    }

    auto col = Maps::getBlockColumn((plant->pos.x / 48)*3, (plant->pos.y / 48)*3);
    if (!col)
        return false;

    vec = &col->plants;
    for (vector<df::plant *>::iterator it = vec->end() - 1; it > vec->begin(); it--)
    {   // Not sorted, but more likely near end
        if (*it == plant)
        {
            vec->erase(it);
            break;
        }
    }

    return true;
}

static bool has_grass(df::map_block *block, int tx, int ty)
{   // Block tile has grass
    for (auto blev : block->block_events)
    {
        if (blev->getType() != block_square_event_type::grass)
            continue;

        auto &g_ev = *(df::block_square_event_grassst*)blev;
        if (g_ev.amount[tx][ty] > 0)
            return true;
    }

    return false;
}

static void set_tt(df::coord pos)
{   // Set tiletype to grass or soil floor
    auto block = Maps::getTileBlock(pos);
    if (!block)
        return;

    int tx = pos.x & 15, ty = pos.y & 15;
    if (has_grass(block, tx, ty))
        block->tiletype[tx][ty] = findRandomVariant((rand() & 1) ? tiletype::GrassLightFloor1 : tiletype::GrassDarkFloor1);
    else
        block->tiletype[tx][ty] = findRandomVariant(tiletype::SoilFloor1);
}

command_result df_removeplant(color_ostream &out, const cuboid &bounds, const plant_options &options, vector<int32_t> *filter = nullptr)
{
    if (!bounds.isValid())
    {
        out.printerr("Invalid cuboid! (%d:%d, %d:%d, %d:%d)\n",
            bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.z_min, bounds.z_max);
        return CR_FAILURE;
    }

    bool by_type = options.shrubs || options.saplings || options.trees;
    bool do_filter = by_type && filter && !filter->empty();
    if (do_filter) // Sort filter vector
        std::sort(filter->begin(), filter->end());


    int count = 0, count_bad = 0;
    auto &vec = world->plants.all;
    for (vector<df::plant *>::iterator it = vec.end() - 1; it > vec.begin(); it--)
    {
        auto plant = *it;

        if (plant->tree_info) // TODO: handle trees
            continue; // Not implemented
        else if (by_type)
        {
            /*if (plant->tree_info && !options.trees)
                continue; // Not removing trees
            else*/
            if (plant->flags.bits.is_shrub)
            {
                if (!options.shrubs)
                    continue; // Not removing shrubs
            }
            else if (!options.saplings)
                continue; // Not removing saplings
        }

        if (!bounds.testPos(plant->pos))
            continue; // Outside cuboid
        else if (do_filter && (vector_contains(*filter, (int32_t)plant->material) == options.filter_ex))
            continue; // Filtered out

        bool bad_tt = false;
        auto tt = Maps::getTileType(plant->pos);
        if (tt)
        {
            if (plant->flags.bits.is_shrub)
            {
                if (tileShape(*tt) != tiletype_shape::SHRUB)
                {
                    out.printerr("Bad shrub tiletype at (%d, %d, %d): %s\n",
                        plant->pos.x, plant->pos.y, plant->pos.z,
                        ENUM_KEY_STR(tiletype, *tt).c_str());
                    bad_tt = true;
                }
            }
            else if (!plant->tree_info)
            {
                if (tileShape(*tt) != tiletype_shape::SAPLING)
                {
                    out.printerr("Bad sapling tiletype at (%d, %d, %d): %s\n",
                        plant->pos.x, plant->pos.y, plant->pos.z,
                        ENUM_KEY_STR(tiletype, *tt).c_str());
                    bad_tt = true;
                }
            }
            // TODO: trees
        }
        else
        {
            out.printerr("Bad plant tiletype at (%d, %d, %d): No map block!\n",
                plant->pos.x, plant->pos.y, plant->pos.z);
            bad_tt = true;
        }

        count++;
        if (bad_tt)
            count_bad++;

        if (!options.dry_run)
        {
            if (!uncat_plant(plant))
                out.printerr("Remove plant: No block column at (%d, %d)!\n", plant->pos.x, plant->pos.y);

            if (!bad_tt) // TODO: trees
                set_tt(plant->pos);

            vec.erase(it);
            delete plant;
        }
    }

    out.print("plants%s removed: %d (%d bad)\n", options.dry_run ? " that would be" : "", count, count_bad);
    return CR_OK;
}

command_result df_plant(color_ostream &out, vector<string> &parameters)
{
    plant_options options;
    cuboid bounds;
    df::coord pos_1, pos_2;
    vector<int32_t> filter; // Unsorted

    CoreSuspender suspend;

    if (!Lua::CallLuaModuleFunction(out, "plugins.plant", "parse_commandline",
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
    else if (options.del && options.age >= 0)
    {   // Don't use age with remove
        out.printerr("Can't use --age with --remove!\n");
        return CR_WRONG_USAGE;
    }
    else if (options.trees)
    {   // TODO: implement
        out.printerr("Tree removal not implemented!\n");
        return CR_FAILURE;
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

    out.print("pos_1 = (%d, %d, %d)\npos_2 = (%d, %d, %d)\n\n",
        pos_1.x, pos_1.y, pos_1.z, pos_2.x, pos_2.y, pos_2.z);
    out.print("grow = %d\n", options.grow);
    out.print("create = %d\n", options.create);
    out.print("remove = %d\n", options.del);
    out.print("shrubs = %d\n", options.shrubs);
    out.print("saplings = %d\n", options.saplings);
    out.print("trees = %d\n", options.trees);
    out.print("dry_run = %d\n", options.dry_run);
    out.print("filter_ex = %d\n", options.filter_ex);
    out.print("zlevel = %d\n", options.zlevel);
    out.print("\nplant_idx = %d\n", options.plant_idx);
    out.print("age = %d\n", options.age);
    out.print("\nfilter =\n");
    for (auto i : filter)
        out.print("%d\n", i);
    out.print("Done.\n");
    if (true) return CR_OK; //DEBUG

    if (!Core::getInstance().isMapLoaded())
    {
        out.printerr("Map not loaded!\n");
        return CR_FAILURE;
    }

    if (options.create)
    {   // Check improper options and plant raw
        if (options.zlevel)
        {
            out.printerr("Cannot use --zlevel with --create!\n");
            return CR_FAILURE;
        }
        else if (!filter.empty())
        {
            out.printerr("Cannot use filter/exclude with --create!\n");
            return CR_FAILURE;
        }
        else if (pos_2.isValid())
        {
            out.printerr("Can't accept second pos for --create!\n");
            return CR_WRONG_USAGE;
        }
        
        if (!pos_1.isValid())
        {   // Attempt to use cursor for pos if active
            Gui::getCursorCoords(pos_1);
            DEBUG(log, out).print("Try to use cursor (%d, %d, %d) for pos_1.\n",
                pos_1.x, pos_1.y, pos_1.z);

            if (!pos_1.isValid())
            {
                out.printerr("Invalid pos for --create! Make sure keyboard cursor is active if not entering pos manually!\n");
                return CR_WRONG_USAGE;
            }
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
    }
    else // options.grow || options.remove
    {   // Check filter and setup cuboid
        if (!filter.empty())
        {   // Validate filter plant raws
            if (!(options.shrubs || options.saplings || options.trees))
            {
                out.printerr("Filter/exclude set, but not targeting shrubs/saplings!\n"); // TODO: trees
                return CR_WRONG_USAGE;
            }

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

        if (options.zlevel)
        {   // Adjusted cuboid
            if (!pos_1.isValid())
            {
                DEBUG(log, out).print("pos_1 invalid and --zlevel. Using viewport.\n");
                pos_1.z = Gui::getViewportPos().z;
            }

            if (!pos_2.isValid())
            {
                DEBUG(log, out).print("pos_2 invalid and --zlevel. Using pos_1.\n");
                pos_2.z = pos_1.z;
            }

            bounds.addPos(0, world->map.y_count-1, pos_1.z);
            bounds.addPos(world->map.x_count-1, 0, pos_2.z);
        }
        else if (pos_1.isValid())
        {   // Cuboid or single point
            bounds.addPos(pos_1);
            bounds.addPos(pos_2); // Point if invalid
        }
        else // Entire map
        {
            if (pos_2.isValid())
            {
                out.printerr("First pos invalid! Second pos okay.\n");
                return CR_FAILURE;
            }

            bounds.addPos(0, 0, world->map.z_count-1);
            bounds.addPos(world->map.x_count-1, world->map.y_count-1, 0);
        }

        DEBUG(log, out).print("bounds = (%d:%d, %d:%d, %d:%d)\n",
            bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.z_min, bounds.z_max);

        if (!bounds.isValid())
        {
            out.printerr("Invalid cuboid! (%d:%d, %d:%d, %d:%d)\n",
                bounds.x_min, bounds.x_max, bounds.y_min, bounds.y_max, bounds.z_min, bounds.z_max);
            return CR_FAILURE;
        }
    }

    if (true) return CR_OK; //DEBUG

    if (options.grow)
        return df_grow(out, bounds, options.age, &filter, options.filter_ex);
    else if (options.create)
        return df_createplant(out, pos_1, options.plant_idx, options.age);
    else if (options.del)
        return df_removeplant(out, bounds, options, &filter);

    return CR_WRONG_USAGE;
}

DFhackCExport command_result plugin_init(color_ostream &out, vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "plant",
        "Grow and remove shrubs or trees.",
        df_plant));

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    return CR_OK;
}
