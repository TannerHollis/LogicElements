@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set ROOT=C:\Users\tanne\OneDrive\Documents\GitHub\LogicElements
if not exist "%ROOT%\build_element_assembly" mkdir "%ROOT%\build_element_assembly"
cd /d "%ROOT%"
cl /nologo /W3 /Od /std:c11 /DLE_COMPILER_STATIC /EHsc /D_CRT_SECURE_NO_WARNINGS /Isrc\compiler\include /Isrc\runtime\include tests\test_element_assembly.c src\runtime\src\le_rt.c src\runtime\src\le_vm.c src\runtime\src\le_opcodes.c src\runtime\src\le_loader.c src\runtime\src\le_process_image.c src\runtime\src\le_storage.c src\hal\le_hal_sim.c src\compiler\src\le_compiler_core.cpp src\compiler\src\le_compiler_c_api.cpp src\compiler\src\le_optimizer.cpp src\compiler\src\le_disasm_core.cpp /Fobuild_element_assembly\ /Febuild_element_assembly\test_element_assembly.exe /link /SUBSYSTEM:CONSOLE