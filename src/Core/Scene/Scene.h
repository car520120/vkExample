#ifndef __SCENE_H__
#define __SCENE_H__
#include <vector>
#include <memory>
#pragma once
namespace VRcz
{
    struct RenderObject;
    class Camera;
    class Scene
    {
    private:
        std::vector<RenderObject*> render_objects;
          std::unique_ptr<Camera> main_camera;
    public:
        inline std::vector<RenderObject*>& renderObjects() { return render_objects; }
        inline auto mainCamera() { return main_camera.get(); }
    public:
        Scene();
        ~Scene();
    };
}
#endif //__SCENE_H__