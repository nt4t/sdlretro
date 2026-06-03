add_library(driver_fbdev STATIC
    fbdev_impl.cpp
    include/fbdev_impl.h
    fbdev_video.cpp
    include/fbdev_video.h
    fbdev_input.cpp
    include/fbdev_input.h
    bmfont.inl
)

target_include_directories(driver_fbdev PUBLIC include PRIVATE ..)
target_link_libraries(driver_fbdev driver_common)
