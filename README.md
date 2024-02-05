# **vkExample**

- **Description**: This project demonstrates the use of Qt5, Qt6, and Vulkan for graphics rendering.

# **References**

- **RenderStationVulkan**: 适用于macOS用户的GitHub项目。访问地址：<https://github.com/yiyaowen/RenderStationVulkan>。
- **VulkanEngine**: 适用于Windows用户的另一个GitHub项目。访问地址：<https://github.com/Vynokris/VulkanEngine>。
- **Vulkan教程**: 提供Vulkan相关教程和资源的网站。访问地址：<https://vulkan-tutorial.com>。

  
- **对于没有OpenGL基础的开发者** 我们建议先了解一些OpenGL相关的知识。这就像学习如何驾驶汽车，首先从自动挡开始熟悉路况和规则，然后再过渡到手动挡。如果你没有特殊需求，我们还是建议你直接你使用OpenGL。https://zhuanlan.zhihu.com/p/20639338

# **Platform Considerations**

- 当前示例仅在Windows上运行。如果您需要其他平台，需要进行修改。对于macOS平台，您需要修改`xmake.lua`文件，将`VK_USE_PLATFORM_WIN32_KHR`替换为`VK_USE_PLATFORM_METAL_EXT`。请注意，其他代码也可能需要修改，因此请参考RenderStationVulkan项目以获取更详细的说明。其他平台可以简单地参考Vulkan教程网站。

# **Building Instructions**

- To clone the `vkExample` repository, run the following command in your terminal:
```bash
git clone https://github.com/car520120/vkExample.git
```
- Next, visit the following links to download the necessary compilation tools and documentation:
- <https://github.com/xmake-io/xmake/releases>
- <https://xmake.io/#/>

- 打开 vkExample 目录
- 需要修改 xmake.lua  工程文件
- 设置 qt5 或者 qt6 库路径 set_config("qt","C:/Lib/Qt/6.6.1")
- 替换一下 vulkane 库相关路径 C:/Lib/VulkanSDK/1.3.224.1
```cmd
xmake
xmake run
```
- 或者，您可以使用以下命令生成Visual Studio项目：command:
```cmd
xmake project -kvsxmake
```
