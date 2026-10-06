add_executable(scoutreg
    src/main.cpp
    src/network.cpp
)

target_include_directories(scoutreg PRIVATE 
    include
)

if(WIN32)
    target_sources(scoutreg PRIVATE 
        src/win32_scout.cpp
    )
    target_compile_options(scoutreg PRIVATE /W4 /permissive-)
elseif(UNIX AND NOT APPLE)
    target_sources(scoutreg PRIVATE 
        src/linux_scout.cpp
    )
    target_compile_options(scoutreg PRIVATE -Wall -Wextra -Wpedantic)
endif()