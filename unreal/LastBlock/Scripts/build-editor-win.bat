@echo off
rem Compiles the C++ module for the Windows editor, then open LastBlock.uproject.
if "%UE_ROOT%"=="" set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" LastBlockEditor Win64 Development -Project="%~dp0..\LastBlock.uproject" -WaitMutex
