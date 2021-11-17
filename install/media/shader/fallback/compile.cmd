glslc.exe src/gpuculling.comp -o bin/gpuculling.comp.spv
glslc.exe src/evaluateDepthMinMax.comp -o bin/evaluateDepthMinMax.comp.spv
glslc.exe src/cascadeShadowSetup.comp -o bin/cascadeShadowSetup.comp.spv
glslc.exe src/brdflut.comp -o bin/brdflut.comp.spv
glslc.exe src/irradiancecube.comp -o bin/irradiancecube.comp.spv
glslc.exe src/hdri2cubemap.comp -o bin/hdri2cubemap.comp.spv
glslc.exe src/prefilterspecularcube.comp -o bin/prefilterspecularcube.comp.spv