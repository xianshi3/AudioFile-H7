# LVGL嵌入式GUI库的模拟器项目

[LVGL](https://github.com/lvgl/lvgl)主要为微控制器和嵌入式系统编写，但你也可以**在PC上**运行该库，无需任何嵌入式硬件。在PC上编写的代码可以在使用嵌入式系统时直接复制。

使用PC模拟器而不是嵌入式硬件有几个优势：
* **成本为$0**，因为你无需购买或设计PCB
* **速度快**，因为你无需设计和制造PCB
* **协作性强**，因为任意数量的开发者可以在同一环境中工作
* **对开发者友好**，因为在PC上调试更容易、更快

## 系统要求
PC模拟器跨平台支持。**Windows、Linux和OSX**都支持，但在Windows上使用[另一个模拟器](https://docs.lvgl.io/latest/en/html/get-started/pc-simulator.html)项目会更容易上手。

* **SDL** 低级驱动库，用于图形、鼠标、键盘等
* 本项目（配置为**Eclipse CDT IDE**）

## 使用方法

### 获取PC项目

克隆PC项目及相关子模块：

```
git clone --recursive https://github.com/littlevgl/pc_simulator_sdl_eclipse.git
```

### 安装SDL
你可以从 https://www.libsdl.org/ 下载SDL

在Linux上，你可以通过终端安装：
```
sudo apt-get update && sudo apt-get install -y build-essential libsdl2-dev
```

### 安装Eclipse CDT
从 http://www.eclipse.org/cdt/ 下载并安装Eclipse CDT

### 导入PC模拟器项目
1. 打开Eclipse CDT
2. 点击**File->Import**，选择**General->Existing project into Workspace**
3. 浏览项目根目录，点击Finish
4. 构建你的项目并运行它

## CMake

以下步骤可在类Unix系统上使用CMake。这可能也适用于其他操作系统，但未经测试。

1. 确保已安装CMake，即`cmake`命令在终端中可用。
2. 创建新目录。名称无关紧要，但本教程使用`build`。
3. 输入`cd build`。
4. 输入`cmake ..`。CMake将生成适当的构建文件。
5. 输入`make -j4`或（更便携的）`cmake --build . --parallel`。

**注意：** CMake v3.12及以后版本支持`--parallel`。如果你使用较旧版本的CMake，请从命令中删除`--parallel`或使用make选项。

6. 二进制文件位于`../bin/main`，可通过输入该命令运行。

## Docker
1. 构建Docker容器
```
docker build -t lvgl_simulator .
```
2. 运行Docker容器
```
docker run lvgl_simulator
```
Docker GUI依赖于平台。例如，在macOS上，你可以按照[本教程](https://cntnr.io/running-guis-with-docker-on-mac-os-x-a14df6a76efc)运行类似的命令：
```
docker run -e DISPLAY=10.103.56.101:0 lvgl_simulator
```

请注意，在macOS上，启动Xquartz前可能需要启用间接GLX渲染：
```
defaults write org.macosforge.xquartz.X11 enable_iglx -bool true
open -a Xquartz
```

对于配有X Server的Linux环境，以下是`docker run`命令。注意第一个命令`xhost +`授予所有人对X Server的访问权限。

```
xhost +
docker run -e DISPLAY=$DISPLAY -v /tmp/.X11-unix/:/tmp/.X11-unix:ro -t lvgl_simulator
```

## 贡献
1. Fork本项目！
2. 创建你的功能分支：`git checkout -b my-new-feature`
3. 提交你的更改：`git commit -am 'Add some feature'`
4. 推送到分支：`git push origin my-new-feature`
5. 提交Pull Request！

如果你发现问题，请通过GitHub报告！

