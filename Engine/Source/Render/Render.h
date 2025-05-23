#pragma once

#include <memory>
#include <source_location>
#include <vector>

#include "Core/Log.h"

#include "Render/Device.h"
#include "reflect.cc/enum"

#include "Render/CommandBuffer.h"


// Forward declarations
struct CommandBuffer;

namespace RenderAPI
{
enum T
{
    None = 0,
    OpenGL,
    Vulkan,
    DirectX12,
    Metal,
    SDL3GPU, // SDL3 GPU backend
    ENUM_MAX,
};
};

enum class EGraphicPipeLinePrimitiveType
{
    TriangleList,
    Line,
    ENUM_MAX,
};

struct VertexBufferDescription
{
    uint32_t slot;
    uint32_t pitch;
};

namespace EVertexAttributeFormat
{
enum T
{
    Float2 = 0,
    Float3,
    Float4,
    ENUM_MAX,
};


extern std::size_t T2Size(T type);

GENERATED_ENUM_MISC(T);
}; // namespace EVertexAttributeFormat


struct VertexAttribute
{
    uint32_t                  location;
    uint32_t                  bufferSlot;
    EVertexAttributeFormat::T format;
    uint32_t                  offset;
};

struct ShaderCreateInfo
{
    std::string shaderName; // we use single glsl now
};

namespace EFrontFaceType
{
enum T
{
    ClockWise = 0,
    CounterClockWise,
};
};

struct GraphicsPipelineCreateInfo
{
    bool                                 bDeriveInfoFromShader = true;
    ShaderCreateInfo                     shaderCreateInfo;
    std::vector<VertexBufferDescription> vertexBufferDescs;
    std::vector<VertexAttribute>         vertexAttributes;
    EGraphicPipeLinePrimitiveType        primitiveType = EGraphicPipeLinePrimitiveType::TriangleList;
    EFrontFaceType::T                    frontFaceType = EFrontFaceType::CounterClockWise;
};

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

//  template <typename W>
//     W *getW()
//     {
//         static_assert(requires { RenderType::getW; });
//         return static_cast<RenderType *>(this)->getW();
//     }
// #define STATIC_POLYMORPHIC_FUNC(CHILD_T, RET_TEMPLATE_NAME, FUNC_NAME, ...)                                                 \
//     template <typename RET_TEMPLATE_NAME>                                                                                   \
//     RET_TEMPLATE_NAME FUNC_NAME(__VA_ARGS__)                                                                                \
//     {                                                                                                                       \
//         static_assert(requires { CHILD_T::FUNC_NAME; }, STRINGIFY(CHILD_T) " Need implement " STRINGIFY(FUNC_NAME) "!!!!"); \
//         return static_cast<CHILD_T *>(this)->FUNC_NAME(__VA_ARGS__);                                                        \
//     }
// #undef STATIC_POLYMORPHIC_FUNC



extern RenderAPI::T gRenderAPIType;