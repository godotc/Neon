#include "SDLGPURender.h"



#include "SDL3/SDL_gpu.h"

#include "Render/Shader.h"
#include "SDLGPUCommandBuffer.h"

// shaders is high related with pipeline, we split it temporarily
// TODO: export shader info -> use reflection do this
std::optional<std::tuple<SDL_GPUShader *, SDL_GPUShader *>> GPURender_SDL::createShaders(const ShaderCreateInfo &shaderCI)
{
    SDL_GPUShader *vertexShader   = nullptr;
    SDL_GPUShader *fragmentShader = nullptr;
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
        ShaderScriptProcessor::stage2spirv_t &codes = ret.value();


        std::unordered_map<EShaderStage::T, ShaderReflection::ShaderResources> shaderResources;
        for (const auto &[stage, code] : codes) {
            ShaderReflection::ShaderResources res = processor->reflect(stage, code);
            shaderResources[stage]                = std::move(res);
        }


        SDL_GPUShaderCreateInfo vertexCrateInfo = {
            .code_size            = codes[EShaderStage::Vertex].size() * sizeof(uint32_t) / sizeof(uint8_t),
            .code                 = (uint8_t *)codes[EShaderStage::Vertex].data(),
            .entrypoint           = "main",
            .format               = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage                = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers         = (Uint32)shaderResources[EShaderStage::Vertex].sampledImages.size(),
            .num_storage_textures = 0, //(Uint32)shaderResources[EShaderStage::Vertex].storageImages.size(),
            .num_storage_buffers  = 0, //(Uint32)shaderResources[EShaderStage::Vertex].storageBuffers.size(),
            .num_uniform_buffers  = (Uint32)shaderResources[EShaderStage::Vertex].uniformBuffers.size(),
        };
        SDL_GPUShaderCreateInfo fragmentCreateInfo = {
            .code_size            = codes[EShaderStage::Fragment].size() * sizeof(uint32_t) / sizeof(uint8_t),
            .code                 = (uint8_t *)codes[EShaderStage::Fragment].data(),
            .entrypoint           = "main",
            .format               = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage                = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers         = (Uint32)shaderResources[EShaderStage::Fragment].sampledImages.size(),
            .num_storage_textures = 0, //(Uint32)shaderResources[EShaderStage::Fragment].storageImages.size(),
            .num_storage_buffers  = 0, //(Uint32)shaderResources[EShaderStage::Fragment].storageBuffers.size(),
            .num_uniform_buffers  = (Uint32)shaderResources[EShaderStage::Fragment].uniformBuffers.size(),
        };

        vertexShader = SDL_CreateGPUShader(device, &vertexCrateInfo);
        if (!vertexShader) {
            NE_CORE_ERROR("Failed to create vertex shader");
            return {};
        }
        fragmentShader = SDL_CreateGPUShader(device, &fragmentCreateInfo);
        if (!fragmentShader) {
            NE_CORE_ERROR("Failed to create fragment shader");
            return {};
        }
    }


    return {{vertexShader, fragmentShader}};
}

std::shared_ptr<CommandBuffer> GPURender_SDL::acquireCommandBuffer(std::source_location location)
{

    // return std::shared_ptr<CommandBuffer>(new GPUCommandBuffer_SDL(this, std::move(location)));
    return std::make_shared<GPUCommandBuffer_SDL>(this, std::move(location));
    // return nullptr;
}


bool GPURender_SDL::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    int n = SDL_GetNumGPUDrivers();
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%d Available drivers: ", n);
    for (int i = 0; i < n; ++i) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetGPUDriver(i));
    }

    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                     SDL_GPU_SHADERFORMAT_DXIL |
                                     SDL_GPU_SHADERFORMAT_MSL,
                                 true,
                                 nullptr);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to create GPU device: %s", SDL_GetError());
        return false;
    }

    const char *driver = SDL_GetGPUDeviceDriver(device);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Chosen GPU Driver: %s", driver);

    window = SDL_CreateWindow("Neon", 1024, 768, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to create window: %s", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to claim window: %s", SDL_GetError());
        return false;
    }

    createSamplers();
    return true;
}

