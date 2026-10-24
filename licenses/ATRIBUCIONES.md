# Atribuciones

- **NVIDIA Parakeet TDT 0.6B v3**, NVIDIA: https://huggingface.co/nvidia/parakeet-tdt-0.6b-v3 . Modelo original CC-BY-4.0. Conversión y cuantización INT8 de k2-fsa/sherpa-onnx: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8.tar.bz2 . Se redistribuye esa exportación, sin entrenamiento adicional. Licencia: `Parakeet-CC-BY-4.0.txt`.
- **sherpa-onnx v2.0.0**, Xiaomi y contribuidores, Apache-2.0. Biblioteca C oficial de CPU: https://github.com/k2-fsa/sherpa-onnx/tree/v2.0.0 .
- **ONNX Runtime**, Microsoft y contribuidores, MIT. Biblioteca incluida en el paquete oficial sherpa-onnx; avisos de terceros adjuntos.
- **Whisper**, OpenAI, y **whisper.cpp**, ggml-org y contribuidores, MIT. Se incluyen modelos multilingües base/small Q5_1 y el código de whisper.cpp v1.8.3.
- **Silero VAD**, Silero Team, MIT; conversión GGML upstream de whisper.cpp.
- **Qt 6.2.4**, The Qt Company y contribuidores. Se distribuye dinámicamente bajo las condiciones aplicables recogidas en `ubuntu/` y `common/`; fuentes e instrucciones de sustitución en `source/COMPILAR.md`.
- **DejaVu Sans**, proyecto DejaVu/Bitstream, licencia incluida en `ubuntu/fonts-dejavu-core/copyright`.
- Las bibliotecas del sistema Ubuntu mantienen las licencias de sus respectivos autores. La lista exacta de paquetes del entorno de empaquetado y los textos se incluyen en esta carpeta.

El proyecto abb128/LiveCaptions sirvió como referencia de comportamiento. No se copió su código.
