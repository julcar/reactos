project(webserver)
add_executable(webserver mongoose.c webserver.c)
target_link_libraries(webserver msvcrt gdi32 shell32 advapi32 comdlg32 wsock32 ws2_32)