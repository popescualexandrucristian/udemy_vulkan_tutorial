@echo off
for /f %%f in ('dir /b shaders') do %VULKAN_SDK%\Bin\glslangValidator.exe -V100 shaders\%%f -o "%1\%%f.spv"