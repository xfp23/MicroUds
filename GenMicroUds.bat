@echo off
setlocal enabledelayedexpansion

echo Creating MicroUDS project structure...

:: 根目录
set ROOT=MicroUDS

:: 创建目录
mkdir %ROOT%
cd %ROOT%

mkdir inc
mkdir src
mkdir src\core
mkdir src\service
mkdir src\transport
mkdir src\session
mkdir src\security
mkdir src\did
mkdir src\routine
mkdir src\download
mkdir src\utils
mkdir port
mkdir config
mkdir example

:: 创建头文件
type nul > inc\MicroUds.h
type nul > inc\MicroUds_Types.h
type nul > inc\MicroUds_Config.h
type nul > inc\MicroUds_Service.h
type nul > inc\MicroUds_Did.h

:: core
type nul > src\core\uds_core.c
type nul > src\core\uds_dispatch.c
type nul > src\core\uds_state.c
type nul > src\core\uds_context.c
type nul > src\core\uds_nrc.c

:: service
type nul > src\service\uds_service.c
type nul > src\service\uds_srv_10.c
type nul > src\service\uds_srv_11.c
type nul > src\service\uds_srv_27.c
type nul > src\service\uds_srv_28.c
type nul > src\service\uds_srv_34.c
type nul > src\service\uds_srv_36.c
type nul > src\service\uds_srv_37.c

:: transport
type nul > src\transport\isotp.c
type nul > src\transport\isotp.h
type nul > src\transport\isotp_config.h

:: session
type nul > src\session\uds_session.c
type nul > src\session\uds_session.h

:: security
type nul > src\security\uds_security.c
type nul > src\security\uds_security.h

:: did
type nul > src\did\uds_did.c
type nul > src\did\uds_did.h

:: routine
type nul > src\routine\uds_routine.c
type nul > src\routine\uds_routine.h

:: download
type nul > src\download\uds_download.c
type nul > src\download\uds_download.h
type nul > src\download\uds_flash_if.h

:: utils
type nul > src\utils\uds_buffer.c
type nul > src\utils\uds_timer.c
type nul > src\utils\uds_log.c

:: port
type nul > port\uds_port.h
type nul > port\uds_port.c
type nul > port\uds_can_port.c
type nul > port\uds_time_port.c

:: config
type nul > config\MicroUds_DefaultConfig.h

:: example
type nul > example\example_basic.c
type nul > example\example_advanced.c

:: README
type nul > README.md

echo.
echo MicroUDS structure created successfully!
pause