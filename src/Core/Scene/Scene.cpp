#include "Scene.h"
#include "Camera.h"
#include "Core/Renderer/RenderObject.h"
#include "Core/Scene/Camera.h"
#include <memory>

namespace ScenePrivate::detail
{
    struct ColorPalette
    {
        inline static constexpr glm::vec3  white    = { 1.f,1.f,1.f };
        inline static constexpr glm::vec3  black    = { 0.f,0.f,0.f };
        inline static constexpr glm::vec3  red      = { 1.f,0.f,0.f };
        inline static constexpr glm::vec3  green    = { 0.f,1.f,0.f };
        inline static constexpr glm::vec3  blue     = { 0.f,0.f,1.f };
        inline static constexpr glm::vec3  yellow   = { 1.f,1.f,0.f };
        inline static constexpr glm::vec3  cyan     = { 0.f,1.f,1.f };
        inline static constexpr glm::vec3  magenta  = { 1.f,0.f,1.f };

    };
}

namespace VRcz
{
    typedef ScenePrivate::detail::ColorPalette ColorPalette;
    Scene::Scene()
    {

        main_camera = std::make_unique<Camera>(0, 0, 90.f, 0.1f, 100.f);
        main_camera->setEye(0.f,0.f,-5.f);
        main_camera->setRight(1.f, 0.f, 0.f);
        main_camera->setUp(0.f, 1.f, 0.f);
        main_camera->setLookAt(0.f, 0.f, 1.f);

        render_objects.push_back(new RenderObject());
        auto& obj = render_objects.back();
        obj->name = "cube";
        obj->vertices.isServerResourceEnabled = true;
        obj->vertices.data = {
            { { -0.5f, -0.5f, -0.5f }, ColorPalette::white },
            { { -0.5f, +0.5f, -0.5f }, ColorPalette::black },
            { { +0.5f, +0.5f, -0.5f }, ColorPalette::red },
            { { +0.5f, -0.5f, -0.5f,}, ColorPalette::green },
            { { -0.5f, -0.5f, +0.5f }, ColorPalette::blue },
            { { -0.5f, +0.5f, +0.5f }, ColorPalette::yellow },
            { { +0.5f, +0.5f, +0.5f }, ColorPalette::cyan },
            { { +0.5f, -0.5f, +0.5f }, ColorPalette::magenta },
        };

        obj->indices.data = {
            // front face
           0, 1, 2,
           0, 2, 3,

           // back face
           4, 6, 5,
           4, 7, 6,

           // left face
           4, 5, 1,
           4, 1, 0,

           // right face
           3, 2, 6,
           3, 6, 7,

           // top face
           1, 5, 6,
           1, 6, 2,

           // bottom face
           4, 0, 3,
           4, 3, 7
        };
    }

    Scene::~Scene()
    {
    }
}
