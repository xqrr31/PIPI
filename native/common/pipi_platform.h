#ifndef PIPI_PLATFORM_H
#define PIPI_PLATFORM_H
long pipi_platform_window_create(const char *title, int width, int height);
int pipi_platform_label_add(long window, const char *text, int x, int y);
int pipi_platform_button_add(long window, const char *text, int x, int y);
int pipi_platform_window_show(long window);
int pipi_platform_next_event(long window);
int pipi_platform_message(long window, const char *title, const char *text);
void pipi_platform_window_destroy(long window);
void pipi_platform_shutdown(void);
#endif
