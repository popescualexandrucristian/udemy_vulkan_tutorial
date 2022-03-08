@echo off
%VULKAN_SDK%\Bin\glslangValidator.exe -V100 %2 -o "%1\%~n2%~x2.spv"