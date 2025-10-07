
[English](./README_EN.md)


这是 [BeShell](https://beshell.become.cool) 的演示工程。

[BeShell](https://beshell.become.cool) 是一个嵌入式 JavaScript 语言框架，让开发者仅使用 JS 语言开发嵌入式应用。

## 准备

#### 硬件

准备一个 ESP32 设备，任意 ESP32 开发板即可。

#### 开发环境

构建和烧录都在 ESP-IDF 环境下进行，首先你需要安装 ESP-IDF 。

* 独立安装 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.2/esp32/get-started/index.html) 
* **推荐** 在 VSCode 中使用 [ESP-IDF 扩展](vscode:extension/espressif.esp-idf-extension) 。


## 创建示例工程

在 VSCode 中从 ESP Component Registry 自动创建示例工程（需要 [ESP-IDF 扩展](vscode:extension/espressif.esp-idf-extension)）。


1. 快捷键 `Ctrl+Shift+P` ，输入关键词 `component`，选择 `ESP-IDF: Show ESP Comonent Registry` 

    ![](./doc/install-1.png "快捷键 `Ctrl+Shift+P` ，输入关键词 `component`，选择 `ESP-IDF: Show ESP Comonent Registry`")

2. 用关键词 `beshell` 搜索 component 

    ![](./doc/install-2.png "用关键词 `beshell` 搜索 component")

3. 点击例子 `basic` 进入页面

    ![](./doc/install-3.png "点击进入例子 `basic` ")

4. 点击页面上的 `Create project from this example`

    ![](./doc/install-4.png "Create project from this example")

5. 浏览存放工程的本地目录，然后 VScode 的 [ESP-IDF 扩展](vscode:extension/espressif.esp-idf-extension) 会自动下载工程并在新窗口打开。


## 构建和烧录


首次构建项目时，只需执行 `idf.py build flash`，即可完成整个项目的构建和烧录（设备已连接至PC）。

[BeShell](https://beshell.become.cool) 支持在设备的 flash 上分配一个独立的分区，用来存放 JavsScript 文件，可以像在 PC 上一样执行、import 这些 JS 文件。

工程的 CMakeLists.txt 提供了用于打包和烧录 JS 文件的命令:

#### 打包JS脚本


用 `idf.py pack-js` 命令打包 `js` 目录内的所有文件，生成一个用于烧录的镜像文件 `img/js.bin`。

> * 执行 `idf.py build` 构建整个项目时，也会自动执行该 js 打包命令
> * js 目录(`js`) 和 镜像文件(`img/js.bin`) 可以在 CMakeLists.txt 中修改
> * 若目标文件已经存在，且源文件目录下的所有文件都没有变动，该命令不会重复执行



#### 烧录 JS 脚本

用 `idf.py flash-js` 命令将 `img/js.bin` 文件烧录到设备 flash 上的 js 分区。

分区的起始地址可以在 CMakeLists.txt 中修改，应该和 partition.cvs 中的 js 分区起始位置一致，且 `img/js.bin` 文件大小不能超过 js 分区。

> * 执行 `idf.py flash` 烧录整个项目时，也会自动执行该 js 分区烧录命令 。



## 运行 JavaScript 例子

[BeShell](https://beshell.become.cool) 的固件提供了一个交互式执行 js 的环境 `REPL`，支持串口、websocket、bt、usb 等形式。可以用任意串口工具，或在线控制台 [BeConsole](https://beconsole.become.cools://beconsole.become.cool) 连接运行 [BeShell](https://beshell.become.cool) 固件的 esp32 设备，然后向固件发送命令或 JS 代码，接收程序输出和 JS 代码的返回值。

固件在启动时会自动运行 js 分区中的 `/main.js`（开机自启的 js 文件路径可以在 main.cpp 中修改)。
`/example` 目录下准备了一些 js 的例子，在串口工具或 [BeConsole](https://beconsole.become.cools://beconsole.become.cool) 里输入 `run [js文件的路径]` 例如 `run /example/wifi-ap.js` 就可以运行这个例子。输入 `reboot` 回到 `/main.js` 入口。


## 在线访问文件系统

[BeConsole](https://beconsole.become.cools://beconsole.become.cool) 连接上设备后，可以列出、访问、维护设备上的 JS 文件，还集成了 VSCode 编辑器内核支持在线编辑、实时运行设备上的 JS 文件。还可以将设备上的 JS 文件整个打包成 zip 文件下载。

