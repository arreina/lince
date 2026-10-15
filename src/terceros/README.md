# Código de terceros

Código ajeno incluido tal cual en el repositorio, para que Lince siga sin
dependencias para compilar.

| Archivo | Qué es | Versión | Licencia |
|---------|--------|---------|----------|
| `stb_vorbis.c` | Decodificador de Ogg Vorbis de Sean Barrett ([nothings/stb](https://github.com/nothings/stb)) | 1.22 | Dominio público o MIT (a elegir) |

Lince lo usa a través de `src/ogg.c` para cargar sonidos y música `.ogg`
con `juego.sonido()`. No se modifica: para actualizarlo, se sustituye el
archivo por la nueva versión.