void GPURender_SDL::clean()
{
    for (auto &[key, sampler] : samplers) {
        if (!sampler) {
            continue;
        }
        SDL_ReleaseGPUSampler(device, sampler);
    }

    if (vertexBuffer) {
        SDL_ReleaseGPUBuffer(device, vertexBuffer);
    }
    if (indexBuffer) {
        SDL_ReleaseGPUBuffer(device, indexBuffer);
    }
    if (pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    }

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
}

// a Pipeline: 1 vertex shader + 1 fragment shader + 1 render pass + 1 vertex buffer + 1 index buffer
// their format should be compatible with each other
// so we put them together with initialization
bool GPURender_SDL::createGraphicsPipeline(const GraphicsPipelineCreateInfo &info)
{

    // SHADER is high related with pipeline!!!
    auto shaders = createShaders(info.shaderCreateInfo);
    NE_ASSERT(shaders.has_value(), "Failed to create shader {}", SDL_GetError());
    auto &[vertexShader, fragmentShader] = shaders.value();

    std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescs;
    std::vector<SDL_GPUVertexAttribute>         vertexAttributes;
    {

        for (int i = 0; i < info.vertexBufferDescs.size(); ++i) {
            vertexBufferDescs.push_back(SDL_GPUVertexBufferDescription{
                .slot               = info.vertexBufferDescs[i].slot,
                .pitch              = info.vertexBufferDescs[i].pitch,
                .input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .instance_step_rate = 0,
            });
        }
        for (int i = 0; i < info.vertexAttributes.size(); ++i) {
            SDL_GPUVertexAttribute sdlVertAttr{
                .location    = info.vertexAttributes[i].location,
                .buffer_slot = info.vertexAttributes[i].bufferSlot,
                .format      = SDL_GPU_VERTEXELEMENTFORMAT_INVALID,
                .offset      = info.vertexAttributes[i].offset,
            };

            switch (info.vertexAttributes[i].format) {
            case EVertexAttributeFormat::Float2:
                sdlVertAttr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
                break;
            case EVertexAttributeFormat::Float3:
                sdlVertAttr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                break;
            case EVertexAttributeFormat::Float4:
                sdlVertAttr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                break;
            default:
                NE_CORE_ASSERT(false, "Invalid vertex attribute format {}", int(info.vertexAttributes[i].format));
                break;
            }
            vertexAttributes.emplace_back(std::move(sdlVertAttr));
        }

        const auto &last = info.vertexAttributes.end() - 1;
        vertexInputSize  = last->offset + EVertexAttributeFormat::T2Size(last->format);
    }


    // this format is the final screen surface's format
    // if you want other format, create texture yourself
    SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(device, window);
    if (format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        NE_CORE_ERROR("Failed to get swapchain texture format: {}", SDL_GetError());
        return false;
    }
    NE_CORE_INFO("current gpu texture format: {}", (int)format);

    SDL_GPUColorTargetDescription colorTargetDesc{
        .format = format,
        // final_color = (src_color × src_color_blendfactor) color_blend_op (dst_color × dst_color_blendfactor)
        // final_alpha = (src_alpha × src_alpha_blendfactor) alpha_blend_op (dst_alpha × dst_alpha_blendfactor)
        .blend_state = SDL_GPUColorTargetBlendState{
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op        = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op        = SDL_GPU_BLENDOP_ADD,
            .color_write_mask      = SDL_GPU_COLORCOMPONENT_A | SDL_GPU_COLORCOMPONENT_B |
                                SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_R,
            .enable_blend            = true,
            .enable_color_write_mask = false,
        },
    };
    SDL_GPUGraphicsPipelineCreateInfo sdlGPUCreateInfo = {
        .vertex_shader      = vertexShader,
        .fragment_shader    = fragmentShader,
        .vertex_input_state = SDL_GPUVertexInputState{
            .vertex_buffer_descriptions = vertexBufferDescs.data(),
            .num_vertex_buffers         = static_cast<Uint32>(vertexBufferDescs.size()),
            .vertex_attributes          = vertexAttributes.data(),
            .num_vertex_attributes      = static_cast<Uint32>(vertexAttributes.size()),
        },
        .rasterizer_state = SDL_GPURasterizerState{
            .fill_mode  = SDL_GPU_FILLMODE_FILL,
            .cull_mode  = SDL_GPU_CULLMODE_BACK, // cull back/front face
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },

        .multisample_state = SDL_GPUMultisampleState{
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .enable_mask  = false,
        },
        .depth_stencil_state = SDL_GPUDepthStencilState{
            .compare_op = SDL_GPU_COMPAREOP_GREATER, // -z forward
            // .back_stencil_state = SDL_GPUStencilOpState{
            //     .fail_op       = SDL_GPU_STENCILOP_ZERO,
            //     .pass_op       = SDL_GPU_STENCILOP_KEEP,
            //     .depth_fail_op = SDL_GPU_STENCILOP_ZERO,
            //     .compare_op    = SDL_GPU_COMPAREOP_NEVER,
            // },
            // .front_stencil_state = SDL_GPUStencilOpState{
            //     .fail_op       = SDL_GPU_STENCILOP_KEEP,
            //     .pass_op       = SDL_GPU_STENCILOP_KEEP,
            //     .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            //     .compare_op    = SDL_GPU_COMPAREOP_LESS,
            // },
            // .compare_mask        = 0xFF,
            // .write_mask          = 0xFF,
            .enable_depth_test   = true,
            .enable_depth_write  = true,
            .enable_stencil_test = false,
        },
        .target_info = SDL_GPUGraphicsPipelineTargetInfo{
            .color_target_descriptions = &colorTargetDesc,
            .num_color_targets         = 1,
            .depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
            .has_depth_stencil_target  = false,
        },

    };
    switch (info.primitiveType) {
    case EGraphicPipeLinePrimitiveType::TriangleList:
        // WTF? it should be SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, so ambiguous
        // sdlGPUCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
        sdlGPUCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        break;
    default:
        NE_CORE_ASSERT(false, "Invalid primitive type {}", int(info.primitiveType));
        break;
    }

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &sdlGPUCreateInfo);
    if (!pipeline) {
        NE_CORE_ERROR("Failed to create graphics pipeline: {}", SDL_GetError());
        return false;
    }

    // create global big size buffer for batch draw call
    {
        {
            SDL_GPUBufferCreateInfo bufferInfo = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                .size  = getVertexBufferSize(),
                .props = 0, // by comment
            };

            vertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);
            NE_ASSERT(vertexBuffer, "Failed to create vertex buffer {}", SDL_GetError());

            SDL_SetGPUBufferName(device, vertexBuffer, "godot42 vertex buffer 😍");
        }

        {
            SDL_GPUBufferCreateInfo bufferInfo = {
                .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                .size  = getIndexBufferSize(),
                .props = 0, // by comment
            };
            indexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);
            NE_ASSERT(indexBuffer, "Failed to create index buffer {}", SDL_GetError());

            SDL_SetGPUBufferName(device, indexBuffer, "godot42 index buffer 😁");
        }

        // unifrom created and specify by shader create info
    }


    fillDefaultIndices(nullptr, info.primitiveType);

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);


    return pipeline != nullptr;
}

