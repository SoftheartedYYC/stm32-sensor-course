@echo off
chcp 65001 >nul
:: ============================================================
::  keilkill.bat  --  Keil MDK 工程中间文件清理脚本
::  编码：UTF-8（无 BOM） 换行：CRLF
::  用法：双击运行，或在任意目录从命令行执行
::  作用：删除 Keil 编译产生的中间文件，保留全部源码与工程配置
:: ============================================================

:: 记录当前代码页，退出前还原（避免影响调用方 cmd 窗口）
for /f "tokens=2 delims=:" %%C in ('chcp') do set "OLDCP=%%C"

:: 切到脚本所在目录（工程根目录），避免在别处运行时误清别的工程
cd /d "%~dp0"

echo [1/3] 清理编译中间文件 ...
:: 通用中间文件：目标文件、依赖文件、列表文件、调试信息等
for %%E in (bak ddk edk lst lnp mpf mpj obj omf plg rpt tmp __i crf o d axf tra dep iex htm sct map dbgconf gpdsc scvd) do (
    del /f /q /s "*.%%E" >nul 2>nul
)

echo [2/3] 清理调试日志 ...
del /f /q /s "JLinkLog.txt" >nul 2>nul
del /f /q /s "JLinkSettings.ini" >nul 2>nul
del /f /q /s "*.log" >nul 2>nul

echo [3/3] 清理按用户名生成的界面布局文件 ...
:: 形如 Framework.uvguix.12474，换台电脑就没用了
del /f /q /s "*.uvguix.*" >nul 2>nul

:: -------- 以下文件一律保留，禁止删除 --------
::  OBJ\Framework.hex  编译产物，保留下来当烧录固件用
::  *.uvprojx          工程文件本体
::  *.uvoptx           调试器/断点/J-Link 设置
::  *.opt             J-LINK 设置
::  *.bin             （本工程未生成，留个位置）
:: 真有一天想连固件一起清掉，把下面两行的 :: 去掉即可
:: del /f /q /s "*.hex" >nul 2>nul
:: del /f /q /s "*.bin" >nul 2>nul

echo.
echo [完成] 清理结束，源码与工程配置均已保留
echo.

:: 还原代码页
if defined OLDCP chcp %OLDCP% >nul

:: 双击运行时自动暂停以便查看结果；在命令行里运行时直接退出
echo %cmdcmdline% | find /i "%~nx0" >nul && pause
