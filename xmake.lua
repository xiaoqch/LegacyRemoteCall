add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

if is_config("target_type", "server") then
    add_requires("levilamina 1.2.0-rc.2", {configs = {target_type = "server"}})
else
    add_requires("levilamina 1.2.0-rc.2", {configs = {target_type = "client"}})
end

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable test")
option_end()

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

target("LegacyRemoteCall")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_files("src/remote_call/**.cpp")
    add_includedirs("src")
    add_headerfiles("src/(remote_call/api/**.h)")
    if has_config("test") then
        add_files("src/test/native/**.cpp")
    end
    -- if is_config("target_type", "server") then
    --     add_includedirs("src-server")
    --     add_files("src-server/**.cpp")
    -- else
    --     add_includedirs("src-client")
    --     add_files("src-client/**.cpp")
    -- end
    after_build(function (target)
        function copy_headers(src_dir, dst_dir) 
            for _, filepath in ipairs(os.files(path.join(src_dir, "**"))) do
                if not filepath:endswith(".cpp") then
                    local relative_path = path.relative(filepath, src_dir)
                    local dst_path = path.join(dst_dir, relative_path)
                    os.cp(filepath, dst_path)
                end
            end
        end
        local bindir = path.join(os.projectdir(), "bin")
        local includedir = path.join(bindir, "include","remote_call", "api")
        local libdir = path.join(bindir, "lib")
        os.mkdir(includedir)
        os.mkdir(libdir)
        copy_headers(path.join(os.projectdir(), "src","remote_call", "api") , includedir)
        os.cp(path.join(target:targetdir(), target:name() .. ".lib"), libdir)
        if has_config("test") then
            local lsetestdir = path.join(bindir, "lse-remote-call-test/")
            os.mkdir(lsetestdir)
            os.cp(path.join(os.projectdir(), "src", "test", "lse", "*.json"), lsetestdir)
            os.cp(path.join(os.projectdir(), "src", "test", "lse", "dist"), lsetestdir)
        end
    end)
