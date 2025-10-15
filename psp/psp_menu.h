/* gpSP PSP Standalone
 * PSP menu interface
 */

#ifndef PSP_MENU_H
#define PSP_MENU_H

// Menu return codes
#define MENU_RESUME    0
#define MENU_EXIT      1
#define MENU_LOAD_ROM  2
#define MENU_SAVE_STATE 3
#define MENU_LOAD_STATE 4

int psp_file_browser(char *selected_file, size_t max_len);
int psp_show_menu(void);
void psp_show_error(const char *message);
void psp_show_message(const char *message);

#endif

