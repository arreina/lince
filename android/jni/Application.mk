# Los móviles de ahora son de 64 bits (arm64-v8a); x86_64, para el emulador
APP_ABI      := arm64-v8a x86_64
APP_PLATFORM := android-21
APP_STL      := c++_static
APP_MODULES  := main SDL2
# Bibliotecas que funcionan en móviles con páginas de memoria de 16 KB (Google
# Play lo pide a las apps que apuntan a Android 15 o más)
APP_LDFLAGS  := -Wl,-z,max-page-size=16384