void GPURender_SDL::fillDefaultIndices(std::shared_ptr<CommandBuffer> commandBuffer, EGraphicPipeLinePrimitiveType primitiveType)
{
    std::vector<Uint32> indices(maxIndexBufferElemSize);

    switch (primitiveType) {
    case EGraphicPipeLinePrimitiveType::TriangleList:
    {
        // For triangle list, generate indices for quads (each quad is two triangles)
        // Pattern: 0,1,3, 0,3,2 for each quad
        // Calculate how many quads we can fit in our index buffer
        size_t maxQuads = maxIndexBufferElemSize / 6;

        for (uint32_t quad = 0; quad < maxQuads; ++quad) {
            uint32_t baseVertex  = quad * 4; // Each quad has 4 vertices
            uint32_t indexOffset = quad * 6; // Each quad has 6 indices

            // First triangle (0,1,3)
            indices[indexOffset]     = baseVertex;
            indices[indexOffset + 1] = baseVertex + 1;
            indices[indexOffset + 2] = baseVertex + 3;

            // Second triangle (0,3,2)
            indices[indexOffset + 3] = baseVertex;
            indices[indexOffset + 4] = baseVertex + 3;
            indices[indexOffset + 5] = baseVertex + 2;
        }
    } break;
    default:
        NE_CORE_ASSERT(false, "Unsupported primitive type for default indices: {}", int(primitiveType));
        break;
    }

    auto cb = acquireCommandBuffer(std::source_location::current());
    cb->uploadIndexBuffers(indices.data(), getIndexBufferSize());
    cb->submit();
}

