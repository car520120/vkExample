rule("glsl.spv")
    set_extensions(".vert", ".tesc", ".tese", ".geom", ".comp", ".frag", ".comp", ".mesh", ".task", ".rgen", ".rint", ".rahit", ".rchit", ".rmiss", ".rcall", ".glsl")
    on_load(function (target)
        local is_bin2c = target:extraconf("rules", "glsl.spv", "bin2c")
        if is_bin2c then
            local headerdir = path.join(target:autogendir(), "rules", "utils", "glsl2spv")
            if not os.isdir(headerdir) then
                os.mkdir(headerdir)
            end
            target:add("includedirs", headerdir)
        end
    end)
    before_buildcmd_file(function (target, batchcmds, sourcefile_glsl, opt)
        import("lib.detect.find_tool")

        -- get glslangValidator
        local glslc
        local glslangValidator = find_tool("glslangValidator")
        if not glslangValidator then
            glslc = find_tool("glslc")
        end
        assert(glslangValidator or glslc, "glslangValidator or glslc not found!")

        -- glsl to spv
        local targetenv = target:extraconf("rules", "glsl.spv", "targetenv") or "vulkan1.0"
        local outputdir = target:extraconf("rules", "glsl.spv", "outputdir") or path.join(target:autogendir(), "rules", "utils", "glsl2spv")
        local spvfilepath = path.join(outputdir, path.basename(sourcefile_glsl) .. ".spv")
        batchcmds:show_progress(opt.progress, "${color.build.object}generating.glsl2spv %s", sourcefile_glsl)
        batchcmds:mkdir(outputdir)
        if glslangValidator then
            batchcmds:vrunv(glslangValidator.program, {"--target-env", targetenv, "-o", path(spvfilepath), path(sourcefile_glsl)})
        else
            batchcmds:vrunv(glslc.program, {"--target-env", targetenv, "-o", path(spvfilepath), path(sourcefile_glsl)})
        end

        -- do bin2c
        local outputfile = spvfilepath
        local is_bin2c = target:extraconf("rules", "glsl.spv", "bin2c")
        if is_bin2c then
            -- get header file
            local headerdir = outputdir
            local headerfile = path.join(headerdir, path.filename(spvfilepath) .. ".h")
            target:add("includedirs", headerdir)
            outputfile = headerfile

            -- add commands
            local argv = {"lua", "private.utils.bin2c", "--nozeroend", "-i", path(spvfilepath), "-o", path(headerfile)}
            batchcmds:vrunv(os.programfile(), argv, {envs = {XMAKE_SKIP_HISTORY = "y"}})
        end

        -- add deps
        batchcmds:add_depfiles(sourcefile_glsl)
        batchcmds:set_depmtime(os.mtime(outputfile))
        batchcmds:set_depcache(target:dependfile(outputfile))
    end)
