option("tests", { description = "Build tests", default = true })

if has_config("tests") then
    set_group("Tests")
    add_requires("gtest")
    includes("*/xmake.lua")
end