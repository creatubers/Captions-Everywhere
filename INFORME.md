# Validación — 24 de octubre de 2026

## Estado entregado

Versión 1.2.2. Reconocimiento principal **Parakeet TDT 0.6B v3 INT8**, local en CPU, idioma automático. Whisper base/small siguen disponibles para selección manual de español/inglés. La sustitución del modelo principal responde a los problemas de precisión observados con la primera implementación.

La versión inicial confirmaba segmentos tras 650 ms de silencio, descartaba el contexto entre segmentos y utilizaba un contexto acústico reducido de Whisper. Esas decisiones se han retirado. La versión final mantiene revisable una ventana de hasta 20 s; las hipótesis reemplazan el bloque anterior y una pausa de 0,9 s provoca una revisión, sin congelarlo. Se cierra tras una pausa de 6 s o al alcanzar el límite de la ventana. La captura conserva un segundo previo a la detección de voz para reducir pérdidas al principio de las palabras.

## Entorno físico

Ubuntu 26.10, KDE/Wayland, Ryzen 7 5825U, PipeWire 1.6.8 con compatibilidad PulseAudio, OBS 32.2.2. Se compiló contra Ubuntu 22.04.5, GCC 11.4 y Qt 6.2.4. Se incluyen bibliotecas y cargador glibc de ese entorno.

## Comprobaciones realizadas

| Comprobación | Resultado y alcance |
|---|---|
| Ventana Wayland | Abre y dibuja la interfaz en el equipo físico. Controles de entrada, idioma y modelo disponibles. |
| X11 | Arranque y captura de imagen mediante XWayland en este equipo. No equivale a un escritorio X11 independiente. |
| Ubuntu Base 22.04.5 | Arranque y `--self-test` de la versión final dentro de raíz mínima, con espacio de red aislado. |
| Ubuntu Base 24.04.5 | Mismo arranque/autocomprobación sin red. |
| Ubuntu Base 26.04.1 | Mismo arranque/autocomprobación sin red. |
| Copia portable | Integración pasada desde carpeta con espacios, tildes y `&`; configuración de fuentes regenerada para esa ruta. |
| Interfaces OBS | HTTP, WebSocket, instantánea inicial, tipografía incluida, escritura atómica y sustitución de una hipótesis por texto corregido verificados. |
| Instancia/permisos | Segunda instancia rechazada; carpeta sin escritura diagnosticada. |
| Audio | Captura real mediante fuente monitor PulseAudio/PipeWire, usando un dispositivo virtual de prueba independiente. |
| Texto FreeType de OBS | Fuente creada y capturada en OBS, leyendo `captions.txt`, en un perfil de prueba separado. |
| Grabación OBS | 163,3 s, 1280×720 a 30 fps; sin fotogramas perdidos en las métricas consultadas. Prueba hecha antes de sustituir el modelo principal; no demuestra estabilidad prolongada de Parakeet. |

La prueba originalmente planteada de 30 minutos se canceló al indicarlo el usuario. No se presenta como completada.

## Voz y tiempos

Se reprodujo en tiempo real una muestra de voz real en español de 5,329 s, distribuida entre los ejemplos de los modelos upstream, convertida a PCM16 mono de 16 kHz. Texto final de Parakeet:

> No preguntes qué puede hacer tu país por ti, pregunta qué puedes hacer tú por tu país.

En la primera revisión, con el modelo cargado, el primer resultado apareció a **1,927 s**. Tras reducir el umbral inicial de audio de 1,5 a 0,8 s y el intervalo de revisión de 1,25 a 0,65 s, el primer resultado de la misma muestra apareció a **0,938 s** en la versión 1.2.1. Los cálculos de las hipótesis tardaron aproximadamente **0,31–1,06 s** en esa prueba. En la versión 1.2.1 hubo revisiones a 0,94; 1,81; 2,61; 3,42 y 4,22 s, entre otras, incluidos cambios de puntuación y de «que» a «qué». La hipótesis final se mantuvo correcta en esta muestra.

