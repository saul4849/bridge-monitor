# Bridge Displacement Monitor

基于 Qt + OpenCV + Linux V4L2 的工业结构健康监测上位机系统，面向桥梁/建筑位移的实时视觉测量场景。

## 项目概述

系统通过 V4L2 采集工业相机视频流，利用 OpenCV 模板匹配算法实时追踪靶标位移，在 Qt GUI 上同步显示视频画面与位移时序曲线，支持靶标管理、零点校准与数据导出。已在 RK3576 嵌入式平台（ARM64）上验证运行。

## 功能特性

- [x] V4L2 MMAP 多缓冲视频采集（NV12 → RGB，零拷贝）
- [x] 实时图像显示与交互式 ROI 框选（鼠标拖拽 + 事件过滤器）
- [x] OpenCV 模板匹配靶标跟踪（NCC，置信度 ≥ 0.55）
- [x] 亚像素级位移跟踪（可选插值优化，提升低像素比场景精度）
- [x] 实时位移计算（像素 → mm，支持自定义像素比例）
- [x] 零点校准与参考图像动态更新
- [x] 实时位移曲线绘制（QPainter 自绘，X/Y 双通道，零第三方图表依赖）
- [x] 监测数据 CSV 自动导出
- [x] 靶标管理（增删改、参数配置 JSON 导入导出）
- [x] 深色工业主题界面（QSS 自定义）

## 技术栈

| 层级 | 技术 |
|------|------|
| GUI 框架 | Qt 5/6 (QWidget, 信号槽, 自定义事件过滤器, QPainter) |
| 视觉算法 | OpenCV 4.x (模板匹配, 图像格式转换, ROI 处理) |
| 视频采集 | Linux V4L2 API (MMAP 零拷贝, 多缓冲队列) |
| 硬件平台 | RK3576 (ARM64) + 工业相机 |
| 构建工具 | qmake |
| 数据存储 | CSV / JSON |

## 系统架构

[工业相机] --V4L2/MMAP--&gt; [VideoWidget] --cv::Mat--&gt;
                                      |
                    [TargetManager] ←→ [MainWindow]
                                      |
                              [PlotWidget] --QPainter--&gt; [UI 显示]
                                      |
                                [CSV 日志]

## 核心实现细节

- **V4L2 高效采集**：使用 4 缓冲队列 + `mmap` 零拷贝，避免 `read()` 额外内存拷贝；NV12 通过 OpenCV 转 RGB 后渲染
- **ROI 交互设计**：基于 `eventFilter` 拦截 `QLabel` 鼠标事件，实现图像坐标与控件坐标的双向映射，支持动态框选与实时回显
- **模板匹配优化**：限定搜索区域为 ROI 邻域（±80px），避免全图遍历；匹配成功后自动更新 ROI 位置实现跟踪
- **跨线程数据流**：视频采集/检测逻辑通过 Qt 信号槽（`targetUpdated`）与 UI 主线程解耦，避免界面卡顿
- **自绘曲线控件**：`PlotWidget` 基于 `QPainter` 自绘，支持多靶标、多时间窗口、自动量程缩放，零第三方依赖

## 编译运行

### 环境要求

- OS: Linux (Ubuntu 20.04+ / 嵌入式 Buildroot)
- Arch: ARM64 (RK3576) / x86_64
- Qt: 5.15+ / 6.x
- OpenCV: 4.5+

### 编译

# 1. 若使用 RK SDK 中的 OpenCV，先设置环境变量
export RK_SDK_PATH=/path/to/rk3576-sdk/.../3rdparty

# 2. 构建
cd bridge_monitor
qmake bridge_monitor.pro
make -j$(nproc)

# 3. 运行（需相机设备 /dev/video33）
./bridge_monitor

若在其他 Linux 平台编译，请确保 pkg-config 能找到 OpenCV，或手动修改 .pro 中的 RK_SDK_PATH。



## 使用说明

1. **打开相机**：点击"打开相机"，等待图像显示
2. **框选靶标**：在图像上按住鼠标左键拖拽框选黑白标靶区域，输入名称后保存
3. **开始监测**：点击"开始监测"，程序自动跟踪靶标并计算位移
4. **零点校准**：移动靶标到基准位置后，点击"零点校准"
5. **查看曲线**：下方实时显示 X（实线）/ Y（虚线）位移曲线
6. **数据导出**：监测开始后自动保存 CSV 到程序目录
7. **靶标管理**：左侧菜单切换"靶标管理"，可修改像素比例、更新参考图像等

## 项目结构

| 文件 | 职责 |
|------|------|
| `videowidget.cpp/h` | V4L2 采集、OpenCV 模板匹配、ROI 交互、CSV 导出 |
| `plotwidget.cpp/h` | 实时位移曲线自绘控件（QPainter） |
| `targetmanager.cpp/h` | 靶标列表管理、ROI 框选、配置导入导出 |
| `target.h` | 靶标数据结构（ROI、模板图、位移、像素比等） |
| `mainwindow.cpp/h/ui` | 主界面布局、页面切换、信号连接 |
| `style.qss` | 深色工业主题样式表 |

## 演示截图

<details>
<summary>点击查看更多截图</summary>

![实时监控主界面](assets/screenshot_main.png)
![靶标管理界面](assets/screenshot_target_manager.png)
![ROI框选过程](assets/screenshot_roi_select.png)
![靶标跟踪特写](assets/screenshot_tracking.png)
![CSV数据文件](assets/screenshot_csv.png)

</details>

## 注意事项

- 默认相机设备 `/dev/video33`，如需修改请编辑 `videowidget.cpp`
- 默认分辨率 1920×1080，格式 NV12
- 模板匹配阈值 0.55，可根据实际场景调整
- 像素比例默认 0.05 mm/pixel，请在靶标管理中按实际标定值修改
    


## 后续优化方向
- 模板匹配迁移至 RKNN NPU 推理，降低 CPU 占用
- 引入多线程解码，分离 V4L2 采集与算法检测线程
- 增加 SQLite 本地数据库替代 CSV，支持历史数据查询
- 支持 ONVIF 网络相机接入


## License

This project is licensed under the MIT License.


