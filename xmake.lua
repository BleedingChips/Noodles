add_rules("mode.debug", "mode.release")
set_languages("cxxlatest")

if os.scriptdir() == os.projectdir() then 
    includes("../Potato/")
end

target("Noodles")
    set_kind("static")
    add_files("Noodles/*.cpp")
    add_files("Noodles/*.ixx")
    add_files("Noodles/Platform/Windows/Form/*.ixx")
    add_files("Noodles/Platform/Windows/Form/*.cpp")
    add_deps("Potato")
target_end()

if os.scriptdir() == os.projectdir() then
    set_project("Noodles")
    for _, file in ipairs(os.files("Test/*.cpp")) do

        local name = "ZTest_" .. path.basename(file)
        target(name)
            set_kind("binary")
            add_files(file)
            add_deps("Noodles")
        target_end()

    end
end 