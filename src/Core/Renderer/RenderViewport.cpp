#include "RenderViewport.h"
#include "RenderObject.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Camera.h"
#include <vulkan/vulkan.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <set>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <assert.h>
#include <iostream>
#include <optional>
#include <algorithm>
#define TMAX(a,b)            (((a) > (b)) ? (a) : (b))
namespace RenderViewportPrivate::Detail
{
    using namespace VRcz;
#ifdef NDEBUG
    const bool VALIDATION_LAYERS_ENABLED = false;
#else
    const bool VALIDATION_LAYERS_ENABLED = true;
#endif
    constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 3;
    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    };
    std::vector<BufferResource> uniforms;
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool IsComplete() const
        {
            return graphicsFamily.has_value()
                && presentFamily.has_value();
        }
    };

    struct vkRenderContext
    {
        VkInstance                      vkInstance = nullptr;
        VkDebugUtilsMessengerEXT        vkDebugMessenger = nullptr;
        VkSurfaceKHR                    vkSurface = nullptr;
        VkPhysicalDevice                vkPhysicalDevice = VK_NULL_HANDLE;
        VkDevice                        vkDevice = nullptr;
        QueueFamilyIndices              vkQueueFamilyIndices;
        VkQueue                         vkGraphicsQueue = nullptr;
        VkQueue                         vkPresentQueue = nullptr;
        VkSwapchainKHR                  vkSwapChain = nullptr;
        VkRenderPass                    vkRenderPass = nullptr;
        VkPipelineLayout                vkPipelineLayout = nullptr;
        VkPipeline                      vkGraphicsPipeline = nullptr;
        VkCommandPool                   vkCommandPool = nullptr;
        VkSampler                       vkTextureSampler = nullptr;
        VkImage                         vkColorImage = nullptr;
        VkDeviceMemory                  vkColorImageMemory = nullptr;
        VkImageView                     vkColorImageView = nullptr;
        VkImage                         vkDepthImage = nullptr;
        VkDeviceMemory                  vkDepthImageMemory = nullptr;
        VkImageView                     vkDepthImageView = nullptr;
        VkFormat                        vkDepthImageFormat = VK_FORMAT_UNDEFINED;
        std::vector<VkCommandBuffer>    vkCommandBuffers;
        std::vector<VkSemaphore>        vkImageAvailableSemaphores;
        std::vector<VkSemaphore>        vkRenderFinishedSemaphores;
        std::vector<VkFence>            vkInFlightFences;
        std::vector<VkFramebuffer>      vkSwapChainFramebuffers;
        std::vector<VkImage>            vkSwapChainImages;
        std::vector<VkImageView>        vkSwapChainImageViews;
        VkFormat                        vkSwapChainImageFormat = VK_FORMAT_UNDEFINED;
        uint32_t                        vkSwapChainWidth = 0, vkSwapChainHeight = 0;
        uint32_t                        vkSwapchainImageIndex = 0;
        VkSampleCountFlagBits           msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkDescriptorSetLayout           vkDescriptorLayout = nullptr;
        VkDescriptorPool                vkDescriptorPool = nullptr;
        VkDescriptorSet                 vkDescriptorSet = nullptr;
        bool                            framebufferResized = false;
        uint32_t                        currentFrame = 0;
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR* capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;

        SwapChainSupportDetails() 
            : capabilities(new VkSurfaceCapabilitiesKHR()) 
        {}
        SwapChainSupportDetails(const SwapChainSupportDetails& other) {
            capabilities = new VkSurfaceCapabilitiesKHR;
            *capabilities = *other.capabilities;
            formats = other.formats;
            presentModes = other.presentModes;
        }

        ~SwapChainSupportDetails()
        {
            delete capabilities;
        }

        bool IsAdequate() const
        {
            return !formats.empty() && !presentModes.empty();
        }
    };

    struct DistanceFogParams
    {
        alignas(16) glm::vec3 color;
        float start;
        float end;
        float invLength;
    };

    enum class LogSeverity
    {
        None,
        Info,
        Warning,
        Error,
    };

    inline static  VkBool32 VkDebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        LogSeverity logSeverity;
        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            logSeverity = LogSeverity::Info;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logSeverity = LogSeverity::Warning;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logSeverity = LogSeverity::Error;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        default:
            logSeverity = LogSeverity::None;
            break;
        }

        //Logger::PushLog({ LogType::Vulkan, logSeverity, pCallbackData->pMessage, "", "", -1 });
        return false;
    }

    inline static  QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
    {
        QueueFamilyIndices queueFamilyIndices;

        // Get an array of all the queue families on the given GPU.
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // Get all queue family indices.
        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            // Find the graphics family.
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                queueFamilyIndices.graphicsFamily = i;

            // Find the present family.
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                queueFamilyIndices.presentFamily = i;
            }

            // Exit if the queue family indices have all been set.
            if (queueFamilyIndices.IsComplete())
                break;
            ++i;
        }
        return queueFamilyIndices;
    }

    inline static  bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device)
    {
        // Get the number of extensions on the given GPU.
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        // Get an array of all the extensions on the given GPU.
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // Create an array of unique required extension names.
        std::set<std::string> requiredExtensions(EXTENSIONS.begin(), EXTENSIONS.end());

        // Make sure all required extensions are available.
        for (const VkExtensionProperties& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    inline static SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
    {
        SwapChainSupportDetails details;

        // Get surface capabilities.
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, details.capabilities);

        // Get supported surface formats.
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // Get supported presentation modes.
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    inline static bool IsDeviceSuitable(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
    {
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return FindQueueFamilies(device, surface).IsComplete()
            && CheckDeviceExtensionSupport(device)
            && QuerySwapChainSupport(device, surface).IsAdequate()
            && supportedFeatures.samplerAnisotropy;
    }

    inline static VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDevice& device)
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

        const VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    inline static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        // Try to find the best surface format.
        for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        // Return the first format as a last resort.
        return availableFormats[0];
    }

    inline static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        // Try to find the best present mode (for triple buffering).
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        // Use FIFO present mode by default.
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    inline static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities, ViewportInfo &view_info)
    {
        auto image = &capabilities->minImageExtent;
        // Make sure the extent is between the minimum and maximum extents.
        auto width = std::clamp(view_info.pixel_width, image->width, image->width);
        auto height = std::clamp(view_info.pixel_height, image->height, image->height);
        VkExtent2D extent{ width ,height };
        return extent;
    }

    inline static VkFormat FindSupportedFormat(const VkPhysicalDevice& device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (const VkFormat& format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(device, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
                return format;
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
                return format;
        }

        //LogError(LogType::Vulkan, "Failed to find supported format.");
        throw std::runtime_error("VULKAN_NO_SUPPORTED_FORMAT_ERROR");
    }

    // TODO: Put this method somewhere it belongs.
    static std::vector<char> ReadBinFile(const std::string& filename)
    {
        // Open the binary file and place the cursor at its end.
        std::ifstream f(filename, std::ios::ate | std::ios::binary);
        if (!f.is_open()) {
            //LogError(LogType::FileIO, "Failed to open file: " + filename);
            throw std::runtime_error("FILE_IO_ERROR");
        }

        // Get the file size and allocate a buffer with it.
        const size_t fSize = (size_t)f.tellg();
        std::vector<char> buffer(fSize);

        // Move the cursor back to the start of the file and read it.
        f.seekg(0);
        f.read(buffer.data(), (std::streamsize)fSize);
        f.close();
        return buffer;
    }

    inline static void CreateShaderModule(const VkDevice& device, const char* filename, VkShaderModule& shaderModule)
    {
        // Read the shader code.
        const std::vector<char> shaderCode = ReadBinFile(filename);

        // Set shader module creation information.
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        // Create and return the shader module.
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create shader module.");
            throw std::runtime_error("VULKAN_SHADER_MODULE_ERROR");
        }
    }

    inline static uint32_t FindMemoryType(const VkPhysicalDevice& device, const uint32_t& typeFilter, const VkMemoryPropertyFlags& properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        //LogError(LogType::Vulkan, "Failed to find suitable memory type.");
        throw std::runtime_error("VULKAN_FIND_MEMORY_TYPE_ERROR");
    }

    inline static void CreateImage(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const uint32_t& width, const uint32_t& height, const uint32_t& mipLevels, const VkSampleCountFlagBits& numSamples, const VkFormat& format, const VkImageTiling& tiling, const VkImageUsageFlags& usage, const VkMemoryPropertyFlags& properties, VkImage& image, VkDeviceMemory& imageMemory)
    {
        // Create a vulkan image.
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = (uint32_t)width;
        imageInfo.extent.height = (uint32_t)height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = numSamples;
        imageInfo.flags = 0; // Optional.
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create texture image.");
            throw std::runtime_error("VULKAN_TEXTURE_IMAGE_ERROR");
        }

        // Allocate GPU memory for the image.
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to allocate texture image memory.");
            throw std::runtime_error("VULKAN_TEXTURE_IMAGE_MEMORY_ERROR");
        }
        vkBindImageMemory(device, image, imageMemory, 0);
    }

    inline static void CreateImageView(const VkDevice& device, const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags, const uint32_t& mipLevels, VkImageView& imageView)
    {
        // Set creation information for the image view.
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;

        // Set color channel swizzling.
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // Choose how the image is used and set the mipmap and layer counts.
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        // Create the image view.
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create image view.");
            throw std::runtime_error("VULKAN_IMAGE_VIEW_ERROR");
        }
    }

    inline static VkMemoryRequirements CreateBuffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Create the buffer.
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create buffer.");
            throw std::runtime_error("VULKAN_BUFFER_CREATION_ERROR");
        }

        // Find the buffer's memory requirements.
        VkMemoryRequirements memRequirements = {};
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        // Find which memory type is most suitable for the buffer.
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        // Allocate the buffer's memory.
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to allocate buffer memory.");
            throw std::runtime_error("VULKAN_BUFFER_ALLOCATION_ERROR");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
        return memRequirements;
    }

    inline static VkCommandBuffer BeginSingleTimeCommands(const VkDevice& device, const VkCommandPool& commandPool)
    {
        // Create a new command buffer.
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        // Start recording the new command buffer.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    inline static void EndSingleTimeCommands(const VkDevice& device, const VkCommandPool& commandPool, const VkQueue& graphicsQueue, const VkCommandBuffer& commandBuffer)
    {
        // Execute the command buffer and wait until it is done.
        vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    inline static void CopyBuffer(const VkDevice& device, const VkCommandPool& commandPool, const VkQueue& graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, const VkDeviceSize& size)
    {
        const VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        // Issue the command to copy the source buffer to the destination one.
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
    }
    
    inline static void CreateVertexBuffer(vkRenderContext* ctx,VertexBuffer& obj)
    {
        static constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        static constexpr auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // create vertex vk memory
        auto& gpu_buffer = obj.serverResource.buffer;
        auto& gpu_memory = obj.serverResource.memory;
        auto& gpu_req = obj.serverResource.requirements;

        auto& cpu_buffer = obj.clientResource.buffer;
        auto& cpu_memory = obj.clientResource.memory;
        auto& cpu_req = obj.clientResource.requirements;

        auto& vertices = obj.data;
        auto vertices_size = sizeof(Vertex) * vertices.size();

        cpu_req = CreateBuffer(ctx->vkDevice, ctx->vkPhysicalDevice, vertices_size, usage, properties, cpu_buffer, cpu_memory);

        // Map the buffer's GPU memory to CPU memory, and write buffer params to it.
        void* data;
        vkMapMemory(ctx->vkDevice, cpu_memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, vertices.data(), vertices_size);
        vkUnmapMemory(ctx->vkDevice, cpu_memory);

        gpu_req = CreateBuffer(ctx->vkDevice, ctx->vkPhysicalDevice, vertices_size, usage, properties, gpu_buffer, gpu_memory);
        CopyBuffer(ctx->vkDevice, ctx->vkCommandPool, ctx->vkGraphicsQueue, cpu_buffer, gpu_buffer, cpu_req.size);


    }

    inline static void CreateIndicesBuffer(vkRenderContext* ctx, IndexBuffer& obj)
    {
        static constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        static constexpr auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // create vertex vk memory
        auto& gpu_buffer = obj.serverResource.buffer;
        auto& gpu_memory = obj.serverResource.memory;
        auto& gpu_req = obj.serverResource.requirements;

        auto& cpu_buffer = obj.clientResource.buffer;
        auto& cpu_memory = obj.clientResource.memory;
        auto& cpu_req = obj.clientResource.requirements;

        auto& indices = obj.data;
        auto indices_size = sizeof(uint32_t) * indices.size();
        cpu_req = CreateBuffer(ctx->vkDevice, ctx->vkPhysicalDevice, indices_size, usage, properties, cpu_buffer, cpu_memory);

        // Map the buffer's GPU memory to CPU memory, and write buffer params to it.
        void* data;
        vkMapMemory(ctx->vkDevice, cpu_memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, indices.data(), indices_size);
        vkUnmapMemory(ctx->vkDevice, cpu_memory);

        gpu_req = CreateBuffer(ctx->vkDevice, ctx->vkPhysicalDevice, indices_size, usage, properties, gpu_buffer, gpu_memory);

        CopyBuffer(ctx->vkDevice, ctx->vkCommandPool, ctx->vkGraphicsQueue, cpu_buffer, gpu_buffer, cpu_req.size);
    }

    inline static void CreateUniformBuffer(vkRenderContext* ctx, BufferResource& obj)
    {
        static constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        static constexpr auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // create vertex vk memory
        auto& cpu_buffer = obj.buffer;
        auto& memory = obj.memory;
        auto& req = obj.requirements;
        auto ubo_size = sizeof(UniformBufferObject);
        req = CreateBuffer(ctx->vkDevice, ctx->vkPhysicalDevice, ubo_size, usage, properties, cpu_buffer, memory);


    }

    inline static void DestroyObject(vkRenderContext* ctx,BufferResource& obj)
    {
        vkDestroyBuffer(ctx->vkDevice, obj.buffer, nullptr);
        vkFreeMemory(ctx->vkDevice, obj.memory, nullptr);
    }
}

namespace VRcz
{
    using namespace RenderViewportPrivate::Detail;
    struct RenderContext final : public vkRenderContext {};

    void RenderViewport::checkValidationLayers()
    {
        // Get the number of vulkan layers on the machine.
        uint32_t layerCount; 
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get all the vulkan layer names. 
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Make sure all required validation layers are installed on the machine.
        bool validationLayersError = false;
        for (const char* layerName : VALIDATION_LAYERS)
        {
            bool layerFound = false;

            for (const VkLayerProperties& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                validationLayersError = true;
                break;
            }
        }

        // Throw and error if a validation layer hasn't been found.
        if (VALIDATION_LAYERS_ENABLED && validationLayersError) {
            //LogError(LogType::Vulkan, "Some of the requested validation layers are not available.");
            throw std::runtime_error("VULKAN_VALIDATION_LAYERS_ERROR");
        }
    }

    void RenderViewport::createVkInstance()
    {
        // Set application information.
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VRczApp";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "VRcz";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // Set vulkan instance creation information. 
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Set validation layer information. 
        if (VALIDATION_LAYERS_ENABLED) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // Set glfw extension information. 
        uint32_t glfwExtensionCount = 0;
        //const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        //std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        std::vector<const char*> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_WIN32_KHR
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif // VK_USE_PLATFORM_WIN32_KHR


        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Create the vulkan instance. 
        if (vkCreateInstance(&createInfo, nullptr, &ctx->vkInstance) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Unable to create a Vulkan instance.");
            throw std::runtime_error("VULKAN_INIT_ERROR");
        }
    }

    void RenderViewport::createDebugMessenger()
    {
        // Setup debug messenger properties.
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.pfnUserCallback = VkDebugMessengerCallback;
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        messengerInfo.pUserData = nullptr;
        auto messenger = &(ctx->vkDebugMessenger);
        const auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->vkInstance, "vkCreateDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT(ctx->vkInstance, &messengerInfo, nullptr, messenger) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Unable to link engine logger to Vulkan log outputs.");
            throw std::runtime_error("VULKAN_LOGGER_INIT_ERROR");
        }
    }

    void RenderViewport::createSurface()
    {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VkWin32SurfaceCreateInfoKHR createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.hinstance = GetModuleHandle(NULL);
        createInfo.hwnd = static_cast<HWND>(view_info.hwnd);
        if (VkResult::VK_SUCCESS != vkCreateWin32SurfaceKHR(ctx->vkInstance, &createInfo, nullptr, &ctx->vkSurface))
        {
            //    LogError(LogType::Vulkan, "Unable to create a  window surface.");
            throw std::runtime_error("VULKAN_WINDOW_SURFACE_ERROR");
        }
#endif // VK_USE_PLATFORM_WIN32_KHR
    }

    void RenderViewport::pickPhysicalDevice()
    {
        // Get the number of GPUs with Vulkan support.
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(ctx->vkInstance, &deviceCount, nullptr);

        // Make sure at least one GPU supports Vulkan.
        if (deviceCount == 0) {
            //LogError(LogType::Vulkan, "Couldn't find a GPU with Vulkan support.");
            throw std::runtime_error("GPU_NO_VULKAN_SUPPORT_ERROR");
        }

        // Get an array of all GPUs with Vulkan support.
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(ctx->vkInstance, &deviceCount, devices.data());

        // Try to find a suitable GPU to run Vulkan on.
        for (const auto& device : devices) {
            if (IsDeviceSuitable(device, ctx->vkSurface)) {
                ctx->vkPhysicalDevice = device;
                ctx->msaaSamples = GetMaxUsableSampleCount(ctx->vkPhysicalDevice);
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(device, &properties);
                //LogInfo(LogType::Vulkan, "Using rendering device " + std::string(properties.deviceName));
                break;
            }
        }

        // Make sure a suitable GPU was found.
        if (ctx->vkPhysicalDevice == VK_NULL_HANDLE) {
            //LogError(LogType::Vulkan, "Couldn't find a GPU with Vulkan support.");
            throw std::runtime_error("GPU_NO_VULKAN_SUPPORT_ERROR");
        }
    }

    void RenderViewport::createLogicalDevice()
    {
        ctx->vkQueueFamilyIndices = FindQueueFamilies(ctx->vkPhysicalDevice, ctx->vkSurface);

        // Set creation information for all required queues.
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        const std::set<uint32_t> uniqueQueueFamilies = { ctx->vkQueueFamilyIndices.graphicsFamily.value(), ctx->vkQueueFamilyIndices.presentFamily.value() };
        for (const uint32_t& queueFamily : uniqueQueueFamilies)
        {
            const float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Specify the devices features to be used.
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.sampleRateShading = VK_TRUE;
        deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;

        // Enable null descriptors.
        VkPhysicalDeviceRobustness2FeaturesEXT deviceRobustnessFeatures{};
        deviceRobustnessFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        deviceRobustnessFeatures.nullDescriptor = VK_TRUE;

        // Set the logical device creation information.
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(EXTENSIONS.size());
        deviceCreateInfo.ppEnabledExtensionNames = EXTENSIONS.data();
        deviceCreateInfo.pNext = &deviceRobustnessFeatures;
        if (VALIDATION_LAYERS_ENABLED) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            deviceCreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }
        else {
            deviceCreateInfo.enabledLayerCount = 0;
        }

        // Create the logical device.
        if (vkCreateDevice(ctx->vkPhysicalDevice, &deviceCreateInfo, nullptr, &ctx->vkDevice) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create a logical device.");
            throw std::runtime_error("VULKAN_LOGICAL_DEVICE_ERROR");
        }

        // Get the graphics and present queue handles.
        vkGetDeviceQueue(ctx->vkDevice, ctx->vkQueueFamilyIndices.graphicsFamily.value(), 0, &ctx->vkGraphicsQueue);
        vkGetDeviceQueue(ctx->vkDevice, ctx->vkQueueFamilyIndices.presentFamily.value(), 0, &ctx->vkPresentQueue);
    }

    void RenderViewport::createSwapChain()
    {
        const SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(ctx->vkPhysicalDevice, ctx->vkSurface);

        // Get the surface format, presentation mode and extent of the swap chain.
        const VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats); //选择渲染面格式
        const VkPresentModeKHR   presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes); //选择呈现模式
        const VkExtent2D         extent = ChooseSwapExtent(swapChainSupport.capabilities, view_info); //选择扩展(能力)

        // Choose the number of images in the swap chain.
        uint32_t imageCount = swapChainSupport.capabilities->minImageCount + 1;
        if (swapChainSupport.capabilities->maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities->maxImageCount)
        {
            imageCount = swapChainSupport.capabilities->maxImageCount;
        }

        // Set creation information for the swap chain.
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = ctx->vkSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.presentMode = presentMode;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // Set the used queue families.
        const QueueFamilyIndices indices = FindQueueFamilies(ctx->vkPhysicalDevice, ctx->vkSurface);
        const uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            // If the graphics and present family are different, share the swapchain between them.
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            // If they are the same, give total ownership of the swapchain to the queue.
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;       // Optional.
            createInfo.pQueueFamilyIndices = nullptr; // Optional.
        }

        // Set the transform to apply to swapchain images.
        createInfo.preTransform = swapChainSupport.capabilities->currentTransform;

        // Choose whether the alpha channel should be used for blending with other windows in the window system or ignored.
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // Don't render pixels that are obscured by other windows.
        createInfo.clipped = VK_TRUE;

        // Create the swap chain.

        if (vkCreateSwapchainKHR(ctx->vkDevice, &createInfo, nullptr, &ctx->vkSwapChain) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create a swap chain.");
            throw std::runtime_error("VULKAN_SWAP_CHAIN_ERROR");
        }

        // Get the swap chain images.
        vkGetSwapchainImagesKHR(ctx->vkDevice, ctx->vkSwapChain, &imageCount, nullptr);
        ctx->vkSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(ctx->vkDevice, ctx->vkSwapChain, &imageCount, ctx->vkSwapChainImages.data());

        // Get the swap chain format.
        ctx->vkSwapChainImageFormat = surfaceFormat.format;
        ctx->vkSwapChainWidth = view_info.coord_width;
        ctx->vkSwapChainHeight = view_info.coord_height;
    }

    void RenderViewport::createImageViews()
    {
        auto& image = ctx->vkSwapChainImages;
        auto& fmt = ctx->vkSwapChainImageFormat;
        auto& view = ctx->vkSwapChainImageViews;
        constexpr auto aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ctx->vkSwapChainImageViews.resize(image.size());
        for (size_t i = 0; i < image.size(); i++)
            CreateImageView(ctx->vkDevice, image[i], fmt, aspect, 1, view[i]);
    }

    void RenderViewport::createDepthImageFormat()
    {
        ctx->vkDepthImageFormat = FindSupportedFormat(ctx->vkPhysicalDevice,
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    void RenderViewport::createRenderPass()
    {
        // Create a color buffer attachment.
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = ctx->vkSwapChainImageFormat;
        colorAttachment.samples = ctx->msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // No stencil is used so we don't care.
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Create the depth buffer attachment.
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = ctx->vkDepthImageFormat;
        depthAttachment.samples = ctx->msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Create the color resolve attachment (to display the multisampled color buffer).
        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = ctx->vkSwapChainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference colorAttachmentResolveRef{};
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Create a single sub-pass.
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        // Create a sub-pass dependency.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // Set the render pass creation information.
        const std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = (uint32_t)attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        // Create the render pass.
        if (vkCreateRenderPass(ctx->vkDevice, &renderPassInfo, nullptr, &ctx->vkRenderPass) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create render pass.");
            throw std::runtime_error("VULKAN_RENDER_PASS_ERROR");
        }
    }

    void RenderViewport::createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        layoutBinding.pImmutableSamplers = nullptr;

        // Create the descriptor set layout.
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;
        if (vkCreateDescriptorSetLayout(ctx->vkDevice, &layoutInfo, nullptr, &ctx->vkDescriptorLayout) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create descriptor set layout.");
            throw std::runtime_error("VULKAN_DESCRIPTOR_SET_LAYOUT_ERROR");
        }

        // Set the type and number of descriptors.
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        // Create the descriptor pool.
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;
        if (vkCreateDescriptorPool(ctx->vkDevice, &poolInfo, nullptr, &ctx->vkDescriptorPool) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create descriptor pool.");
            throw std::runtime_error("VULKAN_DESCRIPTOR_POOL_ERROR");
        }

        // Allocate the descriptor set.
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = ctx->vkDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &ctx->vkDescriptorLayout;
        if (vkAllocateDescriptorSets(ctx->vkDevice, &allocInfo, &ctx->vkDescriptorSet) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to allocate descriptor sets.");
            throw std::runtime_error("VULKAN_DESCRIPTOR_SET_ALLOCATION_ERROR");
        }
    }

    void RenderViewport::createGraphicsPipeline()
    {
        // Load vulkan fragment and vertex shaders.
        VkShaderModule vertShaderModule{};
        VkShaderModule fragShaderModule{};
        CreateShaderModule(ctx->vkDevice, "Shaders/VulkanVert.spv", vertShaderModule);//设置顶点着色器
        CreateShaderModule(ctx->vkDevice, "Shaders/VulkanFrag.spv", fragShaderModule);//设置片段着色器

        // Set the vertex shader's pipeline stage and entry point.
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // Set the fragment shader's pipeline stage and entry point.
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        // Create an array of both pipeline stage info structures.
        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        auto bindingDescription = Vertex::generateBindingDescription();
        auto attributeDescriptions = Vertex::generateAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();

        // Specify the kind of geometry to be drawn.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Specify the scissor rectangle (pixels outside it will be discarded).
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { ctx->vkSwapChainWidth, ctx->vkSwapChainHeight };

        // Create the dynamic states for viewport and scissor.
        std::vector dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Set rasterizer parameters.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Use VK_POLYGON_MODE_LINE to draw wireframe.
        rasterizer.lineWidth = 1.f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.f; // Optional.
        rasterizer.depthBiasClamp = 0.f; // Optional.
        rasterizer.depthBiasSlopeFactor = 0.f; // Optional.

        // Set multisampling parameters (disabled).
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = ctx->msaaSamples;
        multisampling.sampleShadingEnable = VK_TRUE;
        multisampling.minSampleShading = .2f;
        multisampling.pSampleMask = nullptr;  // Optional.
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional.
        multisampling.alphaToOneEnable = VK_FALSE; // Optional.

        // Depth and stencil buffer parameters.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.f; // Optional.
        depthStencil.maxDepthBounds = 1.f; // Optional.
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional.
        depthStencil.back = {}; // Optional.

        // Set color blending parameters for current framebuffer (alpha blending enabled).
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // Set color blending parameters for all framebuffers.
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional.
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.f; // Optional.
        colorBlending.blendConstants[1] = 0.f; // Optional.
        colorBlending.blendConstants[2] = 0.f; // Optional.
        colorBlending.blendConstants[3] = 0.f; // Optional.

        // Define 1 push constant (viewPos) and 3 descriptor set layouts (model, material, lights).
        //const VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Maths::Vector3) };
        //const VkDescriptorSetLayout setLayouts[4] = {
        //    Resources::Model::GetVkDescriptorSetLayout(),
        //    ctx->constDataDescriptorLayout,
        //    Resources::Material::GetVkDescriptorSetLayout(),
        //    Resources::Light::GetVkDescriptorSetLayout()
        //};

        // Set the pipeline layout creation information.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &ctx->vkDescriptorLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // Create the pipeline layout.
        if (vkCreatePipelineLayout(ctx->vkDevice, &pipelineLayoutInfo, nullptr, &ctx->vkPipelineLayout) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create pipeline layout.");
            throw std::runtime_error("VULKAN_PIPELINE_LAYOUT_ERROR");
        }

        // Set the graphics pipeline creation information.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = ctx->vkPipelineLayout;
        pipelineInfo.renderPass = ctx->vkRenderPass;
        pipelineInfo.subpass = 0;

        // Create the graphics pipeline.
        if (vkCreateGraphicsPipelines(ctx->vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ctx->vkGraphicsPipeline) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create graphics pipeline.");
            throw std::runtime_error("VULKAN_GRAPHICS_PIPELINE_ERROR");
        }

        // Destroy both shader modules.
        vkDestroyShaderModule(ctx->vkDevice, fragShaderModule, nullptr);
        vkDestroyShaderModule(ctx->vkDevice, vertShaderModule, nullptr);
    }

    void RenderViewport::createColorResources()
    {
        // Create the color image and image view.
        CreateImage(ctx->vkDevice, ctx->vkPhysicalDevice, ctx->vkSwapChainWidth, ctx->vkSwapChainHeight, 1, ctx->msaaSamples, ctx->vkSwapChainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ctx->vkColorImage, ctx->vkColorImageMemory);
        CreateImageView(ctx->vkDevice, ctx->vkColorImage, ctx->vkSwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, ctx->vkColorImageView);
    }

    void RenderViewport::createDepthResources()
    {
        // Create the depth image and image view.
        CreateImage(ctx->vkDevice, ctx->vkPhysicalDevice, ctx->vkSwapChainWidth, ctx->vkSwapChainHeight, 1, ctx->msaaSamples, ctx->vkDepthImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ctx->vkDepthImage, ctx->vkDepthImageMemory);
        CreateImageView(ctx->vkDevice, ctx->vkDepthImage, ctx->vkDepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, ctx->vkDepthImageView);
    }

    void RenderViewport::createFramebuffers()
    {
        ctx->vkSwapChainFramebuffers.resize(ctx->vkSwapChainImageViews.size());

        for (size_t i = 0; i < ctx->vkSwapChainImageViews.size(); i++)
        {
            std::array<VkImageView, 3> attachments = {
                ctx->vkColorImageView,
                ctx->vkDepthImageView,
                ctx->vkSwapChainImageViews[i]
            };

            // Create the current framebuffer creation information.
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = ctx->vkRenderPass;
            framebufferInfo.attachmentCount = (uint32_t)attachments.size();
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = ctx->vkSwapChainWidth;
            framebufferInfo.height = ctx->vkSwapChainHeight;
            framebufferInfo.layers = 1;

            // Create the current framebuffer.
            if (vkCreateFramebuffer(ctx->vkDevice, &framebufferInfo, nullptr, &ctx->vkSwapChainFramebuffers[i]) != VK_SUCCESS) {
                //LogError(LogType::Vulkan, "Failed to create framebuffer.");
                throw std::runtime_error("VULKAN_FRAMEBUFFER_ERROR");
            }
        }
    }

    void RenderViewport::createCommandPool()
    {
        const QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(ctx->vkPhysicalDevice, ctx->vkSurface);

        // Set the command pool creation information.
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        // Create the command pool.
        if (vkCreateCommandPool(ctx->vkDevice, &poolInfo, nullptr, &ctx->vkCommandPool) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create command pool.");
            throw std::runtime_error("VULKAN_COMMAND_POOL_ERROR");
        }
    }

    void RenderViewport::createTextureSampler()
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(ctx->vkPhysicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod = 0.f;
        samplerInfo.maxLod = (float)std::floor(std::log2(TMAX(ctx->vkSwapChainWidth, ctx->vkSwapChainHeight))) + 1; // (float)texture->GetMipLevels();
        if (vkCreateSampler(ctx->vkDevice, &samplerInfo, nullptr, &ctx->vkTextureSampler) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to create texture sampler.");
            throw std::runtime_error("VULKAN_TEXTURE_SAMPLER_ERROR");
        }
    }

    void RenderViewport::createCommandBuffers()
    { 
        // Set the command buffer allocation information.
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx->vkCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        // Allocate the command buffer.
        ctx->vkCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateCommandBuffers(ctx->vkDevice, &allocInfo, ctx->vkCommandBuffers.data()) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to allocate command buffers.");
            throw std::runtime_error("VULKAN_COMMAND_BUFFER_ERROR");
        }
    }

    void RenderViewport::createSyncObjects()
    {
        ctx->vkImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        ctx->vkRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        ctx->vkInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        // Set the semaphore and fence creation information.
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // Create the semaphores and fence.
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(ctx->vkDevice, &semaphoreInfo, nullptr, &ctx->vkImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(ctx->vkDevice, &semaphoreInfo, nullptr, &ctx->vkRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(ctx->vkDevice, &fenceInfo, nullptr, &ctx->vkInFlightFences[i]) != VK_SUCCESS)
            {
                //LogError(LogType::Vulkan, "Failed to create semaphores and fence.");
                throw std::runtime_error("VULKAN_SYNC_OBJECTS_ERROR");
            }
        }
    }

    void RenderViewport::createRenderObjects()
    {
        for (auto obj : view_info.scene_ptr->renderObjects())
        {
            CreateVertexBuffer(ctx,obj->vertices);
            CreateIndicesBuffer(ctx, obj->indices);
        }
    }

    void RenderViewport::createUniformObjects()
    {
        uniforms.resize(ctx->vkSwapChainImages.size());
        for (size_t i = 0; i < uniforms.size(); i++)
        {
            CreateUniformBuffer(ctx, uniforms[i]);

            VkDescriptorBufferInfo uboBufferInfo = {};
            uboBufferInfo.buffer = uniforms[i].buffer;
            uboBufferInfo.offset = 0;
            uboBufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet uboDescriptorWrite = {};
            uboDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uboDescriptorWrite.dstSet = ctx->vkDescriptorSet;
            uboDescriptorWrite.dstBinding = 0;
            uboDescriptorWrite.dstArrayElement = 0;
            uboDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboDescriptorWrite.descriptorCount = 1;
            uboDescriptorWrite.pBufferInfo = &uboBufferInfo;
            uboDescriptorWrite.pImageInfo = nullptr;
            uboDescriptorWrite.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(ctx->vkDevice, 1, &uboDescriptorWrite, 0, nullptr);
        }
    }

    void RenderViewport::resizeSwapChain()
    {
        ctx->framebufferResized = true;
    }

    void RenderViewport::waitUntilIdle() const
    {
        vkDeviceWaitIdle(ctx->vkDevice);
    }

    void RenderViewport::recreateSwapChain()
    {
        waitUntilIdle(); //空跑等待渲染完成
        destroySwapChain(); //释放交换链
        createSwapChain(); //构造新的交换链
        createImageViews(); //构造新的渲染图像
        createColorResources(); //构造颜色图像资源
        createDepthResources(); //构造深度图资源
        createFramebuffers(); //构造渲染帧
        createUniformObjects(); //构造 ubo
    }

    void RenderViewport::destroySwapChain() const
    {
        for (auto& ubo : uniforms)
        {
            vkDestroyBuffer(ctx->vkDevice, ubo.buffer, nullptr);
            vkFreeMemory(ctx->vkDevice, ubo.memory, nullptr);
        }
        for (const VkFramebuffer& vkSwapChainFramebuffer : ctx->vkSwapChainFramebuffers)
            vkDestroyFramebuffer(ctx->vkDevice, vkSwapChainFramebuffer, nullptr);
        for (const VkImageView& vkSwapChainImageView : ctx->vkSwapChainImageViews)
            vkDestroyImageView(ctx->vkDevice, vkSwapChainImageView, nullptr);
        vkDestroyImageView(ctx->vkDevice, ctx->vkDepthImageView, nullptr);
        vkDestroyImage(ctx->vkDevice, ctx->vkDepthImage, nullptr);
        vkFreeMemory(ctx->vkDevice, ctx->vkDepthImageMemory, nullptr);
        vkDestroyImageView(ctx->vkDevice, ctx->vkColorImageView, nullptr);
        vkDestroyImage(ctx->vkDevice, ctx->vkColorImage, nullptr);
        vkFreeMemory(ctx->vkDevice, ctx->vkColorImageMemory, nullptr);
        vkDestroySwapchainKHR(ctx->vkDevice, ctx->vkSwapChain, nullptr);
       
    }

    void RenderViewport::destroyDescriptor() const
    {
        if (ctx->vkDescriptorLayout)
        {
            vkDestroyDescriptorSetLayout(ctx->vkDevice, ctx->vkDescriptorLayout, nullptr);
        }

        if (ctx->vkDescriptorPool)
        {
            vkDestroyDescriptorPool(ctx->vkDevice, ctx->vkDescriptorPool, nullptr);
        }
    }

    void RenderViewport::newFrame()
    {
        // Wait for the previous frame to finish. 等待上一帧渲染完成
        vkWaitForFences(ctx->vkDevice, 1, &ctx->vkInFlightFences[ctx->currentFrame], VK_TRUE, UINT64_MAX);

        // Acquire an image from the swap chain. 在交换链中取出渲染图像
        const VkResult result = vkAcquireNextImageKHR(ctx->vkDevice, ctx->vkSwapChain, UINT64_MAX, ctx->vkImageAvailableSemaphores[ctx->currentFrame], VK_NULL_HANDLE, &ctx->vkSwapchainImageIndex);

        // Recreate the swap chain if it is out of date.
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain(); //渲染出错，需机重新构造渲染交换链
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            //LogError(LogType::Vulkan, "Failed to acquire swap chain image.");
            throw std::runtime_error("VULKAN_IMAGE_ACQUISITION_ERROR");
        }

        //重置栅格化
        // Only reset fences if we are going to be submitting work.
        vkResetFences(ctx->vkDevice, 1, &ctx->vkInFlightFences[ctx->currentFrame]);
    }

    void RenderViewport::beginRenderPass() const
    {
        // Reset the command buffer. //重置当前帧命令缓冲区
        vkResetCommandBuffer(ctx->vkCommandBuffers[ctx->currentFrame], 0);

        // Begin recording the command buffer.
        VkCommandBufferBeginInfo beginInfo{}; //设置开始记录缓冲命令
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;       // Optional.
        beginInfo.pInheritanceInfo = nullptr; // Optional.
        if (vkBeginCommandBuffer(ctx->vkCommandBuffers[ctx->currentFrame], &beginInfo) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to begin recording command buffer.");
            throw std::runtime_error("VULKAN_BEGIN_COMMAND_BUFFER_ERROR");
        }

        // Define the clear color. //设置清屏色
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Begin the render pass. //开始设置命令缓冲区
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ctx->vkRenderPass;
        renderPassInfo.framebuffer = ctx->vkSwapChainFramebuffers[ctx->vkSwapchainImageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { ctx->vkSwapChainWidth, ctx->vkSwapChainHeight };
        renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
        renderPassInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(ctx->vkCommandBuffers[ctx->currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline. //绑定图形管线
        vkCmdBindPipeline(ctx->vkCommandBuffers[ctx->currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->vkGraphicsPipeline);

        // Set the viewport. //绑定渲染视口
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)ctx->vkSwapChainWidth;
        viewport.height = (float)ctx->vkSwapChainHeight;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(ctx->vkCommandBuffers[ctx->currentFrame], 0, 1, &viewport);

        // Set the scissor. //设置渲染视口剪切信息
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { ctx->vkSwapChainWidth, ctx->vkSwapChainHeight };
        vkCmdSetScissor(ctx->vkCommandBuffers[ctx->currentFrame], 0, 1, &scissor);
    }

    void RenderViewport::endRenderPass() const
    {
        // End the render pass.
        vkCmdEndRenderPass(ctx->vkCommandBuffers[ctx->currentFrame]);

        // Stop recording the command buffer.
        if (vkEndCommandBuffer(ctx->vkCommandBuffers[ctx->currentFrame]) != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to record command buffer.");
            throw std::runtime_error("VULKAN_RECORD_COMMAND_BUFFER_ERROR");
        }
    }

    void RenderViewport::presentFrame()
    {
        // Set the command buffer submit information.
        const VkSemaphore          waitSemaphores[] = { ctx->vkImageAvailableSemaphores[ctx->currentFrame] };
        const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        const VkSemaphore          signalSemaphores[] = { ctx->vkRenderFinishedSemaphores[ctx->currentFrame] };
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx->vkCommandBuffers[ctx->currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // Submit the graphics command queue. 提交图形管道队列,开始栅格化
        const VkResult submit_result = vkQueueSubmit(ctx->vkGraphicsQueue, 1, &submitInfo, ctx->vkInFlightFences[ctx->currentFrame]);
        if (VK_SUCCESS != submit_result) {
            //LogError(LogType::Vulkan, "Failed to submit draw command buffer.");
            //throw std::runtime_error("VULKAN_SUBMIT_COMMAND_BUFFER_ERROR");
        }

        // Present the frame. //查询渲染呈现
        const VkSwapchainKHR swapChains[] = { ctx->vkSwapChain };
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &ctx->vkSwapchainImageIndex;
        presentInfo.pResults = nullptr; // Optional.
        const VkResult result = vkQueuePresentKHR(ctx->vkPresentQueue, &presentInfo);

        // Recreate the swap chain if it is out of date or suboptimal.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || ctx->framebufferResized) {
            ctx->framebufferResized = false;
            recreateSwapChain(); //渲染出错需要，重新构造渲染交换链
        }
        else if (result != VK_SUCCESS) {
            //LogError(LogType::Vulkan, "Failed to present swap chain image.");
            throw std::runtime_error("VULKAN_PRESENTATION_ERROR");
        }

        // Move to the next frame. 切换到下一帧渲染命令缓冲区
        ctx->currentFrame = (ctx->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void RenderViewport::updateUniform()
    {
        auto camera = view_info.scene_ptr->mainCamera();
        UniformBufferObject ubo = {};
        ubo.modelMat = glm::mat4(1.0f);
        camera->updateViewMatrix(ubo.viewMat);
        camera->updateProjMatrix(ubo.projMat);

        auto ubo_size = sizeof(UniformBufferObject);
        auto index = ctx->currentFrame;
        auto& memory = uniforms[index].memory;
        void* data;
        vkMapMemory(ctx->vkDevice, memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, &ubo, ubo_size);
        vkUnmapMemory(ctx->vkDevice, memory);
    }

    void RenderViewport::updateDrawScene()
    {
        // Draw Model
        VkDeviceSize offsets = 0;
        auto index = ctx->currentFrame;
        auto& vkCommandBuffers = ctx->vkCommandBuffers[index];
        auto& vkPipelineLayout = ctx->vkPipelineLayout;
        auto descriptor = &ctx->vkDescriptorSet;
        auto camera_pos = glm::vec3{ 0.f,0.f,0.f };
        auto vec3_size = sizeof(glm::vec3);
        constexpr auto state = VK_SHADER_STAGE_FRAGMENT_BIT;
        //vkCmdPushConstants(vkCommandBuffers, vkPipelineLayout, state, 0, vec3_size, &camera_pos);
        for (auto obj : view_info.scene_ptr->renderObjects())
        {
            VkBuffer vertices = obj->vertices.serverResource.buffer;
            VkBuffer indices = obj->indices.serverResource.buffer;
            vkCmdBindVertexBuffers(vkCommandBuffers, 0, 1, &vertices, &offsets);
            vkCmdBindIndexBuffer(vkCommandBuffers, indices, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(vkCommandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout, 0, 1, descriptor, 0, nullptr);
            vkCmdDrawIndexed(vkCommandBuffers, obj->indices.data.size(), 1, 0, 0, 0);
        }
    }

    void RenderViewport::beginRender()
    {
        newFrame(); //渲染开始需要构造渲染帧
        beginRenderPass(); //设置渲染缓冲帧
    }

    void RenderViewport::updateRender()
    {
        updateUniform();
        updateDrawScene();
        /*
        // set model view projection
        float w = view_info.pixel_width;
        float h = view_info.pixel_width;
        float aspect = w / h;
        constexpr auto fov = glm::radians(120.f);
        glm::mat4 projection = glm::perspectiveFovLH(fov, w, h, 0.1f, 100.f); 
        glm::mat4 view = glm::mat4(1.f); //view_info.scene_ptr->cameraMatrix();
        glm::mat4 model(1.f);
        model = glm::translate(model, glm::vec3(0.f, 0.f, 0.0f));
        model = glm::scale(model, glm::vec3(1.f, 1.0f, 1.0f));
        //model = glm::rotate(model, 360.f, glm::vec3(0.f, 0.f, 1.f));
        UniformBufferObject ubo = { model,view,projection };
        auto ubo_size = sizeof(UniformBufferObject);
        auto index =  ctx->currentFrame;
        auto& memory = uniforms[index].memory;
        void* data;
        vkMapMemory(ctx->vkDevice, memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, &ubo, ubo_size);
        vkUnmapMemory(ctx->vkDevice, memory);

        // Draw Model
        VkDeviceSize offsets = 0;
        auto& vkCommandBuffers = ctx->vkCommandBuffers[index];
        auto& vkPipelineLayout = ctx->vkPipelineLayout;
        auto descriptor = &ctx->vkDescriptorSet;
        auto camera_pos = glm::vec3{ 0.f,0.f,0.f };
        auto vec3_size = sizeof(glm::vec3);
        constexpr auto state = VK_SHADER_STAGE_FRAGMENT_BIT;
        //vkCmdPushConstants(vkCommandBuffers, vkPipelineLayout, state, 0, vec3_size, &camera_pos);
        for (auto obj : view_info.scene_ptr->renderObjects())
        {
            VkBuffer vertices = obj->vertices.serverResource.buffer;
            VkBuffer indices = obj->indices.serverResource.buffer;
            vkCmdBindVertexBuffers(vkCommandBuffers, 0, 1, &vertices, &offsets);
            vkCmdBindIndexBuffer(vkCommandBuffers, indices, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(vkCommandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout, 0, 1, descriptor, 0, nullptr);
            vkCmdDrawIndexed(vkCommandBuffers, obj->indices.data.size(), 1, 0, 0, 0);
        }
        */
    }

    void RenderViewport::endRender()
    {
        endRenderPass();
        presentFrame();
    }

    void RenderViewport::startup(void* hwnd)
    {
        //ctx->vkPhysicalDevice = VK_NULL_HANDLE;      
        //ctx->vkDepthImageFormat = VK_FORMAT_UNDEFINED; 
        //ctx->vkSwapChainImageFormat = VK_FORMAT_UNDEFINED; 
        //ctx->msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        assert(nullptr != hwnd);
        view_info.hwnd = hwnd;
        checkValidationLayers(); 
        createVkInstance();
        createDebugMessenger(); 
        createSurface(); 
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createDepthImageFormat();
        createRenderPass();  //构造渲染信息
        //createDescriptorLayoutsAndPools(); //构造渲染对象结构及相关信息
        createDescriptorSetLayout();//构造渲染对象结构及相关信息
        createGraphicsPipeline(); //构造图形渲染管线
        createColorResources(); //构造色彩资源
        createDepthResources(); //构造深度图资源
        createFramebuffers();   //构造帧缓冲区
        createCommandPool();    //构造渲染命令池（队列）
        createTextureSampler(); //设置纹理采样器
        createCommandBuffers(); //设置命令缓冲区
        createSyncObjects();    //构造栅格化信号量（可渲染图像信号，渲染完成信号）
        //setDistanceFogParams({ 0.f,0.f,0.f }, 60.f, 100.f); 暂时没有雾的功能

        createRenderObjects();
        createUniformObjects();
    }

    void RenderViewport::render()
    {
        if (view_info.update_count != view_info.render_count)
        {
            assert(0 < view_info.dpr);
            auto camera = view_info.scene_ptr->mainCamera();
            view_info.render_count = view_info.update_count;
            view_info.pixel_width = (uint32_t)(view_info.dpr * view_info.coord_width);
            view_info.pixel_height = (uint32_t)(view_info.dpr * view_info.coord_height);
            camera->setViewSize(view_info.pixel_width, view_info.pixel_height);
        }

        beginRender();
        updateRender();
        endRender();
    }

    RenderViewport::RenderViewport()
        :ctx(new RenderContext())
    {

    }

    RenderViewport::~RenderViewport()
    {
        waitUntilIdle();
        const auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->vkInstance, "vkDestroyDebugUtilsMessengerEXT");
        for (auto obj : uniforms)
        {
            DestroyObject(ctx, obj);
        }

        //for (auto obj : view_info.scene_ptr->renderObjects())
        //{
        //    DestroyObject(ctx, obj->vertices.clientResource);
        //    DestroyObject(ctx, obj->vertices.serverResource);

        //    DestroyObject(ctx, obj->indices.serverResource);
        //    DestroyObject(ctx, obj->indices.serverResource);
        //}

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(ctx->vkDevice, ctx->vkRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(ctx->vkDevice, ctx->vkImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(ctx->vkDevice, ctx->vkInFlightFences[i], nullptr);
        }

        destroySwapChain();
        destroyDescriptor();
        vkDestroySampler(ctx->vkDevice, ctx->vkTextureSampler, nullptr);
        vkDestroyCommandPool(ctx->vkDevice, ctx->vkCommandPool, nullptr);
        vkDestroyPipeline(ctx->vkDevice, ctx->vkGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(ctx->vkDevice, ctx->vkPipelineLayout, nullptr);
        vkDestroyRenderPass(ctx->vkDevice, ctx->vkRenderPass, nullptr);
        vkDestroyDevice(ctx->vkDevice, nullptr);
        vkDestroySurfaceKHR(ctx->vkInstance, ctx->vkSurface, nullptr);
        vkDestroyDebugUtilsMessengerEXT(ctx->vkInstance, ctx->vkDebugMessenger, nullptr);
        vkDestroyInstance(ctx->vkInstance, nullptr);
        delete ctx;
    }
}
