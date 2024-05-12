local _ENV = mkmodule('plugins.plants')

local argparse = require('argparse')
local utils = require('utils')

local function search_str(s)
    return dfhack.upperCp437(dfhack.toSearchNormalized(s))
end

local function find_plant_idx(s) --find plant raw index by id string
    local id_str = search_str(s)
    for k, v in ipairs(df.global.world.raws.plants.all) do
        if search_str(v.id) == id_str then
            return k
        end
    end

    return -1
end

local function find_plant(s) --accept index string or match id string
    if s = '' then
        return -2 --will print all non-grass ids (for create)
    elseif tonumber(s) then
        return argparse.nonnegativeInt(s, 'plant_id')
    else
        return find_plant_idx(s)
    end
end

local function build_filter(vec, s)
    if #vec > 0 then
        qerror('Filter list already populated!')
    end

    local set = {}
    for _,id in ipairs(argparse.stringList(s, 'list')) do
        local idx = find_plant(id)
        if idx < 0 then
            qerror('Plant raw not found: "'..id..'"')
        else
            set[idx] = true
        end
    end

    for idx,_ in pairs(set) do
        vec:insert(idx,'#') --add plant raw indices to vector
    end
end

function parse_commandline(opts, pos_1, pos_2, filter_vec, args)
    local positionals = argparse.processArgsGetopt(args,
    {
        {'g', 'grow', handler=function() opts.grow = true end},
        {'c', 'create', hasArg=true, handler=function(optarg)
            opts.plant_idx = find_plant(optarg) end},
        {'r', 'remove', handler=function() opts.del = true end},
        {'s', 'shrubs', handler=function() opts.shrubs = true end},
        {'p', 'saplings', handler=function() opts.saplings = true end},
        {'t', 'trees', handler=function() opts.trees = true end},
        {'d', 'dryrun', handler=function() opts.dry_run = true end},
        {'f', 'filter', hasArg=true, handler=function(optarg)
            build_filter(filter_vec, optarg) end},
        {'e', 'exclude', hasArg=true, handler=function(optarg)
            opts.filter_ex = true
            build_filter(filter_vec, optarg) end},
        {'z', 'zlevel', handler=function() opts.zlevel = true end},
    })

    if #positionals > 2 then
        qerror('Too many positionals!')
    end

    if positionals[1] then
        utils.assign(pos_1, argparse.coords(positionals[1], 'pos_1', true))
    end

    if positionals[2] then
        utils.assign(pos_2, argparse.coords(positionals[2], 'pos_2', true))
    end
end

return _ENV
