# Copyright (C) 201 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS SOFTWARE,
# YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FROM ROCKCHIP IS PROVIDED
# TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL WARRANTIES AND
# REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS, IMPLIED, STATUTORY
# OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
# NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR
# A PARTICULAR PURPOSE.
# Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
# non-transferable license (a) to install, save and use the Software; (b) to copy
# and distribute the Software in binary code format only.
# Except as expressively authorized by Rockchip in writing, you may NOT: (a) distribute
# the Software in source code; (b) distribute on a standalone basis but you may distribute
# the Software in conjunction with platforms incorporating Rockchip integrated circuits;
# (c) modify the Software in whole or part;(d) decompile, reverse-engineer, dissemble,
# or attempt to derive any source code from the Software;(e) remove or obscure any copyright,
# patent, or trademark statement or notices contained in the Software.
LOCAL_PATH:= $(call my-dir)

LIBCAMERA360_WITH_SRC := false

ifeq ($(LIBCAMERA360_WITH_SRC), true)
include $(call all-subdir-makefiles)

PRJ_CPPFLAGS := -DANDROID_OS
PRJ_CPPFLAGS += -Wno-error=unused-variable
PRJ_CPPFLAGS += -Wno-error=unused-parameter
PRJ_CPPFLAGS += -Wno-error=ignored-qualifiers
PRJ_CPPFLAGS += -Wno-error=address-of-packed-member
PRJ_CPPFLAGS += -Wno-error=overloaded-virtual
PRJ_CPPFLAGS += -Wno-error=unused-private-field
PRJ_CPPFLAGS += -Wno-error=zero-length-array
PRJ_CPPFLAGS += -Wno-error=gnu-include-next
PRJ_CPPFLAGS += -Wno-error=c11-extensions
PRJ_CPPFLAGS += -Wno-error=macro-redefined
PRJ_CPPFLAGS += -Wno-error=gnu-zero-variadic-macro-arguments
PRJ_CPPFLAGS += -Wno-error=unused-function

include $(CLEAR_VARS)

uses-permission android:name="android.permission.ACCESS_SURFACE_FLINGER"
uses-permission android:name="android.permission.CAMERA"

LOCAL_SRC_FILES:=\
    MessageQueue.cpp\
    Semaphore.cpp\
    CameraBuffer.cpp\
    DisplayAdapter.cpp\
    ProcessAdapter.cpp\
    CameraAdapter.cpp\
    CameraSocAdapter.cpp\
    CameraHal.cpp\
    CameraHal_Tracer.c\
    RgaCropScale.cpp\
    ExternalCameraMemManager.cpp\
    surround_3d.cpp \
    display/vop_buffers.c \
    display/vop_args.c \
    display/vop_info.c \
    display/vop_set.c \
    display/util/format.c \
    display/util/kms.c \
    display/util/pattern.c \
    main.cpp


ifeq ($(TARGET_RK_GRALLOC_VERSION),4)
    LOCAL_CFLAGS += -DRK_GRALLOC_4
    LOCAL_SRC_FILES += \
        ExternalCameraGralloc4.cpp
else
    LOCAL_SRC_FILES += \
        ExternalCameraGralloc.cpp
endif

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/inc/opencv34 \
    $(LOCAL_PATH)/include \
    frameworks/base/include/ui \
    frameworks/av/include \
    frameworks/native/include \
    hardware/libhardware/include \
    external/tinyxml2 \
    system/media/camera/include \
    system/core/liblog/include \
    system/core/libion/include/ion \
    system/core/libion/kernel-headers/linux \
    hardware/rockchip/librga \
    external/libdrm \
    external/libdrm/include/drm \
    system/core/include/util \
    external/skia/include \
	external/libxml2 \
	external/libxml2/include \
	external/libxml2/include/libxml

# API 29 -> Android 10.0
ifneq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \< 29)))

