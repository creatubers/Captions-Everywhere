#include "i18n.h"
#include <QHash>
#include <QRegularExpression>
#include <atomic>
static std::atomic_bool english{false};
QString systemUiLanguage() {
    QString locale;
    if(qEnvironmentVariableIsSet("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE"))locale=qEnvironmentVariable("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE");
    else for(const char *key:{"LANGUAGE","LC_ALL","LC_MESSAGES","LANG"}) {
        locale=qEnvironmentVariable(key);if(!locale.isEmpty())break;
    }
    const auto primary=locale.section(':',0,0).trimmed().toLower();
    const auto language=primary.split(QRegularExpression("[-_.@]")).value(0);
    return language=="es"?QString("es"):QString("en");
}
void setUiLanguage(const QString &language) {
    const auto resolved=(language=="es"||language=="en")?language:systemUiLanguage();
    english.store(resolved=="en");
}
QString currentUiLanguage(){return english.load()?QString("en"):QString("es");}
QString ui(const char *spanish) {
    if(!english.load())return QString::fromUtf8(spanish);
    static const QHash<QString,QString> catalog={
        {"Los subtítulos aparecerán aquí","Captions will appear here"},
        {"Ajustes","Settings"},
        {"Entrada","Input"},
        {"Audio del sistema","System audio"},
        {"Micrófono predeterminado","Default microphone"},
        {"Idioma del audio","Audio language"},
        {"Español","Spanish"},
        {"Inglés","English"},
        {"Automático","Automatic"},
        {"Detección automática","Automatic detection"},
        {"Modelo","Model"},
        {"Whisper · base","Whisper · base"},
        {"Whisper · small","Whisper · small"},
        {"Parakeet","Parakeet"},
        {"Pausado","Paused"},
        {"Escuchando","Listening"},
        {"Preparando…","Preparing…"},
        {"Listo","Ready"},
        {"▶  Iniciar","▶  Start"},
        {"Ⅱ  Pausar","Ⅱ  Pause"},
        {"Desconectado · ","Disconnected · "},
        {"Guardar transcripción","Save transcript"},
        {"Abrir transcripciones","Open transcripts"},
        {"Guardando","Saving"},
        {"Tamaño de letra","Font size"},
        {"Líneas visibles","Visible lines"},
        {"Opacidad","Opacity"},
        {"Color del texto","Text color"},
        {"Color del fondo","Background color"},
        {"Siempre visible","Always on top"},
        {"Wayland: consulta LEEME.md si necesitas una regla de ventana.","Wayland: see LEEME.md if a window rule is needed."},
        {"Puerto de OBS","OBS port"},
        {"Copiar URL de OBS","Copy OBS URL"},
        {"Copiar ruta de texto","Copy text file path"},
        {"Aceptar","OK"},
        {"Cancelar","Cancel"},
        {"Interfaz / Interface","Interfaz / Interface"},
        {"Sistema (automático)","System (automatic)"},
        {"No se puede abrir la carpeta de transcripciones.","Cannot open the transcripts folder."},
        {"No se puede crear la transcripción. Comprueba los permisos de la carpeta de datos.","Cannot create the transcript. Check data folder permissions."},
        {"No se pudo guardar la transcripción: ","Could not save the transcript: "},
        {"Audio omitido: el equipo no sigue el ritmo.","Audio skipped: the computer cannot keep up."},
        {"No se puede conectar al audio. Comprueba PipeWire/PulseAudio y los permisos de tu sesión.","Cannot connect to audio. Check PipeWire/PulseAudio and session permissions."},
        {"No se puede crear la captura de audio.","Cannot create audio capture."},
        {"Entrada de audio no disponible: ","Audio input unavailable: "},
        {"Audio conectado","Audio connected"},
        {"Se perdió la entrada de audio; reconectando…","Audio input lost; reconnecting…"},
        {"Servicio de audio desconectado; reintentando…","Audio service disconnected; retrying…"},
        {"Se omitió audio antiguo para mantener el directo.","Older audio skipped to keep up."},
        {"Falta un modelo en models/. Vuelve a copiar la carpeta completa.","A model is missing from models/. Copy the complete folder again."},
        {"Cargando modelo ","Loading model "},
        {"Falta un archivo en models/parakeet/. Copia la carpeta completa.","A file is missing from models/parakeet/. Copy the complete folder."},
        {"No se pudo cargar el modelo. Comprueba los archivos y la memoria disponible.","Cannot load model. Check model files and available memory."},
        {"Error durante el reconocimiento; se reintentará con el siguiente fragmento.","Recognition failed; retrying with the next segment."},
        {"El puerto debe estar entre 1024 y 65534.","Port must be between 1024 and 65534."},
        {"Puerto %1 ocupado. Elige otro en Ajustes.","Port %1 is busy. Choose another in Settings."},
        {"No se puede escribir el archivo de subtítulos en la carpeta de datos. Comprueba sus permisos.","Cannot write captions in the data folder. Check permissions."}
    };
    const auto key=QString::fromUtf8(spanish);
    return catalog.value(key,key);
}
