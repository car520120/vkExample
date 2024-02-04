#ifndef __RENDERVIEWPORT_H__
#define __RENDERVIEWPORT_H__
#include <cstdint>
#include <glm/glm.hpp>
#pragma once
namespace VRcz
{
    class Scene;
    struct RenderContext;
    struct ViewportInfo
    {
        void* hwnd = nullptr;
        double dpr = 1.0;
        uint32_t coord_width = 0;
        uint32_t coord_height = 0;
        uint32_t pixel_width = dpr * coord_width;
        uint32_t pixel_height = dpr * coord_height;
        uint32_t update_count = 0;
        uint32_t render_count = 0;
        Scene* scene_ptr = nullptr;
    };
    class RenderViewport
    {
    private:
        RenderContext *ctx;
        ViewportInfo view_info;
    private:
        void checkValidationLayers();
        void createVkInstance();
        void createDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapChain();
        void createImageViews();
        void createDepthImageFormat();
        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createColorResources();
        void createDepthResources();
        void createFramebuffers();
        void createCommandPool();
        void createTextureSampler();
        void createCommandBuffers();
        void createSyncObjects();
        void createRenderObjects();
        void createUniformObjects();
    private:
        void resizeSwapChain();
        void waitUntilIdle() const;
        void recreateSwapChain();
        void destroySwapChain() const;
        void destroyDescriptor() const;

        void newFrame();
        void beginRenderPass() const;
        void endRenderPass() const;
        void presentFrame();
    private:
        void updateUniform();
        void updateDrawScene();
    private:
        void beginRender();
        void updateRender();
        void endRender();
    public:
        void startup(void* hwnd);
        void render();
    public:
        ViewportInfo* viewportInfo() { return &view_info; }
        void resize(uint32_t w, uint32_t h)
        {
            view_info.coord_width = w;
            view_info.coord_height = h;
            view_info.update_count++;
        }
        void resize(uint32_t w, uint32_t h, double dpr)
        {
            view_info.coord_width = w;
            view_info.coord_height = h;
            view_info.dpr = dpr;
            view_info.update_count++;
        }
        
        void setScene(Scene* scene_ptr)
        {
            view_info.scene_ptr = scene_ptr;
        }
    public:
        RenderViewport();
        ~RenderViewport();
    };
}
#endif //__RENDERVIEWPORT_H__