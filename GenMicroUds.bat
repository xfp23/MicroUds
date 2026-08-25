@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ========================================
::  MicroUDS Project Structure Generator
:: ========================================

:: =================配置区=================
set ROOT=MicroUDS
set AUTHOR=https://xfp23.github.io
set YEAR=%date:~0,4%
set CURRENT_DATE=%date:~4%
set VERSION=0.1
:: ========================================

echo.
echo ========================================
echo   MicroUDS Project Initialization
echo ========================================
echo.

:: 检查目标目录
if exist "%ROOT%" (
    echo [ERROR] Directory "%ROOT%" already exists!
    goto END
)

:: 创建根目录
echo [1/4] Creating root directory...
mkdir "%ROOT%" || (echo [ERROR] mkdir failed & goto END)
cd "%ROOT%" || (echo [ERROR] cd failed & goto END)

:: 创建目录结构
echo [2/4] Creating directory structure...
call :CreateDirectories

:: 生成头文件
echo [3/4] Generating header files...
call :GenerateHeaders

:: 生成源文件
echo [4/4] Generating source files...
call :GenerateSources

:: 创建文档
echo.
echo [+] Creating documentation files...
call :CreateDocumentation

echo.
echo ========================================
echo   SUCCESS! Project created at: %ROOT%
echo ========================================

:END
echo.
echo Press any key to exit...
pause >nul
exit /b

:: =======================================
::              函数区
:: =======================================

:CreateDirectories
mkdir inc
mkdir src\core
mkdir src\service
mkdir src\transport\isotp
mkdir src\session
mkdir src\security
mkdir src\did
mkdir src\routine
mkdir src\download
mkdir src\utils
mkdir src\dtc
mkdir port
mkdir example
mkdir doc
goto :eof

:: -----------------------------
:: 自动创建父目录
:: -----------------------------
:EnsureDir
for %%i in ("%~1") do (
    if not exist "%%~dpi" mkdir "%%~dpi" 2>nul
)
goto :eof

:: -----------------------------
:: 字符串转大写
:: -----------------------------
:ToUpper
set "%~2=%~1"
call set "%~2=%%%~2:a=A%%"
call set "%~2=%%%~2:b=B%%"
call set "%~2=%%%~2:c=C%%"
call set "%~2=%%%~2:d=D%%"
call set "%~2=%%%~2:e=E%%"
call set "%~2=%%%~2:f=F%%"
call set "%~2=%%%~2:g=G%%"
call set "%~2=%%%~2:h=H%%"
call set "%~2=%%%~2:i=I%%"
call set "%~2=%%%~2:j=J%%"
call set "%~2=%%%~2:k=K%%"
call set "%~2=%%%~2:l=L%%"
call set "%~2=%%%~2:m=M%%"
call set "%~2=%%%~2:n=N%%"
call set "%~2=%%%~2:o=O%%"
call set "%~2=%%%~2:p=P%%"
call set "%~2=%%%~2:q=Q%%"
call set "%~2=%%%~2:r=R%%"
call set "%~2=%%%~2:s=S%%"
call set "%~2=%%%~2:t=T%%"
call set "%~2=%%%~2:u=U%%"
call set "%~2=%%%~2:v=V%%"
call set "%~2=%%%~2:w=W%%"
call set "%~2=%%%~2:x=X%%"
call set "%~2=%%%~2:y=Y%%"
call set "%~2=%%%~2:z=Z%%"
goto :eof

:ToUpperLoop
if not defined INPUT (
    set "%~2=%OUTPUT%"
    goto :eof
)

set "CHAR=!INPUT:~0,1!"
set "INPUT=!INPUT:~1!"

for %%p in (%UPPER_MAP%) do (
    if "!CHAR!"=="%%~p" (
        set "CHAR=%%~p"
        set "CHAR=!CHAR:~1,1!"
    )
)

set "OUTPUT=!OUTPUT!!CHAR!"
goto ToUpperLoop

:: -----------------------------
:: Header
:: -----------------------------
:CreateHeader
set "FILEPATH=%~1"
set "FILENAME=%~nx1"

call :EnsureDir "%FILEPATH%"

:: 生成宏定义守卫（全大写）
set "GUARD=%FILENAME:.=_%"
set "GUARD=%GUARD:-=_%"
set "GUARD=%GUARD: =_%"
call :ToUpper "%GUARD%" GUARD
set "GUARD=!GUARD!_"

