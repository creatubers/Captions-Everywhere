# Captions Everywhere 1.2.2

Subtítulos locales para Linux y OBS. Incluye los modelos y el entorno de ejecución; no necesita internet, cuentas, Python, instalación ni una GPU.

## Abrir y usar

1. Copia **toda esta carpeta** a una ubicación donde tu usuario pueda escribir.
2. Ejecuta `Iniciar.sh`. Si el gestor de archivos lo abre como texto, usa «Ejecutar» o abre una terminal en esta carpeta y ejecuta `./Iniciar.sh`.
3. Espera a «Modelo listo». La primera carga de Parakeet requiere varios segundos y aproximadamente 1 GB de memoria.
4. Elige «Entrada de audio»: audio del sistema, micrófono predeterminado o un micrófono concreto.
5. Pulsa «Iniciar». «Pausar» detiene la captura. Minimizar mantiene el reconocimiento y la salida de OBS activos; cerrar termina la aplicación.

Si el sistema de archivos elimina el permiso de ejecución, usa `chmod +x Iniciar.sh`. Una unidad montada con `noexec` requiere copiar la carpeta a una ubicación que permita ejecutar programas.

## Idioma y precisión

El control **Idioma del audio** está debajo de la entrada, claramente etiquetado.

- **Precisión en directo · Parakeet** es la opción principal. Detecta el idioma automáticamente, incluidos español e inglés. Se muestra el indicador «Detección automática» porque este modelo no ofrece una selección manual de idioma.
- Para **fijar Español o Inglés**, elige uno de los modelos Whisper y usa «Idioma del audio».
- **Whisper · mayor precisión** usa `small`; exige más CPU y puede tardar varios segundos entre revisiones.
- **Rápido · base** reduce el consumo, con una precisión inferior.

La aplicación vuelve a reconocer un bloque de audio de hasta 20 segundos. Las hipótesis nuevas **reemplazan** el texto anterior de ese bloque: las palabras pueden cambiar al llegar más contexto. Una pausa corta no congela una frase. El texto anterior se conserva como contexto explícito para Whisper. Parakeet vuelve a procesar el contexto acústico reciente y añade puntuación.

El reconocimiento no equivale al motor de Windows 11. La precisión depende del idioma, la voz, el ruido y el modelo. No se ha validado con una colección amplia de conversaciones, música ni todas las variantes del español.

## OBS que tienes instalado: Texto FreeType 2

1. Añade una fuente **Texto (FreeType 2)**.
2. Activa **Leer desde archivo** y elige `state/captions.txt` dentro de esta carpeta.
3. Selecciona la tipografía, tamaño, contorno y ubicación en OBS.

«Ajustes» permite copiar la ruta completa del archivo. La aplicación escribe el archivo de forma atómica, limitado al número de líneas configurado. Los colores y la tipografía de esta fuente se ajustan en OBS. Si mueves la carpeta, actualiza la ruta de la fuente en OBS.

## OBS con fuente Navegador

Añade una fuente **Navegador** con URL `http://127.0.0.1:8765/obs`, ancho 1920 y alto 1080. El fondo de la página es transparente; el fondo del recuadro y su opacidad se cambian desde «Ajustes». El tipo de letra está incluido en el paquete. La página se reconecta al reiniciar la aplicación.

Tu paquete de OBS 32.2.2 no incluye el módulo Navegador; la opción FreeType funciona sin añadirlo. La página HTTP y su WebSocket están disponibles, pero no se ha validado su renderizado dentro de ese módulo de OBS.

Si el puerto está ocupado, cámbialo en «Ajustes» y actualiza la URL en OBS. Solo se escucha en `127.0.0.1`; no se publica nada en la red local. HTTP y WebSocket comparten el mismo puerto.

## Ventana encima de otras ventanas

Activa «Mantener encima» en Ajustes. En X11 se solicita al gestor de ventanas. En Wayland decide el compositor:

- KDE/Kubuntu: menú de la ventana → Más acciones → Mantener encima. Para recordarlo: Más acciones → Configurar opciones especiales de la ventana → Añadir propiedad → Mantener encima → Aplicar inicialmente/Sí. Identifica la ventana por el título `Captions Everywhere`.
- GNOME: menú de la ventana → Siempre encima, cuando esté disponible.

Los nombres concretos del menú pueden variar entre versiones. La ventana sigue siendo redimensionable y se mueve con la barra de título del escritorio.

## Portabilidad y privacidad

La carpeta incluye ejecutable C++, Qt 6, bibliotecas de audio, ONNX Runtime, sherpa-onnx, whisper.cpp, variantes de CPU, tipografía, datos de teclado y modelos. En la distribución en carpeta, la configuración y la caché de fuentes se escriben en `state/`. El AppImage utiliza una carpeta de datos externa y estable; consulta `APPIMAGE.md`. Las rutas absolutas que aparecen en la configuración de fuentes se regeneran en cada arranque para la ubicación actual.

El sistema receptor debe ser Linux x86-64, tener un escritorio X11/Wayland y PulseAudio o la compatibilidad PulseAudio de PipeWire funcionando. No se exige AVX2 para la variante genérica de Whisper. La inferencia ONNX en CPU antiguas no se ha verificado físicamente. OBS se instala y ejecuta por separado.

No se graba audio ni se guarda un historial de transcripciones. Los errores operativos se anotan en `state/diagnostics.log`, con rotación para limitar su tamaño, sin texto reconocido. `state/captions.txt` contiene únicamente el texto reciente para OBS y se vacía al pausar o cerrar normalmente. Después de una terminación forzada puede quedar el último texto hasta el siguiente arranque. Los eventos y transcripciones por terminal solo se activan con `--diagnostics` o los modos de prueba.

## Problemas frecuentes

- **No hay audio:** comprueba la entrada seleccionada y el volumen. La salida del sistema sigue la salida predeterminada, incluidos cambios a auriculares. Una entrada desconectada permanece identificada como tal.
- **Permiso de escritura:** copia la carpeta a una ubicación de tu usuario. No hace falta `sudo`.
- **Falta un modelo:** vuelve a copiar la carpeta completa. La aplicación no descarga archivos automáticamente.
- **Ya está abierto:** busca la ventana existente. Cada copia de la carpeta permite una instancia.
- **El equipo va lento:** el programa limita la cola de audio y avisa si tiene que omitir audio antiguo.
- **Problema con Wayland:** prueba desde terminal `QT_QPA_PLATFORM=xcb ./Iniciar.sh` si el sistema dispone de XWayland.

La carpeta anterior `LiveCaptions-Portable` se mantiene como enlace de compatibilidad para las fuentes de OBS ya configuradas. El programa y la carpeta principal se llaman **Captions Everywhere**.

Código y compilación: `source/COMPILAR.md`. Validaciones y límites: `INFORME.md`. Atribuciones y licencias: `licenses/`.
