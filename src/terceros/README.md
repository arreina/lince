# Código de terceros

Código ajeno incluido tal cual en el repositorio, para que Lince siga sin
dependencias para compilar.

| Archivo | Qué es | Versión | Licencia |
|---------|--------|---------|----------|
| `stb_vorbis.c` | Decodificador de Ogg Vorbis de Sean Barrett ([nothings/stb](https://github.com/nothings/stb)) | 1.22 | Dominio público o MIT (a elegir) |
| `stb_image.h` | Lector de PNG, JPG, BMP, GIF y TGA, del mismo autor y repositorio | 2.30 | Dominio público o MIT (a elegir) |

Lince los usa a través de `src/ogg.c` (sonidos y música `.ogg` con
`juego.sonido()`) y `src/imagen.c` (imágenes con `juego.imagen()`). No se
modifican: para actualizarlos, se sustituye el archivo por la nueva versión.
