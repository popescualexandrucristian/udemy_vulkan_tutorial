@echo off
for /f %%f in ('dir /b shaders') do %VULKAN_SDK%\Bin\glslangValidator.exe -g -V100 shaders\%%f -o "%1\%%f.spv"