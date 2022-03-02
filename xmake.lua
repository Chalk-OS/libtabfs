add_rules("mode.debug", "mode.release")

add_requires("cxxspec")

-- public target that gets installed & should be used by any tool that also lives outside of this repo
target("libtabfs")
    set_kind("static")
    add_files("src/*.c")
    add_installfiles("include/(*.h)", {prefixdir = "include/libtabfs"})
    add_includedirs("include", {public = true})

    set_basename("tabfs")

    if is_mode("debug") then
        add_defines("DEBUG")
    end

-- local target that is used for the specs to enable more debugging capabilities and
-- also so that xmake dosnt use the system-wide installed libtabfs!
target("local_libtabfs")
    set_default(false)
    set_kind("static")
    add_files("src/*.c")
    add_includedirs("include", {public = true})
    add_defines("DEBUG")
    add_defines("LIBTABFS_DEBUG_PRINTF")

target("specs")
    set_default(false)
    set_kind("binary")
    add_deps("local_libtabfs")
    add_packages("cxxspec")
    add_files("specs/*.cpp", "utils/dump.cpp")
    add_includedirs("utils")
    set_toolchains("clang")

target("tabfs_inspect")
    set_kind("binary")
    add_deps("libtabfs")
    add_files("utils/tabfs_inspect.cpp", "utils/dump.cpp")
    add_includedirs("utils")