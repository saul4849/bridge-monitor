QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

# ============================================================
# OpenCV 路径配置（针对 RK3576 交叉编译环境）
# 
# 编译前请设置环境变量：
#   export RK_SDK_PATH=/path/to/rk3576-sdk/.../3rdparty
# 
# 如果未设置，将使用下方默认路径作为 fallback
# ============================================================
RK_SDK_PATH = $$(RK_SDK_PATH)
isEmpty(RK_SDK_PATH) {
    RK_SDK_PATH = /home/saul/rk3576-sdk/rk3576-linux-SDK-20260616/external/rknpu2/examples/3rdparty
    message(">>> RK_SDK_PATH not set, using default: $$RK_SDK_PATH")
}

OPENCV_INCLUDE = $$RK_SDK_PATH/opencv/opencv-linux-aarch64/include
OPENCV_LIB     = $$RK_SDK_PATH/opencv/opencv-linux-aarch64/lib

INCLUDEPATH += $$OPENCV_INCLUDE
LIBS += -L$$OPENCV_LIB \
        -lopencv_core \
        -lopencv_imgproc

# 如果目标平台支持 pkg-config，可简化为：
# CONFIG += link_pkgconfig
# PKGCONFIG += opencv4

SOURCES += \
    calibrationmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    plotwidget.cpp \
    slidingaverage.cpp \
    statusanalyzer.cpp \
    targetmanager.cpp \
    videowidget.cpp

HEADERS += \
    calibrationmanager.h \
    mainwindow.h \
    plotwidget.h \
    slidingaverage.h \
    statusanalyzer.h \
    target.h \
    targetmanager.h \
    videowidget.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    res.qrc

# 部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target