ifneq (,$(filter mali-tDVx mali-G52, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/bifrost \
        hardware/rockchip/libgralloc/bifrost/src
endif

ifneq (,$(filter mali-t860 mali-t760, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/midgard
endif

ifneq (,$(filter mali400 mali450, $(TARGET_BOARD_PLATFORM_GPU)))
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc/utgard
endif
else
LOCAL_C_INCLUDES += \
        hardware/rockchip/libgralloc

endif


LOCAL_C_INCLUDES += \
    system/core/include \
    frameworks/base/include \
    frameworks/base/core/jni/android/graphics \
    frameworks/libs/androidfw/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/arect/include \
    external/skia \
    external/skia/include \
    external/skia/include/core \
    external/skia/include/effects \
    external/skia/include/images \
    external/skia/src/ports \
    external/skia/include/utils \
    external/skia/android \
    external/expat/lib

LOCAL_HEADER_LIBRARIES += \
    libutils_headers \
    libcutils_headers \
    libhardware_headers \
    liblog_headers \
    libgui_headers \
    libbinder_headers \
    android.hardware.graphics.common@1.2 \
    android.hardware.graphics.common-V2-ndk_platform\
    android.hardware.graphics.mapper@4.0\
    android.hardware.graphics.allocator@4.0\
    libgralloctypes

LOCAL_STATIC_LIBRARIES += \
    android.hardware.camera.common@1.0-helper\
    libgrallocusage

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libbinder \
    libutils \
    libcutils \
    libcamera_client \
    libgui\
    libjpeg\
    libion\
    libdl\
    libexpat \
    libhardware \
    liblog \
    libdrm \
    libgralloctypes \
    libcamera_metadata\
    libfmq\
    libsync\
    libyuv\
    libjpeg\
    libexif\
    libtinyxml2\
    librga\
    libbase\
    libGLESv2\
    libEGL\
    android.hardware.graphics.allocator@2.0\
    android.hardware.graphics.allocator@3.0\
    android.hardware.graphics.allocator@4.0\
    android.hardware.graphics.common-V2-ndk_platform\
    android.hardware.graphics.common@1.2\
    android.hardware.graphics.mapper@2.0\
    android.hardware.graphics.mapper@2.1\
    android.hardware.graphics.mapper@3.0\
    android.hardware.graphics.mapper@4.0\
    libhidlbase

LOCAL_SYSTEM_SHARED_LIBRARIES := \
        libc \
        libm

LOCAL_LDLIBS := -ljnigraphics

LOCAL_CFLAGS += -Wno-error=unused-function -Wno-array-bounds -Wno-error -Wno-error=macro-redefined
LOCAL_CFLAGS += -DLINUX  -D_FILE_OFFSET_BITS=64 -DHAS_STDINT_H -DENABLE_ASSERT
LOCAL_CFLAGS += $(PRJ_CPPFLAGS)
LOCAL_CPPFLAGS += -frtti -Wno-error -std=c++1z -Wno-error=macro-redefined -fexceptions
LOCAL_CPPFLAGS +=  -DLINUX  -DENABLE_ASSERT
LOCAL_CPPFLAGS += $(PRJ_CPPFLAGS)

LOCAL_LDFLAGS_$(TARGET_2ND_ARCH) := $(LOCAL_PATH)/rkrender/lib32/libopencv_world.so $(LOCAL_PATH)/rkrender/lib32/libassimp.so $(LOCAL_PATH)/rkrender/lib32/lib_render_3d.so $(LOCAL_PATH)/rkrender/lib32/libdmabufheap.so
LOCAL_LDFLAGS_$(TARGET_ARCH) := $(LOCAL_PATH)/rkrender/lib64/libopencv_world.so $(LOCAL_PATH)/rkrender/lib64/libassimp.so $(LOCAL_PATH)/rkrender/lib64/lib_render_3d.so $(LOCAL_PATH)/rkrender/lib64/libdmabufheap.so

LOCAL_MULTILIB := 64

LOCAL_MODULE:=camera360

include $(BUILD_SHARED_LIBRARY)
endif
