# LVGL嵌入式GUI库的模拟器项目

[LVGL](https://github.com/lvgl/lvgl)主要为微控制器和嵌入式系统编写，但你也可以**在PC上**运行该库，无需任何嵌入式硬件。在PC上编写的代码可以在使用嵌入式系统时直接复制。

## 快速链接

**在线模拟器**
- [模拟器列表](https://sim.lvgl.io/)
- [v8.3版本模拟器](https://sim.lvgl.io/v8.3/micropython/ports/javascript/index.html)

**源码下载**
- LVGL源码：[v8.3](https://github.com/lvgl/lvgl/tree/release/v8.3) | [v9.3](https://github.com/lvgl/lvgl/tree/release/v9.3)
- PC Eclipse模拟器：[v8.3](https://github.com/lvgl/lv_port_pc_eclipse/tree/release/v8.3) | [v9.3](https://github.com/lvgl/lv_port_pc_eclipse/tree/release/v9.3)
- PC VSCode模拟器：[v9.3](https://github.com/lvgl/lv_port_pc_vscode/tree/release/v9.3)
- LVGL驱动源码（v8.3需要）：[v8.3](https://github.com/lvgl/lv_drivers/tree/release/v8.3)

## 优势

使用PC模拟器而不是嵌入式硬件有几个优势：
* **成本为$0**，因为你无需购买或设计PCB
* **速度快**，因为你无需设计和制造PCB
* **协作性强**，因为任意数量的开发者可以在同一环境中工作
* **对开发者友好**，因为在PC上调试更容易、更快

## 项目搭建

### 新建工程

1. 创建目录`lvgl_simulator`作为项目目录
2. 解压`lv_port_pc_eclipse`或`lv_port_pc_vscode`的内容到项目目录
3. 解压LVGL源码，重命名为`lvgl`，放入项目目录
4. 解压`lv_drivers`源码（v8.3需要），重命名为`lv_drivers`，放入项目目录

### 环境配置

#### Windows开发环境

**MinGW安装**
- 下载[MinGW](https://pan.baidu.com/s/1QjK4r-I3xIfSnTMZyf5-lQ?pwd=6666)，解压到无中文和空格的路径（如`C:\devtools\mingw64`）
- 配置环境变量`Path`，添加：
  ```
  D:\Develop\mingw64\bin
  D:\Develop\mingw64\x86_64-w64-mingw32\bin
  ```
- 验证：`gcc -v`（需v12.2.0+）

**CMake安装**
- 下载[CMake](https://cmake.org/download/)并安装，将其添加到环境变量
- 验证：`cmake --version`

#### Linux安装

```bash
sudo apt-get update && sudo apt-get install -y build-essential libsdl2-dev cmake
```

## VSCode编译运行

### 安装扩展
- C/C++ Extension Pack
- CMake Tools

### 编译步骤
1. 用VSCode打开项目目录
2. 配置GCC编译器（如无显示，点击GCC进行配置）
3. 点击Build编译源码
4. 编译完成后，将`mingw64\x86_64-w64-mingw32\bin\SDL2.dll`复制到`bin`目录
5. 点击运行按钮执行

## Eclipse CDT使用（可选）

1. 从[Eclipse官网](http://www.eclipse.org/cdt/)下载Eclipse CDT
2. 打开Eclipse，点击**File→Import**，选择**General→Existing project into Workspace**
3. 浏览项目根目录，点击Finish
4. 构建并运行项目

## CMake命令行编译（可选）

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
./bin/main
```

## Docker运行（可选）

```bash
docker build -t lvgl_simulator .
docker run lvgl_simulator
```

## 常见问题

**重要提示**：项目路径不能包含中文和空格！

| 问题 | 解决方案 |
|------|--------|
| 找不到`glob.h` | 注释掉该头文件引用 |
| 找不到GCC | 确认环境变量PATH已配置MinGW路径；点击"扫描工具包"；重启VSCode |
| CMake找不到生成器 | 确保`cmake --version`正常；在CMake设置中配置生成器为"MinGW Makefiles" |
| CMakeCache错误 | 删除build文件夹，重新编译 |
| CMake配置失败 | 按F1输入`cmake reset`重置；删除build目录；重新选择GCC环境 |
| 无法查看输出日志 | 在CMakeLists.txt中添加：`if(CMAKE_HOST_WIN32) target_link_libraries(main -mconsole) endif()` |
| 代码无法提示和跳转 | 禁用clangd插件；在C/C++设置中选择"default"启用IntelliSense |

## 贡献

1. Fork本项目
2. 创建功能分支：`git checkout -b my-new-feature`
3. 提交更改：`git commit -am 'Add some feature'`
4. 推送分支：`git push origin my-new-feature`
5. 提交Pull Request

如发现问题，请通过[GitHub](https://github.com/lvgl/lvgl/issues)报告。

