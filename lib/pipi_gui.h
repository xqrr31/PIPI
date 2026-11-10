#ifndef PIPI_GUI_H
#define PIPI_GUI_H

/* Public PIPI GUI C API. Keep this header C89-compatible. */
#if defined(_WIN32) && defined(PIPI_GUI_BUILD)
#define PIPI_GUI_API __declspec(dllexport)
#elif defined(_WIN32)
#define PIPI_GUI_API __declspec(dllimport)
#else
#define PIPI_GUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

PIPI_GUI_API long pipi_gui_window_create(const char *title, int width, int height);
PIPI_GUI_API int pipi_gui_label_add(long window, const char *text, int x, int y);
PIPI_GUI_API int pipi_gui_button_add(long window, const char *text, int x, int y);
PIPI_GUI_API int pipi_gui_window_show(long window);
PIPI_GUI_API int pipi_gui_next_event(long window);
PIPI_GUI_API int pipi_gui_message(long window, const char *title, const char *text);
PIPI_GUI_API void pipi_gui_window_destroy(long window);
PIPI_GUI_API void pipi_gui_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
