local _ENV = mkmodule('plugins.regrass')

local argparse = require('argparse')

function parse_commandline(opts, pos_1, pos_2, args)
    local positionals = argparse.processArgsGetopt(args,
    {
        {'m', 'max', handler=function() opts.max_grass = true end},
        {'n', 'new', handler=function() opts.new_grass = true end},
        {'a', 'ashes', handler=function() opts.ashes = true end},
        {'u', 'mud', handler=function() opts.mud = true end},
        {'b', 'block', handler=function() opts.block = true end},
        {'z', 'zlevel', handler=function() opts.zlevel = true end},
        {'f', 'force', hasArg=true, handler=function(optarg)
            opts.force = true
            print('force', optarg)
        end},
    })

    for _,p in ipairs(positionals) do
        print(_, p)
    end
end

return _ENV
