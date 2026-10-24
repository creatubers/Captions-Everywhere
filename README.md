# Captions Everywhere

<img src="resources/logo.png" alt="Logo de Captions Everywhere" width="128" />

Subtítulos en directo para Linux y OBS, sin dependencia de KDE, con reconocimiento local y funcionamiento offline. Incluye una ventana flotante, Parakeet para reconocimiento multilingüe y modelos Whisper con selección manual de idioma.

**[Descargar AppImage](https://github.com/creatubers/Captions-Everywhere/releases/latest)** · **[Guía de AppImage y Gear Lever](APPIMAGE.md)**

Clonar el repositorio descarga el código fuente; para ejecutarlo directamente, utiliza el AppImage de Releases o compílalo siguiendo la guía.

El AppImage incluye los modelos y permite comprobar actualizaciones desde Gear Lever. Si utilizas la distribución en carpeta, ejecuta **`Iniciar.sh`**.

- [Descarga, actualizaciones y modo portable del AppImage](APPIMAGE.md)
- [Guía de uso y configuración de OBS](LEEME.md)
- [Compilación y estructura del proyecto](source/COMPILAR.md)
- [Pruebas y límites conocidos](INFORME.md)
- [Atribuciones de terceros](licenses/ATRIBUCIONES.md)

El logo se incorpora tal como lo ha proporcionado el usuario. El código de la aplicación tiene licencia MIT; las bibliotecas y modelos conservan sus licencias respectivas.

## Compatibilidad entre distribuciones

El AppImage está diseñado para funcionar en distribuciones Linux **x86_64** sobre X11 o Wayland. Incluye Qt y las bibliotecas de ejecución; no necesita KDE, ni Qt instalado, ni Gear Lever para funcionar. El audio requiere PulseAudio o la compatibilidad PulseAudio de PipeWire.

GNOME, KDE Plasma, XFCE, Cinnamon y otros escritorios pueden utilizar los protocolos que emplea la aplicación, aunque no se han probado todas sus versiones. Las funciones de ventana —como mantener encima— dependen del gestor de ventanas. AppImage no garantiza compatibilidad con absolutamente cualquier kernel, arquitectura o configuración Linux: consulta las pruebas reales y los límites en `INFORME.md`.
