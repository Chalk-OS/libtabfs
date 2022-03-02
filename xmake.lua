add_rules("mode.debug", "mode.release")

add_requires("cxxspec")

target("libtabfs")
    set_kind("static")
    add_files("src/*.c")
    add_installfiles("include/(*.h)", {prefixdir = "include/libtabfs"})
    add_includedirs("include", {public = true})

    set_basename("tabfs")

    if is_mode("debug") then
        add_defines("DEBUG")
    end

target("specs")
    set_default(false)
    set_kind("binary")
    add_deps("libtabfs")
    add_packages("cxxspec")
    add_files("specs/*.cpp", "utils/dump.cpp")
    add_includedirs("utils")

target("tabfs_inspect")
    set_kind("binary")
    add_deps("libtabfs")
    add_files("utils/tabfs_inspect.cpp", "utils/dump.cpp")
    add_includedirs("utils")