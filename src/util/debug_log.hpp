#pragma once

// Logger de debug liviano para diagnosticar el crash de GLES2 en Switch.
// Guarda las últimas N líneas en memoria y las vuelca a disco bajo demanda.

void frame_log(const char* fmt, ...);
void flush_log_to_disk();
