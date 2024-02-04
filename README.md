# vkExample
 vkExample Qt5 Qt6  vulkan

# Building
git clone https://github.com/car520120/vkExample.git

打开 vkExample 目录
需要修改 xmake.lua  工程文件
设置 qt5 或者 qt6  set_config("qt","C:/Lib/Qt/6.6.1")
替换一下 vulkane 库相关路径 C:/Lib/VulkanSDK/1.3.224.1

替换好后执行以下命令就可以正常运行的了
xmake 
xmake run

或者执行以下命令生成 vs工程
xmake project -kvsxmake