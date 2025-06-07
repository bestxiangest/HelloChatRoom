@echo off
echo 正在编译HelloChatRoom项目...

REM 设置Qt环境变量（根据实际安装路径调整）
set QT_DIR=C:\Qt\6.5.3\mingw_64
set PATH=%QT_DIR%\bin;%PATH%
set QTDIR=%QT_DIR%

REM 清理之前的构建文件
if exist Makefile del Makefile
if exist *.o del *.o
if exist moc_*.cpp del moc_*.cpp
if exist ui_*.h del ui_*.h
if exist HelloChatRoom.exe del HelloChatRoom.exe

REM 生成Makefile
echo 生成Makefile...
qmake HelloChatRoom.pro
if %ERRORLEVEL% neq 0 (
    echo qmake失败，尝试使用mingw32-make...
    goto :manual_compile
)

REM 编译项目
echo 编译项目...
mingw32-make
if %ERRORLEVEL% neq 0 (
    echo 编译失败！
    goto :manual_compile
)

echo 编译成功！
goto :end

:manual_compile
echo 尝试手动编译...
REM 手动编译（如果qmake不可用）
g++ -std=c++17 -I. -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB ^^
    main.cpp ChatClient.cpp FileTransferManager.cpp loginwindow.cpp mainwindow.cpp privatechat.cpp ^^
    -lQt6Widgets -lQt6Gui -lQt6Network -lQt6Core ^^
    -o HelloChatRoom.exe

if %ERRORLEVEL% neq 0 (
    echo 手动编译也失败了！请检查Qt环境配置。
    pause
    exit /b 1
)

echo 手动编译成功！

:end
echo 构建完成！
if exist HelloChatRoom.exe (
    echo 可执行文件：HelloChatRoom.exe
) else (
    echo 警告：未找到可执行文件！
)
pause