> "%FILEPATH%" (
echo /**
echo  * @file    %FILENAME%
echo  * @author  %AUTHOR%
echo  * @version %VERSION%
echo  * @date    %CURRENT_DATE%
echo  */
echo.
echo #ifndef %GUARD%
echo #define %GUARD%
echo.
echo #ifdef __cplusplus
echo extern "C" ^{
echo #endif
echo.
echo 
/* Includes ------------------------------------------------------------------*/
echo.
echo /* Exported types ------------------------------------------------------------*/
echo.
echo /* Exported constants --------------------------------------------------------*/
echo.
echo /* Exported macros -----------------------------------------------------------*/
echo.
echo /* Exported functions --------------------------------------------------------*/
echo.
echo #ifdef __cplusplus
echo ^}
echo #endif
echo.
echo #endif /* %GUARD% */
)

if not exist "%FILEPATH%" echo [ERROR] Failed to create %FILEPATH%
goto :eof

:: -----------------------------
:: Source
:: -----------------------------
:CreateSource
set "FILEPATH=%~1"
set "FILENAME=%~nx1"
set "BASENAME=%~n1"

call :EnsureDir "%FILEPATH%"

> "%FILEPATH%" (
echo /**
echo  * @file    %FILENAME%
echo  * @author  %AUTHOR%
echo  * @version %VERSION%
echo  * @date    %CURRENT_DATE%
echo  */
echo.
echo 
/* Includes ------------------------------------------------------------------*/
if /i "%~dp1"=="src\service\" (
    echo #include "uds_service.h"
) else (
    echo #include "%BASENAME%.h"
)
echo.
echo /* Private typedef -----------------------------------------------------------*/
echo.
echo /* Private define ------------------------------------------------------------*/
echo.
echo /* Private macro -------------------------------------------------------------*/
echo.
echo /* Private variables ---------------------------------------------------------*/
echo.
echo /* Private function prototypes -----------------------------------------------*/
echo.
echo /* Exported functions --------------------------------------------------------*/
echo.
echo /* Private functions ---------------------------------------------------------*/
)

if not exist "%FILEPATH%" echo [ERROR] Failed to create %FILEPATH%
goto :eof

:: -----------------------------
:: Headers list
:: -----------------------------
:GenerateHeaders
:: 公共接口头文件
call :CreateHeader "inc\MicroUds.h"
call :CreateHeader "inc\MicroUds_types.h"
call :CreateHeader "inc\MicroUds_conf.h"
call :CreateHeader "inc\MicroUds_Service.h"
call :CreateHeader "inc\MicroUds_Did.h"
call :CreateHeader "inc\MicroUds_Dtc.h"

:: Core模块头文件
call :CreateHeader "src\core\uds_core.h"
call :CreateHeader "src\core\uds_dispatch.h"
call :CreateHeader "src\core\uds_state.h"
call :CreateHeader "src\core\uds_context.h"
call :CreateHeader "src\core\uds_nrc.h"

:: 传输层
call :CreateHeader "src\transport\isotp\isotp.h"
call :CreateHeader "src\transport\isotp\isotp_conf.h"

:: 会话管理
call :CreateHeader "src\session\uds_session.h"

:: 安全访问
call :CreateHeader "src\security\uds_security.h"

:: 数据标识符
call :CreateHeader "src\did\uds_did.h"

:: 例程控制
call :CreateHeader "src\routine\uds_routine.h"

:: 下载模块
call :CreateHeader "src\download\uds_download.h"

:: 工具模块
call :CreateHeader "src\utils\uds_timer.h"

:: 故障码
call :CreateHeader "src\dtc\uds_dtc.h"

:: 移植层接口
call :CreateHeader "port\uds_flash_port.h"
call :CreateHeader "port\uds_dtc_port.h"
call :CreateHeader "port\uds_did_port.h"
call :CreateHeader "port\uds_can_port.h"
call :CreateHeader "port\uds_security_port.h"

:: 配置文件
call :CreateHeader "MicroUds_conf.h"
call :CreateHeader "MicroUds_Version.h"
goto :eof

:: -----------------------------
:: Sources list
:: -----------------------------
:GenerateSources
:: 核心模块
call :CreateSource "src\core\uds_core.c"
call :CreateSource "src\core\uds_dispatch.c"
call :CreateSource "src\core\uds_state.c"
call :CreateSource "src\core\uds_context.c"
call :CreateSource "src\core\uds_nrc.c"

:: 服务模块
call :CreateHeader "src\service\uds_service.h"
call :CreateSource "src\service\uds_service.c"
call :CreateSource "src\service\uds_srv_10.c"
call :CreateSource "src\service\uds_srv_11.c"
call :CreateSource "src\service\uds_srv_27.c"
call :CreateSource "src\service\uds_srv_28.c"
call :CreateSource "src\service\uds_srv_34.c"
call :CreateSource "src\service\uds_srv_36.c"
call :CreateSource "src\service\uds_srv_37.c"

