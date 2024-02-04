#include "Camera.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include <glm/gtc/matrix_transform.hpp>
namespace VRcz
{
    void Camera::updateViewMatrix(glm::mat4 &view_mat)
    {
        camera_right.y = 0.f;
        camera_right = glm::normalize(camera_right);
        camera_look_at = glm::normalize(camera_look_at);
        camera_up = glm::cross(camera_look_at, camera_right);

        view_mat = glm::lookAtLH(camera_eye, camera_eye + camera_look_at, camera_up);
    }
    
    void Camera::updateProjMatrix(glm::mat4 &proj_mat)
    {
        float w = static_cast<float>(width);
        float h = static_cast<float>(height);
        proj_mat = glm::perspectiveFovLH(glm::radians(fov), w, h, view_near, view_far);
    }

    void Camera::translate(glm::vec3 pos)
    {
        camera_eye += pos;
    }

    void Camera::translate(float dx, float dy, float dz)
    {
            camera_eye += glm::vec3(dx, dy, dz);
    }
    
    void Camera::rotate(float pitch, float yaw, float roll)
    {
        auto do_patch = glm::rotate(glm::mat4(1.0f), pitch, camera_right);
        auto do_pitch_yaw = glm::rotate(do_patch, yaw, camera_up);
        // auto do_pitch_yaw_rool = glm::rotate(do_pitch_yaw, roll, camera_look_at);

        auto zo_right = glm::vec4(camera_right,0.f);
        auto zo_up =  glm::vec4(camera_up,0.f);
        auto zo_look_at =  glm::vec4(camera_look_at,0.f);

        camera_right = glm::vec3(zo_right * do_pitch_yaw);
        camera_up = glm::vec3(zo_up * do_pitch_yaw);
        camera_look_at = glm::vec3(zo_look_at * do_pitch_yaw);
    }
    
    void Camera::zoom(float delta)
    {
        translate(delta * camera_look_at);
    }

    Camera::Camera(int scene_width, int scene_height, float scene_fov,float scene_near, float scene_far)
        : width(scene_width)
        , height(scene_height)
        , fov(scene_fov)
        , view_near(scene_near)
        , view_far((scene_far))
        , camera_eye{0}
        , camera_right{0}
        , camera_up{0}
        , camera_look_at{0} 
    {}

    Camera::~Camera() 
    {}

}
