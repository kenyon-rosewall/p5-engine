@echo off

set CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST x64\Debug mkdir x64\Debug
pushd x64\Debug

del *.pdb > NUL 2> NUL

REM Asset file builder build
cl %CommonCompilerFlags% -I..\..\P5Engine -D_CRT_SECURE_NO_WARNINGS ..\..\AssetBuilder\asset_builder.cpp /link %CommonLinkerFlags%

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
REM Optimization switches /wO2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -O2 -I..\..\lib\iaca-win64\ -c ..\..\P5Engine\p5engine_optimized.cpp -Fop5engine_optimized.obj -LD
cl %CommonCompilerFlags% -I..\..\P5Engine -I..\..\lib\iaca-win64\ ..\..\P5Engine\p5engine.cpp p5engine_optimized.obj -Fmp5engine.map -LD /link -incremental:no -opt:ref -PDB:p5engine_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
del lock.tmp
cl %CommonCompilerFlags% ..\..\Win32P5Engine\Win32P5Engine.cpp -Fmwin32_p5engine.map /link %CommonLinkerFlags%
popd