:: 传输层
call :CreateSource "src\transport\isotp\isotp.c"

:: 会话管理
call :CreateSource "src\session\uds_session.c"

:: 安全访问
call :CreateSource "src\security\uds_security.c"

:: 数据标识符
call :CreateSource "src\did\uds_did.c"

:: 故障码
call :CreateSource "src\dtc\uds_dtc.c"

:: 例程控制
call :CreateSource "src\routine\uds_routine.c"

:: 下载模块
call :CreateSource "src\download\uds_download.c"

:: 工具模块
call :CreateSource "src\utils\uds_timer.c"

:: 移植层实现
call :CreateSource "port\uds_dtc_port.c"
call :CreateSource "port\uds_did_port.c"
call :CreateSource "port\uds_can_port.c"
call :CreateSource "port\uds_flash_port.c"
call :CreateSource "port\uds_security_port.c"

:: 示例代码
call :CreateSource "example\example_basic.c"
call :CreateSource "example\example_advanced.c"
goto :eof

:: -----------------------------
:: 文档 + gitignore
:: -----------------------------
:CreateDocumentation

> readme.md (
echo # MicroUDS
echo.
echo A lightweight UDS ^(Unified Diagnostic Services^) stack implementation.
echo.
echo ## Features
echo.
echo - ISO 14229-1 compliant
echo - Minimal memory footprint
echo - Easy to port
echo - Modular architecture
echo.
echo ## Directory Structure
echo.
echo ```
echo MicroUDS/
echo ├── inc/                  # Public headers
echo ├── src/
echo │   ├── core/             # Core functionality
echo │   ├── service/          # UDS services ^(0x10, 0x11, 0x27, etc.^)
echo │   ├── transport/isotp/  # ISO-TP layer
echo │   ├── session/          # Session management
echo │   ├── security/         # Security access
echo │   ├── did/              # Data Identifier
echo │   ├── routine/          # Routine control
echo │   ├── download/         # Download/Upload
echo │   ├── dtc/              # Diagnostic Trouble Codes
echo │   └── utils/            # Utilities
echo ├── port/                 # Platform adaptation layer
echo ├── example/              # Example code
echo └── doc/                  # Documentation
echo ```
echo.
echo ## Getting Started
echo.
echo 1. Configure `MicroUds_conf.h`
echo 2. Implement platform-specific functions in `port/`
echo 3. Build and integrate into your project
echo.
echo ## License
echo.
echo See LICENSE file for details.
)

> readme.zh.md (
echo # MicroUDS 中文文档
echo.
echo 轻量级 UDS ^(统一诊断服务^) 协议栈实现
echo.
echo ## 特性
echo.
echo - 符合 ISO 14229-1 标准
echo - 内存占用小
echo - 易于移植
echo - 模块化架构
echo.
echo ## 目录结构
echo.
echo ```
echo MicroUDS/
echo ├── inc/                  # 公共头文件
echo ├── src/
echo │   ├── core/             # 核心功能
echo │   ├── service/          # UDS 服务 ^(0x10, 0x11, 0x27 等^)
echo │   ├── transport/isotp/  # ISO-TP 传输层
echo │   ├── session/          # 会话管理
echo │   ├── security/         # 安全访问
echo │   ├── did/              # 数据标识符
echo │   ├── routine/          # 例程控制
echo │   ├── download/         # 下载/上传
echo │   ├── dtc/              # 故障码
echo │   └── utils/            # 工具函数
echo ├── port/                 # 平台适配层
echo ├── example/              # 示例代码
echo └── doc/                  # 文档
echo ```
echo.
echo ## 快速开始
echo.
echo 1. 配置 `MicroUds_conf.h`
echo 2. 在 `port/` 目录实现平台相关函数
echo 3. 编译并集成到您的项目
echo.
echo ## 许可证
echo.
echo 详见 LICENSE 文件
)

> .gitignore (
echo # Build artifacts
echo *.o
echo *.obj
echo *.exe
echo *.out
echo *.a
echo *.lib
echo *.elf
echo *.bin
echo *.hex
echo.
echo # IDE files
echo .vscode/
echo .vs/
echo *.suo
echo *.user
echo *.sln.docstates
echo.
echo # OS files
echo .DS_Store
echo Thumbs.db
echo desktop.ini
)

goto :eof
