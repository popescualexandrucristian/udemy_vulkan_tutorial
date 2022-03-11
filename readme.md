# Learning Vulkan

## Summary

The 2022 Autodesk Recharge Sprint gave me the opportunity to go trough an introductory course for Vulkan([link]((https://www.udemy.com/course/learn-the-vulkan-api-with-cpp))).
I followed the lesson plan and I implemented everything the instructor explained and a little extra.

Thanks Autodesk !

## What I learned

* Create a Vulkan instance, get a physical device, create a logical device, get the physical queues, get the logical queues, setup the validation layers on debug, get the surface, get the swap-chain
* Create the render passes with sub-passes and dependencies
* Create the resource descriptor layouts and the graphics pipeline, texture samplers
* Crete the depth buffer, color frame buffer targets, load texture data, vertex data, index data, uniform buffers, dynamic uniform buffers and push constants on the device
* Create command pools and command buffers
* Crete and use Vulkan semaphores, fences and barriers for device-device and device-host synchronization
* Record command buffers so data can be generated in the frame-buffers
* Present the generated data to the screen
* Resize the swap-chain and the screen at runtime
* Load custom model data with the ASSIMP library
* Use multiple sub-passes

## Repo usage instructions

* Make sure your video card supports at least Vulkan 1.1(this is what the code was written against)
* I only provide solutions for visual studio 2019 on windows, but the code is cross-platform(not tested)
* Install the Vulkan SDK (I worked with version 1.3.204.0 but a newer version should also work as expected)
* Download this repository from https://github.com/popescualexandrucristian/udemy_vulkan_tutorial.git
* Open the solution(vulkan_course_app.sln) compile and run it (the resources project should compile the shaders and copy the resources to the output folder and the vulkan_course_app should compile the executable and copy the necessary dlls)

### Notes
* Only x64 versions of the project are available, but the code should compile on x86
* Make sure the SDK added an environment variable for it's location VULKAN_SDK, this variable is used to find the library and to compile the shaders
* If for some reason you need to compile the shaders outside of visual studio they are all located in /shaders and they respect the standard naming conventions, the results should be placed in the same directory as the executable
* If visual studio is not used all the files from the resources folder should be placed in the same directory as the executable and the following dlls should be also added there /external/glfw/glfw3.dll, external/assimp/assimp-vc140-mt.dll
* There is an open source extension for visual studio https://github.com/danielscherzer/GLSL also offered directly from the Microsoft Visual Studio extension marketplace https://marketplace.visualstudio.com/items?itemName=DanielScherzer.GLSL it offers full support for GLSL on Vulkan, the user just has to add "-V" for the "Arguments for the external compiler executable" and an absolute path for glslangValidator in the "External compiler executable file path (without quotes)", otherwise it will not work with the Vulkan specific stuff
* There is a great free and open source graphics application debugger called RenderDoc(https://github.com/baldurk/renderdoc) it looks and feels like Pix from Direct3D 9, but it can work with Vulkan, OpenGl, OpenCL and Direct3D, it saved me a lot of time and I fully recommend it
* For a complete list of the external libs that I use see the "External libs" section, all of them are cross-platform, but for GLFW and ASSIM I only provide the windows versions in this repository, they are easy to recompile on any platform

## Course
[https://www.udemy.com/course/learn-the-vulkan-api-with-cpp](https://www.udemy.com/course/learn-the-vulkan-api-with-cpp)

## Progress

### Mar 10, 2022 Add support for swap-chain resize/screen resize
![e26985e5dec60b6aa59a79c19f93083d0e774330](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/01dd01b26a3634e5ffc64a7cea3d346d55adbf47.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/e26985e5dec60b6aa59a79c19f93083d0e774330](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/e26985e5dec60b6aa59a79c19f93083d0e774330)

### Mar 10, 2022 Add a second sub-pass, the first pass stores the color and the depth and the second pass will display on half of the screen the color pass and on half the depth pass
![9ac2d3a90fd7f7a7cc3352965246224eee05e31b](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/9ac2d3a90fd7f7a7cc3352965246224eee05e31b.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/9ac2d3a90fd7f7a7cc3352965246224eee05e31b](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/9ac2d3a90fd7f7a7cc3352965246224eee05e31b)

### Mar 9, 2022 Add model loading support
![a4d95cee27e5b7e72053c393d3a720f3e2266bc5](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/a4d95cee27e5b7e72053c393d3a720f3e2266bc5.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/a4d95cee27e5b7e72053c393d3a720f3e2266bc5](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/a4d95cee27e5b7e72053c393d3a720f3e2266bc5)

### Mar 8, 2022 Add textures support
![ea09110a269aa9a79519e7061a1b552af3d888d5](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/ea09110a269aa9a79519e7061a1b552af3d888d5.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/ea09110a269aa9a79519e7061a1b552af3d888d5](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/ea09110a269aa9a79519e7061a1b552af3d888d5)

### Mar 7, 2022 Draw multiple objects with dynamic uniform buffers and push constants
##Mar 4, 2022 Draw multiple objects with uniform buffers
![3dd27dfee5eae0e5646e4c3de878f2338ce815ff](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/3dd27dfee5eae0e5646e4c3de878f2338ce815ff.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/3dd27dfee5eae0e5646e4c3de878f2338ce815ff](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/3dd27dfee5eae0e5646e4c3de878f2338ce815ff)

### Mar 3, 2022 Draw with an index buffer and a vertex buffer
### Mar 4, 2022 Use uniform buffers
![b99650ccdf7f6429e5b546b7027fb8fa934cd3a0](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/b99650ccdf7f6429e5b546b7027fb8fa934cd3a0.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/b99650ccdf7f6429e5b546b7027fb8fa934cd3a0](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/b99650ccdf7f6429e5b546b7027fb8fa934cd3a0)

### Mar 2, 2022 Draw the first triangle with hard-coded data in the shaders(it's beautiful)
![b857c3117b8e4481c6adf380c9324ee05f24ec40](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/blob/master/documentation/b857c3117b8e4481c6adf380c9324ee05f24ec40.jpg?raw=true)
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/b857c3117b8e4481c6adf380c9324ee05f24ec40](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/b857c3117b8e4481c6adf380c9324ee05f24ec40)

### Mar 1, 2022 Create the logical device, the swap chains and the render pass
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/35d4b749ba292edc834d2b6936940adbd8b7d9ac](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/35d4b749ba292edc834d2b6936940adbd8b7d9ac)

### Feb 28, 2022 Set-up project learn the theory on the structure of a Vulkan application and setup the project
[https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/ecef744503e8d8a12c3b13bd93eb80cf1989b5d6](https://github.com/popescualexandrucristian/udemy_vulkan_tutorial/commit/ecef744503e8d8a12c3b13bd93eb80cf1989b5d6)

## External libs
[STB image](https://github.com/nothings/stb)  
[GLM](https://github.com/g-truc/glm)  
[GLFW](https://github.com/glfw/glfw)  
[ASSIM](https://github.com/assimp/assimp)  

## External resources
[Cat model](https://free3d.com/3d-model/cat-v1--522281.html)  
Test texture - default color grid from Blander  
[Panda texture](https://www.istockphoto.com/ro/fotografie/t%C3%A2n%C4%83rul-urs-panda-%C3%AEn-copac-gm1060491624-283471940)  