void GPURender_SDL::createSamplers()
{
    // DefaultLinear
    SDL_GPUSamplerCreateInfo defaultLinearInfo = {
        .min_filter        = SDL_GPU_FILTER_LINEAR, // 缩小采样
        .mag_filter        = SDL_GPU_FILTER_LINEAR, // 放大采样
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .mip_lod_bias      = 0.0,
        .compare_op        = SDL_GPU_COMPAREOP_ALWAYS,
        .min_lod           = 1.0,
        .max_lod           = 1.0,
        .enable_anisotropy = false,
        .enable_compare    = false,
    };
    samplers[ESamplerType::DefaultLinear] = SDL_CreateGPUSampler(device, &defaultLinearInfo);

    // DefaultNearest
    SDL_GPUSamplerCreateInfo defaultNearestInfo = defaultLinearInfo;
    defaultNearestInfo.min_filter               = SDL_GPU_FILTER_NEAREST;
    defaultNearestInfo.mag_filter               = SDL_GPU_FILTER_NEAREST;
    defaultNearestInfo.mipmap_mode              = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplers[ESamplerType::DefaultNearest]      = SDL_CreateGPUSampler(device, &defaultNearestInfo);

    // PointClamp
    SDL_GPUSamplerCreateInfo pointClampInfo = {
        .min_filter     = SDL_GPU_FILTER_NEAREST,
        .mag_filter     = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    samplers[ESamplerType::PointClamp] = SDL_CreateGPUSampler(device, &pointClampInfo);

    // PointWrap
    SDL_GPUSamplerCreateInfo pointWrapInfo = {
        .min_filter     = SDL_GPU_FILTER_NEAREST,
        .mag_filter     = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    samplers[ESamplerType::PointWrap] = SDL_CreateGPUSampler(device, &pointWrapInfo);

    // LinearClamp
    SDL_GPUSamplerCreateInfo linearClampInfo = {
        .min_filter     = SDL_GPU_FILTER_LINEAR,
        .mag_filter     = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    samplers[ESamplerType::LinearClamp] = SDL_CreateGPUSampler(device, &linearClampInfo);

    // LinearWrap
    SDL_GPUSamplerCreateInfo linearWrapInfo = {
        .min_filter     = SDL_GPU_FILTER_LINEAR,
        .mag_filter     = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    samplers[ESamplerType::LinearWrap] = SDL_CreateGPUSampler(device, &linearWrapInfo);

    // AnisotropicClamp
    SDL_GPUSamplerCreateInfo anisotropicClampInfo = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .max_anisotropy    = 4,
        .enable_anisotropy = true,
    };
    samplers[ESamplerType::AnisotropicClamp] = SDL_CreateGPUSampler(device, &anisotropicClampInfo);


    // AnisotropicWrap
    SDL_GPUSamplerCreateInfo anisotropicWrapInfo = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_anisotropy    = 4,
        .enable_anisotropy = true,
    };
    samplers[ESamplerType::AnisotropicWrap] = SDL_CreateGPUSampler(device, &anisotropicWrapInfo);
    // validate samplers
    for (auto &[key, sampler] : samplers) {
        NE_CORE_ASSERT(sampler, "Failed to create sampler {} {}", (int)key, SDL_GetError());
    }
}
