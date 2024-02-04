#ifndef __VKWIDGET_H__
#define __VKWIDGET_H__
#include <QWidget>
#include <QMap>
#pragma once
QT_BEGIN_NAMESPACE
class QPaintEngine;
class QResizeEvent;
class QPaintEvent;
class QTimer;
QT_END_NAMESPACE
namespace VRcz
{
    class Scene;
    class RenderViewport;
    class VKWidget : public QWidget
    {
    private:
        QScopedPointer<RenderViewport> renderer_viewport;
        QScopedPointer<Scene> owner_scene;
        QMap<Qt::Key, bool> keys_state;
        QPointF mouse_pos;
        QPointF mouse_last;
    private:
        void init();
        void handleInputEvent();
    protected:
        void paintEvent(QPaintEvent* event) override;
        QPaintEngine* paintEngine() const override;
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
    public:
        void onUpdateRender();
    public:
        VKWidget(QWidget* parent = nullptr);
        ~VKWidget();
    };
}
#endif //__VKWIDGET_H__