Estos tiempos **excluyen la carga inicial del modelo**, que requiere varios segundos. La aplicación ahora carga el modelo al abrirse o seleccionarlo y muestra cuándo está listo. No se promete ese tiempo para otras CPU, audios o ventanas de mayor duración.

También se evaluaron una grabación real en inglés y voz sintética en español durante el desarrollo. La voz sintética produjo errores importantes con varios modelos; no se utiliza como demostración de precisión. No hay un corpus amplio ni un cálculo formal de tasa de error. Una frase correcta no certifica una precisión general equivalente a Windows 11.

## Pendiente de validar

- Escritorios completos e independientes de Ubuntu/Kubuntu 22.04, 24.04 y 26.04; reglas de ventana de cada compositor.
- Renderizado de la fuente Navegador dentro de OBS: el paquete instalado no incluye ese módulo. El navegador integrado disponible para las pruebas tampoco pudo acceder al servidor local del equipo; se verificaron HTTP y WebSocket mediante clientes locales.
- Inferencia física en CPU antiguas sin AVX2. Se incluye el backend genérico de Whisper; ONNX Runtime y Parakeet deben validarse en esos equipos.
- Desconexiones físicas de micrófonos, cambios repetidos Bluetooth, múltiples usuarios/sesiones y pruebas largas de memoria.
- Música, ruido intenso, conversaciones simultáneas, nombres propios y variaciones regionales del español en una colección representativa.

La carpeta está preparada para las versiones indicadas, pero estas comprobaciones no certifican funcionamiento perfecto en cualquier instalación Linux ni en futuras versiones.

## Aviso de KDE investigado

El registro de KDE/systemd identifica un SIGSEGV a las 11:40:38 del proceso 57680, ejecutado desde la **carpeta de prueba**, con `--headless --test-wav .../jfk.wav`; no era la ventana principal. Se detectó que el preparador de pruebas compartía bibliotecas mediante enlaces duros y después copiaba archivos sobre esos mismos inodos mientras la prueba seguía ejecutándose.

Se cambió la preparación para copiar físicamente ejecutables y bibliotecas; las actualizaciones utilizan sustitución atómica. El empaquetador rechaza sobrescribir una aplicación existente. Se añadió un registro operativo sin transcripciones y una interfaz local `/health`. La repetición de la prueba con bibliotecas independientes terminó con código 0, reconoció correctamente la grabación inglesa y no produjo otro informe de fallo en KDE. La aplicación principal también cerró normalmente con código 0 cuando se comprobó su salida.

## Identidad del proyecto

El proyecto y el programa se han renombrado a **Captions Everywhere**. Se incorpora el PNG original facilitado por el usuario, sin modificarlo, como logo de la cabecera, icono de la ventana y favicon de la página de OBS. El ejecutable se llama `captions-everywhere`. Se conserva un enlace a la carpeta anterior para las rutas de OBS existentes.

## Distribución AppImage 1.3.0

Se separan recursos y estado para admitir un sistema de archivos de solo lectura. La versión en carpeta sigue utilizando `state/`; el AppImage utiliza XDG_CONFIG_HOME o una carpeta lateral portable. El identificador enviado a Wayland es `captions_everywhere`, coincidente con la entrada de KDE y con Gear Lever. Se añaden metadatos de actualización de GitHub Releases, publicación de `.zsync` y flujo de compilación de Releases.

El AppImage 1.3.0 pasó el arranque mediante FUSE y mediante extracción, y reconoció correctamente la muestra real en español con la carpeta de datos externa. También pasó `--self-test` en una raíz mínima Alpine Linux 3.24.3, sin KDE ni Qt del sistema y con la red aislada; esta prueba solo cubre arranque/salida y no equivale a probar un escritorio Alpine completo. Gear Lever 4.6.2 detectó `GithubUpdater` y los metadatos incrustados durante la integración.
