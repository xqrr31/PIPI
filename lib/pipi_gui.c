#include <string.h>
#include "pipi_gui.h"
#include "../native/common/pipi_platform.h"

long pipi_gui_window_create(const char *title, int width, int height) {
    if (title == 0 || width < 160 || height < 120 || width > 4096 || height > 4096) return 0;
    return pipi_platform_window_create(title, width, height);
}
int pipi_gui_label_add(long window, const char *text, int x, int y) {
    if (window <= 0 || text == 0 || x < 0 || y < 0) return 0;
    return pipi_platform_label_add(window, text, x, y);
}
int pipi_gui_button_add(long window, const char *text, int x, int y) {
    if (window <= 0 || text == 0 || x < 0 || y < 0) return 0;
    return pipi_platform_button_add(window, text, x, y);
}
int pipi_gui_window_show(long window) {
    if (window <= 0) return 0;
    return pipi_platform_window_show(window);
}
int pipi_gui_next_event(long window) {
    if (window <= 0) return -1;
    return pipi_platform_next_event(window);
}
int pipi_gui_message(long window, const char *title, const char *text) {
    if (title == 0 || text == 0) return 0;
    return pipi_platform_message(window, title, text);
}
void pipi_gui_window_destroy(long window) {
    if (window > 0) pipi_platform_window_destroy(window);
}
void pipi_gui_shutdown(void) { pipi_platform_shutdown(); }
