# 🐆 Lince para Android: libmain.so (todo Lince) y libSDL2.so (de las fuentes
# de SDL2, que no van en el repositorio: SDL_RUTA las señala). Lo usa
# android/construir.sh.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := main
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/../../src/*.c)
LOCAL_CFLAGS    := -O2 -Wall -Wno-unused-parameter -Wno-sign-compare -fvisibility=default
LOCAL_LDLIBS    := -llog -ldl -lm
include $(BUILD_SHARED_LIBRARY)

include $(SDL_RUTA)/Android.mk
