#include "VKWidget.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/Camera.h"
#include "Core/Renderer/RenderViewport.h"

#include <QApplication>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QWindow>

namespace VRcz
{
    void VKWidget::init()
    {
        setObjectName("VKWidget");
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
        // Replace fullscreen button with maximization button on OS.
        setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
        windowHandle()->setSurfaceType(QWindow::VulkanSurface);
        owner_scene.reset(new Scene());
        renderer_viewport.reset(new RenderViewport());
        renderer_viewport->resize(width(), height(), devicePixelRatio());
        renderer_viewport->setScene(owner_scene.get());
        void* hwnd = reinterpret_cast<void*>(windowHandle()->winId());
        renderer_viewport->startup(hwnd);

        keys_state[Qt::Key_A] = false;
        keys_state[Qt::Key_D] = false;
        keys_state[Qt::Key_Q] = false;
        keys_state[Qt::Key_E] = false;
        keys_state[Qt::Key_W] = false;
        keys_state[Qt::Key_S] = false;
    }
    void VKWidget::handleInputEvent()
    {
        constexpr auto move_speed = 0.02f;
        auto camera = owner_scene->mainCamera();

        auto cbMove = [=](Qt::Key a, Qt::Key b)
        {
            auto v = keys_state[a] ? -1.f : 0.f;
            v += keys_state[b] ? 1.f : 0.f;
            return  v * move_speed;
        };

     
        float horizontal = cbMove(Qt::Key_A,Qt::Key_D);

        float vertical = cbMove(Qt::Key_Q, Qt::Key_E);

        float frontback = cbMove(Qt::Key_W, Qt::Key_S);
        camera->translate(horizontal, vertical, frontback);
    }
    void VKWidget::paintEvent(QPaintEvent* ev)
    {
        Q_UNUSED(ev);
    }
    QPaintEngine* VKWidget::paintEngine() const
    {
        //Be sure to rewrite to avoid QT damaging the stack
        return nullptr;
    }
    void VKWidget::resizeEvent(QResizeEvent* ev)
    {
        auto sz = ev->size();
        renderer_viewport->resize(sz.width(),sz.height());
        QWidget::resizeEvent(ev);
    }
    
    void VKWidget::keyPressEvent(QKeyEvent* ev)
    {
        auto key = Qt::Key(ev->key());
        if (keys_state.contains(key))
        {
            
            if (!keys_state[key])
            {
                keys_state[key] = true;
            }
         
        }
        else
        {
            QWidget::keyPressEvent(ev);
        }
    }
    
    void VKWidget::keyReleaseEvent(QKeyEvent* ev)
    {
        auto key = Qt::Key(ev->key());
        if (keys_state.contains(key))
        {
            if (keys_state[key])
            {
                keys_state[key] = false;
            }
        }
        else
        {
            QWidget::keyReleaseEvent(ev);
        }
    }
    
    void VKWidget::mouseMoveEvent(QMouseEvent* ev)
    {
        constexpr auto rotate_speed = 0.005f;
        mouse_pos = ev->windowPos();
        if (Qt::NoButton != ev->buttons())
        {
            auto camera = owner_scene->mainCamera();
            auto delta = (mouse_last - mouse_pos) * rotate_speed;
            camera->rotate(delta.y(), delta.x());
        }
        else
        {
            QWidget::mouseMoveEvent(ev);
        }
        mouse_last = mouse_pos;
    }
    
    void VKWidget::wheelEvent(QWheelEvent* ev)
    {
        constexpr auto zoom_speed = 0.002f;
        auto camera = owner_scene->mainCamera();
#if QT_VERSION_MAJOR > 5
        auto angle = ev->angleDelta();
        auto delta = angle.x() + angle.y();
        camera->zoom(zoom_speed * delta);
#else
        camera->zoom(zoom_speed * ev->delta());
#endif
    }

    void VKWidget::onUpdateRender()
    {
        handleInputEvent();
        renderer_viewport->render();
    }

    VKWidget::VKWidget(QWidget* parent)
    {
        init();
    }
    
    VKWidget::~VKWidget()
    {
    }
}
