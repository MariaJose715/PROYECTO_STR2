/**
 * dashboard_content.h - CONTENIDO EMBEBIDO DEL DASHBOARD WEB
 *
 * Los archivos HTML, CSS y JS del dashboard se embeben en el
 * firmware usando raw string literals de C11 (R"delim(...)delim").
 *
 * Esto evita la necesidad de un sistema de archivos (SPIFFS)
 * y simplifica el despliegue: todo el contenido web está
 * compilado directamente en el binario.
 *
 * Raw string literals:
 *   index_html -> R"hdash(...)hdash" (HTML del dashboard)
 *   style_css  -> R"cssd(...)cssd"  (Estilos CSS)
 *   app_js     -> R"appjs(...)appjs" (JavaScript del dashboard)
 */

#ifndef DASHBOARD_CONTENT_H
#define DASHBOARD_CONTENT_H

extern const char index_html[];
extern const char style_css[];
extern const char app_js[];

#endif
