add_rules("mode.debug", "mode.release")
-- add_subdirs("xmgr")
includes("xmgr")


toolchain("clang_vs")
    set_kind("standalone")
    -- set_bindir("C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\Llvm\\x64\\bin")
    on_load(function (toolchain)
        local tooldir = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\Llvm\\x64\\bin"
        toolchain:add("runenvs","PATH",tooldir)
        toolchain:set("toolset","cc", "clang-cl")
        toolchain:set("toolset","cxx", "clang-cl")
    end)
toolchain_end()

set_config("qt","C:/Lib/Qt/6.6.1")
-- set_config("qt","E:/qt5.15.2/build/install")
-- set_config("qt","C:/Lib/Qt/5.15.2/msvc2019_64")
-- set_toolchains("clang_vs")

-- -- QT 需要指定运行时，不然退出时会报错
if is_mode("debug") then
    set_runtimes("MDd")
else
    set_runtimes("MT")
end

-- option("vkResources")
--     before_check(function(option)
--         local res = "$(buildir)/$(plat)/$(arch)/$(mode)/Resources"

--         if os.isdir(res) then
--             return
--         end

--         os.cp("Resources",res)
--     end)
-- option_end()

target("vkExample")
    set_languages("c++17")
    add_rules("qt.widgetapp")
    add_options("vkResources")
    add_rules("glsl.spv",{outputdir="$(buildir)/$(plat)/$(arch)/$(mode)/Shaders"})
    add_files("src/Shaders/*.frag","src/Shaders/*.vert")
    add_headerfiles("src/Shaders/*.frag","src/Shaders/*.vert")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    -- add files with Q_OBJECT meta (only for qt.moc)
    add_files("src/mainwindow.h")

    add_frameworks("QtCore","QtGui","QtWidgets") --QT5

    add_includedirs("C:/Lib/VulkanSDK/1.3.224.1/Include","src")
    add_linkdirs("C:/Lib/VulkanSDK/1.3.224.1/Lib")
    add_links("vulkan-1")

    -- add_defines("NOMINMAX")
    add_defines("VK_USE_PLATFORM_WIN32_KHR")
    add_defines( "UNICODE", "_UNICODE")
    add_cxflags("/execution-charset:utf-8")
    add_cxflags("/source-charset:utf-8")

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

