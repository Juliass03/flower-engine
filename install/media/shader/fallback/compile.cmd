glslc.exe src/fullscreen.vert -o bin/tonemapper.vert.spv
glslc.exe src/tonemapper.frag -o bin/tonemapper.frag.spv

glslc.exe src/fullscreen.vert -o bin/lighting.vert.spv
glslc.exe src/lighting.frag -o bin/lighting.frag.spv

glslc.exe src/depth.vert -o bin/depth.vert.spv
glslc.exe src/depth.frag -o bin/depth.frag.spv

glslc.exe src/gbuffer.vert -o bin/gbuffer.vert.spv
glslc.exe src/gbuffer.frag -o bin/gbuffer.frag.spv