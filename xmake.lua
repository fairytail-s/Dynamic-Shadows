set_project("DynamicShadows")
set_version("2.2.4")
set_languages("c++20")
set_allowedplats("android")
set_allowedarchs("arm64-v8a")
add_rules("mode.debug", "mode.release")

option("preloader_lib")
    set_default("")
    set_showmenu(true)
    set_description("Path to the real Android libpreloader.so; empty uses the link-only stub")
option_end()

local preloader_library = get_config("preloader_lib")
if not preloader_library or #preloader_library == 0 then
    target("preloader")
        set_kind("shared")
        set_runtimes("c++_shared")
        set_basename("preloader")
        set_targetdir("build/link-only/$(plat)/$(arch)/$(mode)")
        add_files("stub/preloader_stub.cpp")
        add_includedirs("vendor")
        if is_plat("android") then
            add_shflags("-Wl,-z,max-page-size=16384", {force = true})
        end
    target_end()
end

target("DynamicShadows")
    set_kind("shared")
    set_runtimes("c++_shared")
    set_filename("DynamicShadows.so")
    add_files("src/*.cpp")
    remove_files("src/Log.cpp")
    add_includedirs("include", "vendor")
    set_warnings("all", "extra")
    if preloader_library and #preloader_library > 0 then
        add_linkdirs(path.directory(path.absolute(preloader_library)))
        add_links("preloader")
    else
        add_deps("preloader")
    end
    if is_mode("release") then
        set_optimize("fastest")
        set_strip("all")
        set_policy("build.optimization.lto", true)
    end
    if is_plat("android") then
        add_cxflags("-fPIC", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections")
        add_cxxflags("-fvisibility-inlines-hidden", "-fexceptions")
        add_shflags("-Wl,--gc-sections", "-Wl,-z,max-page-size=16384", "-Wl,-z,relro", "-Wl,-z,now", "-Wl,--build-id=sha1", "-Wl,--no-undefined", {force = true})
        add_links("dl", "m")
    end
    after_build(function (target)
        if not target:is_plat("android") then return end
        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        local args = {}
        if not python then
            python = find_tool("py")
            if python then table.insert(args, "-3") end
        end
        assert(python, "Python 3 is required to package DynamicShadows.levipack")
        table.insert(args, path.join(os.projectdir(), "scripts", "package_levipack.py"))
        table.insert(args, "--library")
        table.insert(args, target:targetfile())
        table.insert(args, "--output")
        table.insert(args, path.join(target:targetdir(), "DynamicShadows.levipack"))
        os.vrunv(python.program, args)
    end)
target_end()
