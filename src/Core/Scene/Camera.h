#ifndef __CAMERA_H__
#define __CAMERA_H__
#include "glm/fwd.hpp"
#include <glm/glm.hpp>
#pragma once
namespace VRcz
{
    class Camera
    {
    private:
        int width;
        int height;
        float fov;
        float view_near;
        float view_far;
        glm::vec3 camera_eye;
        glm::vec3 camera_right;
        glm::vec3 camera_up;
        glm::vec3 camera_look_at;
    public:
        inline void setViewSize(int w, int h) { width = w; height = h; }
        inline void setFrustumDepth(float scene_near,float scene_far) { view_near = scene_near;view_far = scene_far; }

        inline auto eye(){ return camera_eye; }
        inline auto setEye(glm::vec3 pos) { camera_eye = pos; }
        inline auto setEye(float dx, float dy, float dz) { camera_eye = glm::vec3{ dx,dy,dz }; }

        inline auto right() { return camera_right; }
        inline auto setRight(glm::vec3 pos) { camera_right = pos; }
        inline auto setRight(float dx, float dy, float dz) { camera_right = glm::vec3{ dx,dy,dz }; }

        inline auto up() { return camera_up; }
        inline auto setUp(glm::vec3 pos) { camera_up = pos; }
        inline auto setUp(float dx, float dy, float dz) { camera_up = glm::vec3{ dx,dy,dz }; }

    
        inline auto lookAt() { return camera_look_at; }
        inline auto setLookAt(glm::vec3 pos) { camera_look_at = pos; }
        inline auto setLookAt(float dx, float dy, float dz) { camera_look_at = glm::vec3{ dx,dy,dz }; }
    public:
        void updateViewMatrix(glm::mat4 &view_mat);
        void updateProjMatrix(glm::mat4 &proj_mat);

        void translate(glm::vec3 pos);
        void translate(float dx, float dy, float dz);
        void rotate(float pitch, float yaw, float roll = 0.0f);
        void zoom(float delta);

    public:
        Camera(int scene_width,int scene_height,float scene_fov,float scene_near,float scene_far);
        ~Camera();
    };
}
#endif //__CAMERA_H__