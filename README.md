# vkExample
    vkExample Qt5 Qt6  vulkan
# Refer
    https://github.com/yiyaowen/RenderStationVulkan    macOS
    https://github.com/Vynokris/VulkanEngine           window
    https://vulkan-tutorial.com
    
    没有gl基础建议先去看一次gl相关的
    如果同你开车一样先开自动档熟识一下路况及规则
    再换手动档开为一不适应的可能就只有换档的了
    如果没有什么特殊需求，还是建议直接gl比较好
    https://zhuanlan.zhihu.com/p/20639338
# Platform
    当前例子只能在 win 上跑如果需要其它平台
    macOS 平台需要做以下修改 xmake.lua 把  VK_USE_PLATFORM_WIN32_KHR 替换 VK_USE_PLATFORM_METAL_EXT
    需要注意的是相对就的代码也是需要修改，具体参考 RenderStationVulkan
    其它平台直接参考 vulkan-tutorial 就可以的了
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
