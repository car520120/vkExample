#ifndef __RENDEROBJECT_H__
#define __RENDEROBJECT_H__
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

#pragma once
namespace VRcz
{
    struct UniformBufferObject {
        glm::mat4 modelMat;
        glm::mat4 viewMat;
        glm::mat4 projMat;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;

        static VkVertexInputBindingDescription generateBindingDescription() {
            VkVertexInputBindingDescription bindDesc = {};

            bindDesc.binding = 0;
            bindDesc.stride = sizeof(Vertex);
            bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindDesc;
        }

        static std::array<VkVertexInputAttributeDescription, 2> generateAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 2> attrDescs = {};

            attrDescs[0].binding = 0;
            attrDescs[0].location = 0;
            attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrDescs[0].offset = offsetof(Vertex, pos);

            attrDescs[1].binding = 0;
            attrDescs[1].location = 1;
            attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attrDescs[1].offset = offsetof(Vertex, color);

            return attrDescs;
        }
    };

    struct BufferResource 
    {
        VkBuffer  buffer = {};
        VkDeviceMemory memory = {};
        VkMemoryRequirements requirements = {};
    };

    struct VertexBuffer 
    {
        std::vector<Vertex> data = {};

        BufferResource clientResource = {};

        // Note this field should be decided when declaring, i.e. before creating the actual buffers.
        bool isServerResourceEnabled = false;
        // Optional server resource (Unused if client resource is host visible and coherent)
        BufferResource serverResource = {};

        VertexBuffer() = default;
        VertexBuffer(bool enableServerBuffer, const std::vector<Vertex>& vertices) {
            isServerResourceEnabled = enableServerBuffer;
            data = vertices;
        }
    };

    struct IndexBuffer {
        std::vector<uint32_t> data = {};

        BufferResource clientResource = {};
        BufferResource serverResource = {};

        IndexBuffer() = default;
        explicit IndexBuffer(const std::vector<uint32_t>& indices) { data = indices; }
    };

    struct RenderObject
    {
        VertexBuffer vertices;
        IndexBuffer indices;
        glm::mat4 transform;
        std::string name;
    };
}
#endif //__RENDEROBJECT_H__