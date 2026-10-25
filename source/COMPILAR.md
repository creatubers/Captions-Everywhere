# Compilar y empaquetar

El programa entregado no necesita Python. Python 3 se utiliza únicamente para empaquetar y ejecutar comprobaciones de desarrollo.

## Entorno usado

Ubuntu Base 22.04.5 x86-64; GCC 11.4; Qt 6.2.4; CMake 3.22; Ninja; libpulse. Se compiló dentro de un entorno Jammy aislado en la carpeta de trabajo, sin instalar herramientas en el sistema del usuario.

En un entorno de desarrollo Ubuntu 22.04, prepara estas dependencias:

```
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-wayland libpulse-dev libgl-dev libegl-dev libxkbcommon-dev libvulkan-dev python3 fonts-dejavu-core xkb-data
```

Desde `source/`:

```
./scripts/build.sh /ruta/al/build
python3 scripts/package.py /ruta/al/build /ruta/a/Captions-Everywhere
```

El directorio destino debe tener `Iniciar.sh`, `resources/obs.html`, `models/` y `state/`, procedentes del paquete. El empaquetador recoge la clausura de dependencias ELF, el cargador glibc, complementos de Qt, fuentes, teclado y licencias. Todas las dependencias deben recogerse en el entorno Ubuntu 22.04, nunca con `ldd` de una distribución más reciente.

## Versiones y fuentes

- Aplicación: C++17 y Qt Widgets/Network, código en `src/`. Licencia MIT.
- whisper.cpp: v1.8.3, fuente completa en `vendor/whisper.cpp`; revisión exacta en `WHISPER_COMMIT`. Backend CPU con selección automática de variantes, incluida x64 genérica. GPU y OpenMP desactivados.
- sherpa-onnx: v2.0.0. Cabecera C y bibliotecas oficiales en `vendor/sherpa/`. Fuente: https://github.com/k2-fsa/sherpa-onnx/tree/v2.0.0 ; paquete usado: https://github.com/k2-fsa/sherpa-onnx/releases/download/v2.0.0/sherpa-onnx-v2.0.0-linux-x64-shared-no-tts-lib.tar.bz2 . Para reconstruir esas bibliotecas sigue el CMake y las instrucciones de esa revisión, con bibliotecas compartidas y sin TTS.
- ONNX Runtime 1.28.2: incluido en ese paquete oficial; licencia MIT y avisos de terceros en `licenses/`. No requiere GPU.
- Parakeet TDT 0.6B v3 INT8: modelo de NVIDIA, convertido por sherpa-onnx. https://huggingface.co/nvidia/parakeet-tdt-0.6b-v3 ; exportación: https://github.com/k2-fsa/sherpa-onnx/tree/v2.0.0/scripts/nemo/parakeet-tdt-0.6b-v3 ; licencia CC-BY-4.0. Se incluye sin entrenamiento adicional, con los cambios de formato y cuantización de la exportación upstream.
- Whisper base/small Q5_1: https://huggingface.co/ggerganov/whisper.cpp ; pesos originales de OpenAI Whisper, licencia MIT.
- Silero VAD v5.1.2: exportación GGML de https://huggingface.co/ggml-org/whisper-vad ; fuente original https://github.com/snakers4/silero-vad .
- Qt y las bibliotecas Ubuntu están vinculadas dinámicamente. Versiones exactas y licencias en `licenses/ubuntu-package-versions.txt` y `licenses/ubuntu/`. Se pueden sustituir las bibliotecas compatibles dentro de `lib/` y `plugins/`. Las fuentes correspondientes a los paquetes Ubuntu se obtienen con `apt-get source <paquete>` habilitando `deb-src` de Jammy; Qt upstream: https://download.qt.io/archive/qt/6.2/6.2.4/submodules/ .

La aplicación no accede a servicios de reconocimiento ni descarga modelos durante su ejecución.

## Comprobaciones breves

```
QT_QPA_PLATFORM=offscreen ../Iniciar.sh --self-test
python3 tests/integration.py ..
```

La integración comprueba una copia en una ruta con espacios, tildes y `&`, HTTP, WebSocket, tipografía incluida, bloqueo de una segunda instancia y rechazo de una carpeta sin permisos de escritura.

Para evaluar audio propio sin capturar un micrófono, usa un WAV PCM16 mono de 16 kHz:

```
QT_QPA_PLATFORM=offscreen ../Iniciar.sh --headless --test-wav /ruta/voz.wav --model parakeet --language es --quit-after 25
```

Este modo imprime hipótesis y tiempos por stdout, carga el modelo antes de alimentar el WAV y permite comparar las revisiones. Parakeet detecta el idioma automáticamente; `--language es/en` fija el idioma únicamente en los modos Whisper.

## Interfaces locales

`GET /health`: estado operativo, modelo y último error, sin transcripciones. `GET /obs`: página transparente. `GET /font.ttf`: tipografía. `GET /state`: instantánea JSON. `GET /ws`: actualización WebSocket. La instantánea contiene `confirmed`, `provisional`, `text` (últimas líneas para FreeType), `style` y `sequence`. Son interfaces locales de lectura; la captura y la configuración se controlan desde la ventana.

Empaqueta siempre en un destino nuevo. No modifiques ejecutables ni bibliotecas de una copia en ejecución. Las pruebas copian físicamente su entorno de ejecución; solo los modelos de solo lectura pueden compartir inodos.

## AppImage y publicación

Consulta `../APPIMAGE.md`. La versión 1.3.0 separa los recursos de solo lectura de la carpeta de estado, proporciona metadatos de escritorio para Wayland y añade el flujo de GitHub Releases con información de actualización compatible con Gear Lever.

## Comprobaciones breves de las funciones de 1.4

En el entorno de compilación, activa `-DCE_BUILD_CHECKS=ON` con CMake y ejecuta `QT_QPA_PLATFORM=offscreen build/captions-feature-checks`. Comprueba en segundos el cambio de interfaz y su persistencia, restauración/reconexión de entrada, menú de modelos disponibles y guardado de correcciones, última frase, pausa y errores de escritura. No carga modelos ni captura audio.
