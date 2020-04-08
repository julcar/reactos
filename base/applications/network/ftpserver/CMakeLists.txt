project(ftpserver)
set_cpp(WITH_RUNTIME)
include_directories(
  .
  core
)
list(APPEND CPP_SOURCE
  #core/BlowfishCrypt.cpp
  core/CmdLine.cpp
  #core/Crypto.cpp
  core/Dll.cpp
  core/FSUtils.cpp
  core/Ftps.cpp
  core/FtpsXfer.cpp
  core/Log.cpp
  core/Service.cpp
  core/SiteInfo.cpp
  core/Sock.cpp
  core/SSLSock.cpp
  core/StrUtils.cpp
  core/Termcli.cpp
  core/Termsrv.cpp
  core/Thr.cpp
  core/Timer.cpp
  IndiFileUtils.cpp
  IndiFtps.cpp
  indimain.cpp
  IndiSiteInfo.cpp
)
add_executable(ftpserver ${CPP_SOURCE})
set_module_type(ftpserver win32cui)
add_importlibs(ftpserver user32 ws2_32 advapi32 shell32 msvcrt kernel32)
add_cd_file(TARGET ftpserver DESTINATION reactos/system32 FOR all)