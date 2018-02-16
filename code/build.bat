@echo off

REM Compiler Warning Options : https://msdn.microsoft.com/en-us/library/thxezb7y.aspx
REM Compiler Switches : https://msdn.microsoft.com/en-us/library/fwkeyyhe.aspx

REM TODO: can we just build both(32bit and 64 bit) with one exe?

REM WARNINGS
REM -WX : Treat warning as errors
REM -W4 : Warning level 4
REM c4201: no name for the struct
REM c4100: unreferenced formal parameter
REM c4189: local variable initialized but not used
REM c4505: function not used

REM Compiler Switchs
REM Zi(Z7): produce debug information(.pdb) - things like where does this part in exe correnspond to the code...
REM Od : debug mode 
REM 0i: if the compiler knows the intrinsic version, do it(in assembly code), and don't use the c++ runtime library.  ex) sinf
REM -GR-: Turn off c++ runtime typo.
REM -Eha: Turn off c++ exception handler(it creates extra things in stack)

REM -MD: Use the internal(hidden) C runtime DLL instead of packing into exe. DONT USE THIS!!!
REM -MT: Use the static library and pack the c runtime library to the exe so that
REM it will run no matter what OS the user is using beacuse there are so many versions of c runtime DLL
REM and certain OS might not have the right one.
REM -Gm-: minimal rebuilding when we rebuild the code.
REM cl: command line
REM -Fm: Create map file. Map file shows the whole process.
REM -opt:ref: Hey linker don't put something into the exe if noone is using it.

set commonCompilerFlags= -MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -DFOX_WIN32=1 -DFOX_SLOW=1 -DFOX_DEBUG=1 -FC -Z7
set commonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
REM 32bit build
REM cl %commonCompilerFlags% ..\fox\code\win32_fox.cpp /link -subsystem:windows,5.1  %commonLinkerFlags% 
del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\fox\code\fox.cpp -Fmfox.map -LD /link -incremental:no -opt:ref -PDB:fox_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
del lock.tmp
cl %CommonCompilerFlags% ..\fox\code\win32_fox.cpp -Fmwin32_fox.map /link %CommonLinkerFlags%
popd
