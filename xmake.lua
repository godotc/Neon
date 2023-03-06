add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_requires("glew","glfw", "opengl", "glm","spdlog")
set_languages("c++20")
add_shflags("-fPIC",{force = true})
--set_targetdir("./bin")

if is_mode("debug") then 
    add_cxxflags("-Wall ",{force=true})
    add_cxxflags("-O0")
end


-- target("config")
--     set_kind("headeronly" )
--     add_headerfiles("include/Gloria/config/*.h")

-- target("ownkit")
--     set_kind("static")
--     add_files("src/OwnKit/*.cc")
--     add_includedirs("./include/Gloria/")
--     add_deps("config")
--     add_packages( "glm","glfw","glew")

-- target("logx")
--     set_kind("shared")
--     add_files("src/logx/*.cc")
--     add_includedirs("include/Gloria/")
--     add_deps("ownkit")
--     add_packages("spdlog")



-- target("glinternal")
--     set_kind("static")
--     add_files("src/glinternal/*.cc")
--     add_includedirs("./include/Gloria/")
--     add_deps("ownkit")
--     add_packages("glm","glfw","glew")
    

target("Gloria")
    set_kind("binary")
    add_files("src/**.cc")
    add_includedirs("./include/Gloria/")
    --add_deps("glinternal","logx","ownkit","config")
    add_packages("glfw","glew", "glm", "opengl","spdlog")
