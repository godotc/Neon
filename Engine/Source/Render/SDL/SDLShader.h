#pragma once

#include "Render/Render.h"
#include "Render/Shader.h"

namespace SDL
{

struct SDLShaderProcessor
{

    SDL_GPUDevice                                                         &device;
    SDL_GPUShader                                                         *vertexShader   = nullptr;
    SDL_GPUShader                                                         *fragmentShader = nullptr;
    std::unordered_map<EShaderStage::T, ShaderReflection::ShaderResources> shaderResources;
    SDL_GPUShaderCreateInfo                                                vertexCreateInfo;
    SDL_GPUShaderCreateInfo                                                fragmentCreateInfo;


    ShaderScriptProcessor::stage2spirv_t shaderCodes; // store the codes

    SDLShaderProcessor(SDL_GPUDevice &device) : device(device) {}

    SDLShaderProcessor &preprocess(const ShaderCreateInfo &shaderCI)
    {

        ShaderScriptProcessorFactory factory;
        factory.withProcessorType(ShaderScriptProcessorFactory::EProcessorType::GLSL)
            .withShaderStoragePath("Engine/Shader/GLSL")
            .withCachedStoragePath("Engine/Intermediate/Shader/GLSL");

        std::shared_ptr<ShaderScriptProcessor> processor = factory.FactoryNew();

        auto ret = processor->process(shaderCI.shaderName);
        if (!ret) {
            NE_CORE_ERROR("Failed to process shader: {}", processor->tempProcessingPath);
            NE_CORE_ASSERT(false, "Failed to process shader: {}", processor->tempProcessingPath);
        }
        // store the temp codes
        shaderCodes = std::move(ret.value());

        // Process each shader stage and store both SPIRV-Cross resources and our custom reflection data
        for (const auto &[stage, code] : shaderCodes) {
            // Get shader resources from SPIR-V reflection using our new custom reflection
            ShaderReflection::ShaderResources reflectedResources = processor->reflect(stage, code);

            // Also store the original SPIRV-Cross resources for backward compatibility
            shaderResources[stage] = reflectedResources;
        }

        // Create shader create info for vertex shader
        vertexCreateInfo = {
            .code_size            = shaderCodes[EShaderStage::Vertex].size() * sizeof(uint32_t) / sizeof(uint8_t),
            .code                 = (uint8_t *)shaderCodes[EShaderStage::Vertex].data(),
            .entrypoint           = "main",
            .format               = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage                = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers         = (Uint32)shaderResources[EShaderStage::Vertex].sampledImages.size(),
            .num_storage_textures = 0, // We're not using storage images currently
            .num_storage_buffers  = 0, // We're not using storage buffers currently
            .num_uniform_buffers  = (Uint32)shaderResources[EShaderStage::Vertex].uniformBuffers.size(),
            .props                = 0,

        };

        // Create shader create info for fragment shader
        fragmentCreateInfo = {
            .code_size            = shaderCodes[EShaderStage::Fragment].size() * sizeof(uint32_t) / sizeof(uint8_t),
            .code                 = (uint8_t *)shaderCodes[EShaderStage::Fragment].data(),
            .entrypoint           = "main",
            .format               = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage                = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers         = (Uint32)shaderResources[EShaderStage::Fragment].sampledImages.size(),
            .num_storage_textures = 0, // We're not using storage images currently
            .num_storage_buffers  = 0, // We're not using storage buffers currently
            .num_uniform_buffers  = [&]() -> Uint32 {
                const auto fragmentUniformCount = shaderResources[EShaderStage::Fragment].uniformBuffers.size();
                const auto samplerCount         = shaderResources[EShaderStage::Fragment].sampledImages.size();
                if (samplerCount + fragmentUniformCount > 99999) {
                    NE_CORE_ERROR("Combined uniform buffer count exceeds the maximum allowed: Vertex={}, Fragment={}", samplerCount, fragmentUniformCount);
                    NE_CORE_ASSERT(false, "Uniform buffer count mismatch");
                }

                // The sampler count is added to the fragment uniform count because both contribute to the total uniform buffer usage.
                NE_CORE_DEBUG("Fragment shader uniform count: {}, sampler count: {}", fragmentUniformCount, samplerCount);
                return static_cast<Uint32>(samplerCount + fragmentUniformCount);
            }(),
        };

        return *this;
    }


    SDLShaderProcessor &create()
    {
        bool ok      = true;
        vertexShader = SDL_CreateGPUShader(&device, &vertexCreateInfo);
        if (!vertexShader) {
            NE_CORE_ERROR("Failed to create vertex shader");
            ok = false;
        }
        fragmentShader = SDL_CreateGPUShader(&device, &fragmentCreateInfo);
        if (!fragmentShader) {
            NE_CORE_ERROR("Failed to create fragment shader");
            ok = false;
        }

        if (!ok) {
            clean();
        }
        return *this;
    }

    void clean()
    {
        if (vertexShader) {
            SDL_ReleaseGPUShader(&device, vertexShader);
        }
        if (fragmentShader) {
            SDL_ReleaseGPUShader(&device, fragmentShader);
        }
    }
};

} // namespace SDL