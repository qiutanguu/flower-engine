glslc.exe src/gpuculling.comp -o bin/gpuculling.comp.spv
glslc.exe src/evaluateDepthMinMax.comp -o bin/evaluateDepthMinMax.comp.spv
glslc.exe src/cascadeShadowSetup.comp -o bin/cascadeShadowSetup.comp.spv
glslc.exe src/brdflut.comp -o bin/brdflut.comp.spv
glslc.exe src/irradiancecube.comp -o bin/irradiancecube.comp.spv
glslc.exe src/hdri2cubemap.comp -o bin/hdri2cubemap.comp.spv
glslc.exe src/prefilterspecularcube.comp -o bin/prefilterspecularcube.comp.spv
glslc.exe src/taa.comp -o bin/taa.comp.spv
glslc.exe src/taa_sharpen.comp -o bin/taa_sharpen.comp.spv
glslc.exe src/downsample.comp -o bin/downsample.comp.spv
glslc.exe src/blur.frag -o bin/blur.frag.spv
glslc.exe src/blend.frag -o bin/blend.frag.spv

glslc.exe src/fullscreen.vert -o bin/fullscreen.vert.spv