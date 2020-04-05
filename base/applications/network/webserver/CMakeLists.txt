
project(webserver)
add_executable(webserver mongoose.c webserver.c)
set_module_type(webserver win32cui)
add_importlibs(webserver user32 gdi32 shell32 advapi32 comdlg32 ws2_32 msvcrt kernel32)
add_cd_file(TARGET webserver DESTINATION reactos/system32 FOR all)
