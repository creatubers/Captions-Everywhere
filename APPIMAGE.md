# AppImage, GitHub Releases y Gear Lever

## Descarga

La AppImage incluye el programa, sus bibliotecas, Parakeet, Whisper base/small y el detector de voz. El reconocimiento funciona sin conexión y no necesita cuentas ni GPU.

Descarga el archivo `.AppImage` para `x86_64` desde [GitHub Releases](https://github.com/creatubers/Captions-Everywhere/releases/latest), dale permiso de ejecución y ábrelo. También puedes importarlo en Gear Lever para añadirlo al menú de aplicaciones.

El código fuente se publica en el repositorio; los modelos y binarios grandes se distribuyen en Releases, no en el historial de Git.

## Escritorios y distribuciones

No hay una dependencia de KDE. El programa incluye Qt y funciona sobre X11 o Wayland; el registro del icono en KDE solo es integración con ese escritorio. Gear Lever es opcional y también se utiliza fuera de KDE. Esta compilación se dirige a Linux x86_64 con un servicio de audio PulseAudio/PipeWire. La distribución está preparada para ser independiente de la distro, con el alcance de pruebas indicado en `INFORME.md`, sin prometer compatibilidad universal con cualquier sistema Linux.

## Actualizaciones con Gear Lever

El AppImage lleva esta información de actualización incrustada:

```
gh-releases-zsync|creatubers|Captions-Everywhere|latest|Captions-Everywhere-*-x86_64.AppImage.zsync
```

Cada versión publica el `.AppImage`, su archivo `.zsync` y `SHA256SUMS`. Gear Lever puede detectar el origen y comprobar las versiones publicadas en GitHub. Si lo configuras manualmente:

- Repositorio: `creatubers/Captions-Everywhere`
- Archivo de Release: `Captions-Everywhere-*-x86_64.AppImage`
- Versiones preliminares: desactivadas para seguir el canal estable.

Gear Lever permite comprobar y aplicar actualizaciones según su configuración; esto no implica que sustituya automáticamente el archivo mientras el programa está abierto. Cierra Captions Everywhere antes de aplicar una actualización.

## Configuración y archivo para OBS

Un AppImage es de solo lectura. La configuración y los subtítulos recientes se guardan normalmente en:

```
~/.config/captions-everywhere/
```

Si utilizas `XDG_CONFIG_HOME`, se respeta esa ruta. Las actualizaciones conservan esos archivos. «Ajustes → Copiar ruta de captions.txt» proporciona la ruta exacta para OBS. La fuente Navegador conserva su dirección local habitual.

Para llevar también la configuración contigo, crea una carpeta llamada `Captions-Everywhere.data` junto al AppImage. Al encontrarla, la aplicación la utiliza como carpeta de datos. Copia el AppImage y esa carpeta al otro equipo. También puedes activarlo al primer arranque con:

```
CAPTIONS_EVERYWHERE_PORTABLE=1 ./Captions-Everywhere-1.3.0-x86_64.AppImage
```

`CAPTIONS_EVERYWHERE_STATE_DIR` permite elegir una carpeta de datos concreta.

## Wayland y el icono

El identificador de escritorio es `captions_everywhere`. El AppImage contiene la entrada `.desktop`, el icono y `StartupWMClass=CaptionsEverywhere`. Gear Lever crea una entrada con ese mismo identificador para que KDE pueda asociar correctamente la ventana y el logo.

## Sistemas sin FUSE disponible

Se incluye el runtime oficial de AppImage con libfuse enlazada estáticamente. Sigue siendo necesario que el sistema permita montajes FUSE de usuario. Si no están disponibles, puedes ejecutar:

```
./Captions-Everywhere-1.3.0-x86_64.AppImage --appimage-extract-and-run
```

Esta alternativa extrae temporalmente el contenido y puede tardar más al arrancar. No necesita privilegios de administrador.

## Compilar un AppImage

En Ubuntu 22.04, instala las dependencias indicadas en `source/COMPILAR.md` y, además, `patchelf`, `squashfs-tools`, `zsync` y `desktop-file-utils`. Trabaja con una carpeta de empaquetado nueva:

```
mkdir -p .cache/bundle/resources .cache/bundle/state .cache/tools
cp Iniciar.sh LEEME.md LICENSE .cache/bundle/
cp resources/logo.png resources/obs.html .cache/bundle/resources/
cp -a licenses .cache/bundle/
python3 source/scripts/fetch_dependencies.py --cache .cache/dependencies --bundle .cache/bundle --tools .cache/tools
source/scripts/build.sh "$PWD/.cache/build"
python3 source/scripts/package.py .cache/build .cache/bundle
python3 source/scripts/build_appimage.py --bundle .cache/bundle --output dist --appimagetool .cache/tools/appimagetool.AppImage --runtime .cache/tools/runtime-x86_64
```

Las descargas de compilación están fijadas mediante SHA-256 en `source/dependencies.lock.json`. Si un proveedor cambia un archivo, la compilación se detiene en vez de usar una dependencia distinta silenciosamente.

La acción de GitHub `.github/workflows/release.yml` compila y publica una Release al subir una etiqueta `v*` que coincida con la versión de CMake. También puede ejecutarse manualmente con una etiqueta existente. Para una versión nueva: cambia la versión, confirma los cambios y sube su etiqueta. Gear Lever consultará la nueva Release marcada como más reciente.

## Empaquetado

La AppImage conserva todos los modelos y bibliotecas. Utiliza compresión Zstandard de nivel 19 con bloques de 1 MiB para reducir el tamaño sin modificar los archivos. No hay variantes de producto. Una compilación local no se publica automáticamente en GitHub.

## Transcripciones

El guardado se activa con **Guardar transcripción / Save transcript**. Los TXT UTF-8 se crean en `transcripts/` dentro de la misma carpeta de datos que la configuración. El botón **Abrir transcripciones / Open transcripts** abre esa carpeta. Cada activación crea una sesión nueva; guardar está desactivado al arrancar y no modifica la salida de OBS.

## Idioma de interfaz

El modo predeterminado «Sistema (automático) / System (automatic)» muestra español si el idioma del escritorio es español e inglés en los demás casos. Se comprueba al arrancar. Puedes fijar Español o English desde Ajustes; la elección manual se recuerda. El idioma de reconocimiento del audio es independiente.
