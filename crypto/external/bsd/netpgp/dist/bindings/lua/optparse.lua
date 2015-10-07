-- Lua command line option parser.
-- Interface based on Pythons optparse.
-- http://docs.python.org/lib/module-optparse.html
-- (c) 2008 David Manura, Licensed under the same terms as Lua (MIT license)
--
-- To be used like this:                                                                  
-- t={usage="<some usage message>", version="<version string>"}                           
-- op=OptionParser(t)                                                                     
-- op=add_option{"<opt>", action=<action>, dest=<dest>, help="<help message for this option>"}
--
-- with :
--   <opt> the option string to be used (can be anything, if one letter opt, then should be -x val, more letters: -xy=val )
--   <action> one of
--   - store: store in options as key, val                                                  
--   - store_true: stores key, true                                                         
--   - store_false: stores key, false
--   <dest> is the key under which the option is saved
--                                      
-- options,args = op.parse_args()
--
-- now options is the table of options (key, val) and args is the table with non-option arguments.
-- You can use op.fail(message) for failing and op.print_help() for printing the usage as you like.

function OptionParser(t)
  local usage = t.usage
  local version = t.version

  local o = {}
  local option_descriptions = {}
  local option_of = {}

  function o.fail(s) -- extension
    io.stderr:write(s .. '\n')
    os.exit(1)
  end

  function o.add_option(optdesc)
    option_descriptions[#option_descriptions+1] = optdesc
    for _,v in ipairs(optdesc) do
      option_of[v] = optdesc
    end
  end
  function o.parse_args()
    -- expand options (e.g. "--input=file" -> "--input", "file")
    local arg = {unpack(arg)}
    for i=#arg,1,-1 do local v = arg[i]
      local flag, val = v:match('^(%-%-%w+)=(.*)')
      if flag then
        arg[i] = flag
        table.insert(arg, i+1, val)
      end
    end

    local options = {}
    local args = {}
    local i = 1
    while i <= #arg do local v = arg[i]
      local optdesc = option_of[v]
      if optdesc then
        local action = optdesc.action
        local val
        if action == 'store' or action == nil then
          i = i + 1
          val = arg[i]
          if not val then o.fail('option requires an argument ' .. v) end
        elseif action == 'store_true' then
          val = true
        elseif action == 'store_false' then
          val = false
        end
        options[optdesc.dest] = val
      else
        if v:match('^%-') then o.fail('invalid option ' .. v) end
        args[#args+1] = v
      end
      i = i + 1
    end
    if options.help then
      o.print_help()
      os.exit()
    end
    if options.version then
      io.stdout:write(t.version .. "\n")
      os.exit()
    end
    return options, args
  end

  local function flags_str(optdesc)
    local sflags = {}
    local action = optdesc.action
    for _,flag in ipairs(optdesc) do
      local sflagend
      if action == nil or action == 'store' then
        local metavar = optdesc.metavar or optdesc.dest:upper()
        sflagend = #flag == 2 and ' ' .. metavar
                              or  '=' .. metavar
      else
        sflagend = ''
      end
      sflags[#sflags+1] = flag .. sflagend
    end
    return table.concat(sflags, ', ')
  end

  function o.print_help()
    io.stdout:write("Usage: " .. usage:gsub('%%prog', arg[0]) .. "\n")
    io.stdout:write("\n")
    io.stdout:write("Options:\n")
    for _,optdesc in ipairs(option_descriptions) do
      io.stdout:write("  " .. flags_str(optdesc) ..
                      "  " .. optdesc.help .. "\n")
    end
  end
  o.add_option{"--help", action="store_true", dest="help",
               help="show this help message and exit"}
  if t.version then
    o.add_option{"--version", action="store_true", dest="version",
                 help="output version info."}
  end
  return o
end

