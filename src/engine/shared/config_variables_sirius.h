#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
// This helps IDEs properly syntax highlight the uses of the macro below.
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc)
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc)
#endif

MACRO_CONFIG_INT(UiSidebarPage, ui_sidebar_page, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface sidebar page")
MACRO_CONFIG_INT(UiSidebarSubPage, ui_sidebar_subpage, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface sidebar subpage")
MACRO_CONFIG_INT(UiCursorSize, ui_cursor_size, 99, 0, 99, CFGFLAG_CLIENT | CFGFLAG_SAVE, "UI cursor size")
MACRO_CONFIG_INT(ClSiriusPauseMenu, cl_sirius_pause_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use the Sirius pause menu when opening the ingame menu")

