local function include_xmake(path)
    includes(path .. "/xmake.lua")
end

include_xmake("./log.cc")
include_xmake("./utility.cc")
