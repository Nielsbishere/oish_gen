# oish_gen
The compiler from SPIR-V to oiSH format. Converts it through SPIRV-CROSS shader reflection.
## Usage
First, you have to generate your shader code through the Khronos SPIR-V compiler for Vulkan. Then, you need to run the script with the following parameters:  
`oish_gen.exe "%FULL_PATH_TO_SHADER_BASE%" "%SHADER_NAME%" %SHADER_EXTENSIONS%`  
This requires you to use the same names for a path, except you distinguish them by .vert, .geom, .frag and .comp extensions. An example would be the following:  
`oish_gen.exe "D:\programming\repos\ocore\app\res\shaders\simple" "simple" .vert .frag`  
Which would require the files "simple.vert.spv" and "simple.frag.spv" to be available.
## Script in Osomi Graphics Core
This is used in a script in Osomi Graphics Core to ensure that all shaders will be compiled into .oiSH format:
```bat
@echo off
call :treeProcess
pause
goto :eof

rem Call oish_gen.exe on every shader that is found
rem Will generate an .oiSH file from it
:perShader
echo Found a shader named %~1 with %~2 stages and id %~3
set /a end=%~2-1
set "args="%~dp0oish_gen.exe" "%~4" "%~1" !stages[0]!"
for /l %%x in (1, 1, %end%) do set args=!args! !stages[%%x]!
cmd /c %args%
exit /b

:treeProcess

setlocal enabledelayedexpansion

set /a i=0
set /a j=-1

for %%f in (*.vert *.frag *.comp *.geom) do (

	rem Compile shader source into spir-v code
	"%VULKAN_SDK%/Bin/glslangValidator.exe" -V -e main "%%~ff" -o "%%~nf%%~xf.spv"
	
	rem Optimize shader source code and strip all reflection data from source
	rem "%VULKAN_SDK%/Bin/spirv-remap.exe" --do-everything -i "%%~nf%%~xf.spv" -o ../shaders/
	rem "%VULKAN_SDK%/Bin/spirv-opt.exe" -Os -O "%%~nf%%~xf.spv" -o "%%~nf%%~xf.spv"
	
	if NOT !name!==%%~nf (
		if defined !name! (
			call :perShader !name! !i! !j! !fullname!
		)
		
		set stages[0]=%%~xf
		set /a i=1
		set /a j=!j!+1
	)
	
	if !name!==%%~nf (
		set stages[!i!]=%%~xf
		set /a i=!i!+1
	)
	
	
	set name=%%~nf
	set fullname=%%~df%%~pf%%~nf
)

if defined name (
	call :perShader %name% %i% %j% %fullname%
)

endlocal

rem go through other directories inside this directory
for /D %%d in (*) do (
    cd %%d
    call :treeProcess
    cd ..
)```
This traverses all folders in the ocore root and finds all shader stages, which get recombined into a .oiSH file.
