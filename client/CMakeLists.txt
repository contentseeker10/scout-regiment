find_package(CURL REQUIRED)

option(SCOUTREG_NETWORK_FORM
    "Use multipart/form-data networking instead of JSON"
    ON
)

add_executable(scoutreg
    src/main.cpp
)

target_include_directories(scoutreg PRIVATE
    include
)

if(SCOUTREG_NETWORK_FORM)
    target_sources(scoutreg PRIVATE
        src/network_form.cpp
    )
else()
    target_sources(scoutreg PRIVATE
        src/network_json.cpp
    )
endif()

if(WIN32)
    target_sources(scoutreg PRIVATE
        src/win32_scout.cpp
    )
    target_compile_options(scoutreg PRIVATE
        /W4
        /permissive-
    )
elseif(UNIX AND NOT APPLE)
    target_sources(scoutreg PRIVATE
        src/linux_scout.cpp
    )
    target_compile_options(scoutreg PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

target_link_libraries(scoutreg
    PRIVATE
        CURL::libcurl
)
