/*
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; You may only use version 2 of the License,
        you have no option to use any other version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002 Olivier Fourdan
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <pango/pango.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4mcs/mcs-client.h>
#include "main.h"
#include "hints.h"
#include "parserc.h"
#include "client.h"
#include "workspaces.h"
#include "debug.h"

#define CHANNEL "xfwm4"
#define TOINT(x) (x ? atoi(x) : 0)

Params params;

static McsClient *client = NULL;
static int mcs_initted = FALSE;

static void notify_cb(const char *name, const char *channel_name, McsAction action, McsSetting * setting, void *data)
{
    if(g_ascii_strcasecmp(CHANNEL, channel_name))
    {
        return;
    }

    switch (action)
    {
        case MCS_ACTION_NEW:
            /* The following is to reduce initial startup time and reloads */
            if(!mcs_initted)
            {
                return;
            }
        case MCS_ACTION_CHANGED:
            if(setting->type == MCS_TYPE_INT)
            {
                if(!strcmp(name, "Xfwm/ClickToFocus"))
                {
                    params.click_to_focus = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/FocusNewWindow"))
                {
                    params.focus_new = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/FocusRaise"))
                {
                    params.raise_on_focus = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/RaiseDelay"))
                {
                    params.raise_delay = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/RaiseOnClick"))
                {
                    params.raise_on_click = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/SnapToBorder"))
                {
                    params.snap_to_border = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/SnapWidth"))
                {
                    params.snap_width = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/WrapWorkspaces"))
                {
                    params.wrap_workspaces = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/BoxMove"))
                {
                    params.box_move = setting->data.v_int;
                }
                else if(!strcmp(name, "Xfwm/BoxResize"))
                {
                    params.box_resize = setting->data.v_int;
                }
            }
            else if(setting->type == MCS_TYPE_STRING)
            {
                if(!strcmp(name, "Xfwm/DblClickAction"))
                {
                    reloadSettings(UPDATE_NONE);
                }
                else if(!strcmp(name, "Xfwm/KeyThemeName"))
                {
                    reloadSettings(UPDATE_KEYGRABS);
                }
                else if(!strcmp(name, "Xfwm/ThemeName"))
                {
                    reloadSettings(UPDATE_GRAVITY);
                }
                else if(!strcmp(name, "Xfwm/ButtonLayout"))
                {
                    reloadSettings(UPDATE_FRAME);
                }
                if(!strcmp(name, "Xfwm/TitleAlign"))
                {
                    reloadSettings(UPDATE_FRAME);
                }
                if(!strcmp(name, "Xfwm/TitleFont"))
                {
                    reloadSettings(UPDATE_FRAME);
                }
            }
            break;
        case MCS_ACTION_DELETED:
        default:
            break;
    }
}

static GdkFilterReturn client_event_filter(GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
    if(mcs_client_process_event(client, (XEvent *) xevent))
        return GDK_FILTER_REMOVE;
    else
        return GDK_FILTER_CONTINUE;
}

static void watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin;

    gdkwin = gdk_window_lookup(window);

    if(is_start)
    {
        if(!gdkwin)
        {
            gdkwin = gdk_window_foreign_new(window);
        }
        else
        {
            g_object_ref(gdkwin);
        }
        gdk_window_add_filter(gdkwin, client_event_filter, cb_data);
    }
    else
    {
        g_assert(gdkwin);
        gdk_window_remove_filter(gdkwin, client_event_filter, cb_data);
        g_object_unref(gdkwin);
    }
}

static void loadRcData(Settings rc[])
{
    const gchar *homedir = g_get_home_dir();
    if(!parseRc("defaults", DATADIR, rc))
    {
        fprintf(stderr, "%s: Missing defaults file\n", progname);
        exit(1);
    }
    parseRc(".xfwm4rc", homedir, rc);
}

static void loadMcsData(Settings rc[])
{
    McsSetting *setting;
    if(client)
    {
        if(mcs_client_get_setting(client, "Xfwm/ClickToFocus", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("click_to_focus", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/FocusNewWindow", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("focus_new", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/FocusRaise", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("raise_on_focus", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/RaiseDelay", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt("raise_delay", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/RaiseOnClick", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("raise_on_click", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/SnapToBorder", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("snap_to_border", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/SnapWidth", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt("snap_width", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/WrapWorkspaces", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("wrap_workspaces", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/BoxMove", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("box_move", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/BoxResize", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt("box_resize", setting->data.v_int, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/DblClickAction", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("double_click_action", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/ThemeName", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("theme", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/KeyThemeName", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("keytheme", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/ButtonLayout", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("button_layout", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/TitleAlign", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("title_alignment", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
        if(mcs_client_get_setting(client, "Xfwm/TitleFont", CHANNEL, &setting) == MCS_SUCCESS)
        {
            setValue("title_font", setting->data.v_string, rc);
            mcs_setting_free(setting);
        }
    }
}

static void loadTheme(Settings rc[])
{
    gchar *theme;
    gchar *font;
    XpmColorSymbol colsym[20];
    GtkWidget *widget = getDefaultGtkWidget();
    PangoFontDescription *desc;
    guint i;

    rc[0].value = get_style(widget, "fg", "selected");
    rc[1].value = get_style(widget, "fg", "normal");
    rc[2].value = get_style(widget, "fg", "active");
    rc[3].value = get_style(widget, "fg", "normal");
    rc[4].value = get_style(widget, "bg", "selected");
    rc[5].value = get_style(widget, "light", "selected");
    rc[6].value = get_style(widget, "dark", "selected");
    rc[7].value = get_style(widget, "mid", "selected");
    rc[8].value = get_style(widget, "bg", "normal");
    rc[9].value = get_style(widget, "light", "normal");
    rc[10].value = get_style(widget, "dark", "normal");
    rc[11].value = get_style(widget, "mid", "normal");
    rc[12].value = get_style(widget, "bg", "active");
    rc[13].value = get_style(widget, "light", "active");
    rc[14].value = get_style(widget, "dark", "active");
    rc[15].value = get_style(widget, "mid", "active");
    rc[16].value = get_style(widget, "bg", "normal");
    rc[17].value = get_style(widget, "light", "normal");
    rc[18].value = get_style(widget, "dark", "normal");
    rc[19].value = get_style(widget, "mid", "normal");

    theme = getThemeDir(getValue("theme", rc));
    parseRc("themerc", theme, rc);

    for(i = 0; i < 20; i++)
    {
        colsym[i].name = rc[i].option;
        colsym[i].value = rc[i].value;
    }

    if(params.title_colors[ACTIVE].allocated)
    {
        gdk_colormap_free_colors(gdk_colormap_get_system(), &params.title_colors[ACTIVE].col, 1);
        params.title_colors[ACTIVE].allocated = FALSE;
    }
    if(gdk_color_parse(rc[0].value, &params.title_colors[ACTIVE].col))
    {
        if(gdk_colormap_alloc_color(gdk_colormap_get_system(), &params.title_colors[ACTIVE].col, FALSE, FALSE))
        {
            params.title_colors[ACTIVE].allocated = TRUE;
            if(params.title_colors[ACTIVE].gc)
            {
                g_object_unref(G_OBJECT(params.title_colors[ACTIVE].gc));
            }
            params.title_colors[ACTIVE].gc = gdk_gc_new(getDefaultGdkWindow());
            gdk_gc_copy(params.title_colors[ACTIVE].gc, get_style_gc(widget, "text", "selected"));
            gdk_gc_set_foreground(params.title_colors[ACTIVE].gc, &params.title_colors[ACTIVE].col);
        }
        else
        {
            gdk_beep();
            g_message("Cannot allocate active color %s\n", rc[0].value);
        }
    }
    else
    {
        gdk_beep();
        g_message("Cannot parse active color %s\n", rc[0].value);
    }

    if(params.black_gc)
    {
        g_object_unref(G_OBJECT(params.black_gc));
    }
    params.black_gc = widget->style->black_gc;
    g_object_ref(G_OBJECT(widget->style->black_gc));

    if(params.white_gc)
    {
        g_object_unref(G_OBJECT(params.white_gc));
    }
    params.white_gc = widget->style->white_gc;
    g_object_ref(G_OBJECT(widget->style->white_gc));

    if(params.title_colors[INACTIVE].allocated)
    {
        gdk_colormap_free_colors(gdk_colormap_get_system(), &params.title_colors[INACTIVE].col, 1);
        params.title_colors[INACTIVE].allocated = FALSE;
    }
    if(gdk_color_parse(rc[1].value, &params.title_colors[INACTIVE].col))
    {
        if(gdk_colormap_alloc_color(gdk_colormap_get_system(), &params.title_colors[INACTIVE].col, FALSE, FALSE))
        {
            params.title_colors[INACTIVE].allocated = TRUE;
            if(params.title_colors[INACTIVE].gc)
            {
                g_object_unref(G_OBJECT(params.title_colors[INACTIVE].gc));
            }
            params.title_colors[INACTIVE].gc = gdk_gc_new(getDefaultGdkWindow());
            gdk_gc_copy(params.title_colors[INACTIVE].gc, get_style_gc(widget, "text", "normal"));
            gdk_gc_set_foreground(params.title_colors[INACTIVE].gc, &params.title_colors[INACTIVE].col);
        }
        else
        {
            gdk_beep();
            g_message("Cannot allocate inactive color %s\n", rc[1].value);
        }
    }
    else
    {
        gdk_beep();
        g_message("Cannot parse inactive color %s\n", rc[1].value);
    }

    font = getValue("title_font", rc);
    if(font && strlen(font))
    {
        desc = pango_font_description_from_string(font);
        if(desc)
        {
            gtk_widget_modify_font(widget, desc);
        }
    }

    loadPixmap(dpy, &params.sides[SIDE_LEFT][ACTIVE], theme, "left-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.sides[SIDE_LEFT][INACTIVE], theme, "left-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.sides[SIDE_RIGHT][ACTIVE], theme, "right-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.sides[SIDE_RIGHT][INACTIVE], theme, "right-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.sides[SIDE_BOTTOM][ACTIVE], theme, "bottom-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.sides[SIDE_BOTTOM][INACTIVE], theme, "bottom-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_TOP_LEFT][ACTIVE], theme, "top-left-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_TOP_LEFT][INACTIVE], theme, "top-left-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_TOP_RIGHT][ACTIVE], theme, "top-right-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_TOP_RIGHT][INACTIVE], theme, "top-right-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_BOTTOM_LEFT][ACTIVE], theme, "bottom-left-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_BOTTOM_LEFT][INACTIVE], theme, "bottom-left-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_BOTTOM_RIGHT][ACTIVE], theme, "bottom-right-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.corners[CORNER_BOTTOM_RIGHT][INACTIVE], theme, "bottom-right-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[HIDE_BUTTON][ACTIVE], theme, "hide-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[HIDE_BUTTON][INACTIVE], theme, "hide-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[HIDE_BUTTON][PRESSED], theme, "hide-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[CLOSE_BUTTON][ACTIVE], theme, "close-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[CLOSE_BUTTON][INACTIVE], theme, "close-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[CLOSE_BUTTON][PRESSED], theme, "close-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MAXIMIZE_BUTTON][ACTIVE], theme, "maximize-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MAXIMIZE_BUTTON][INACTIVE], theme, "maximize-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MAXIMIZE_BUTTON][PRESSED], theme, "maximize-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[SHADE_BUTTON][ACTIVE], theme, "shade-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[SHADE_BUTTON][INACTIVE], theme, "shade-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[SHADE_BUTTON][PRESSED], theme, "shade-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[STICK_BUTTON][ACTIVE], theme, "stick-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[STICK_BUTTON][INACTIVE], theme, "stick-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[STICK_BUTTON][PRESSED], theme, "stick-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MENU_BUTTON][ACTIVE], theme, "menu-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MENU_BUTTON][INACTIVE], theme, "menu-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.buttons[MENU_BUTTON][PRESSED], theme, "menu-pressed.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_1][ACTIVE], theme, "title-1-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_1][INACTIVE], theme, "title-1-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_2][ACTIVE], theme, "title-2-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_2][INACTIVE], theme, "title-2-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_3][ACTIVE], theme, "title-3-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_3][INACTIVE], theme, "title-3-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_4][ACTIVE], theme, "title-4-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_4][INACTIVE], theme, "title-4-inactive.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_5][ACTIVE], theme, "title-5-active.xpm", colsym, 20);
    loadPixmap(dpy, &params.title[TITLE_5][INACTIVE], theme, "title-5-inactive.xpm", colsym, 20);

    if(!g_ascii_strcasecmp("left", getValue("title_alignment", rc)))
    {
        params.title_alignment = ALIGN_LEFT;
    }
    else if(!g_ascii_strcasecmp("right", getValue("title_alignment", rc)))
    {
        params.title_alignment = ALIGN_RIGHT;
    }
    else
    {
        params.title_alignment = ALIGN_CENTER;
    }

    params.full_width_title = !g_ascii_strcasecmp("true", getValue("full_width_title", rc));
    params.title_shadow[ACTIVE] = !g_ascii_strcasecmp("true", getValue("title_shadow_active", rc));
    params.title_shadow[INACTIVE] = !g_ascii_strcasecmp("true", getValue("title_shadow_inactive", rc));

    strncpy(params.button_layout, getValue("button_layout", rc), 7);
    params.button_spacing = TOINT(getValue("button_spacing", rc));
    params.button_offset = TOINT(getValue("button_offset", rc));
    params.title_vertical_offset_active = TOINT(getValue("title_vertical_offset_active", rc));
    params.title_vertical_offset_inactive = TOINT(getValue("title_vertical_offset_inactive", rc));
    params.title_horizontal_offset = TOINT(getValue("title_horizontal_offset", rc));

    params.box_gc = createGC(cmap, "#FFFFFF", GXxor, NULL, True);

    g_free(theme);
}

static gboolean loadKeyBindings(Settings rc[])
{
    gchar *keytheme;
    gchar *keythemevalue;

    keythemevalue = getValue("keytheme", rc);
    if(keythemevalue)
    {
        keytheme = getThemeDir(keythemevalue);
        parseRc("keythemerc", keytheme, rc);
        g_free(keytheme);

        if(!checkRc(rc))
        {
            fprintf(stderr, "%s: Missing values in defaults file\n", progname);
            return FALSE;
        }
    }

    parseKeyString(dpy, &params.keys[KEY_MOVE_UP], getValue("move_window_up_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_DOWN], getValue("move_window_down_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_LEFT], getValue("move_window_left_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_RIGHT], getValue("move_window_right_key", rc));
    parseKeyString(dpy, &params.keys[KEY_RESIZE_UP], getValue("resize_window_up_key", rc));
    parseKeyString(dpy, &params.keys[KEY_RESIZE_DOWN], getValue("resize_window_down_key", rc));
    parseKeyString(dpy, &params.keys[KEY_RESIZE_LEFT], getValue("resize_window_left_key", rc));
    parseKeyString(dpy, &params.keys[KEY_RESIZE_RIGHT], getValue("resize_window_right_key", rc));
    parseKeyString(dpy, &params.keys[KEY_CYCLE_WINDOWS], getValue("cycle_windows_key", rc));
    parseKeyString(dpy, &params.keys[KEY_CLOSE_WINDOW], getValue("close_window_key", rc));
    parseKeyString(dpy, &params.keys[KEY_HIDE_WINDOW], getValue("hide_window_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MAXIMIZE_WINDOW], getValue("maximize_window_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MAXIMIZE_VERT], getValue("maximize_vert_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MAXIMIZE_HORIZ], getValue("maximize_horiz_key", rc));
    parseKeyString(dpy, &params.keys[KEY_SHADE_WINDOW], getValue("shade_window_key", rc));
    parseKeyString(dpy, &params.keys[KEY_NEXT_WORKSPACE], getValue("next_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_PREV_WORKSPACE], getValue("prev_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_ADD_WORKSPACE], getValue("add_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_DEL_WORKSPACE], getValue("del_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_STICK_WINDOW], getValue("stick_window_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_1], getValue("workspace_1_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_2], getValue("workspace_2_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_3], getValue("workspace_3_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_4], getValue("workspace_4_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_5], getValue("workspace_5_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_6], getValue("workspace_6_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_7], getValue("workspace_7_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_8], getValue("workspace_8_key", rc));
    parseKeyString(dpy, &params.keys[KEY_WORKSPACE_9], getValue("workspace_9_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_NEXT_WORKSPACE], getValue("move_window_next_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_PREV_WORKSPACE], getValue("move_window_prev_workspace_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_1], getValue("move_window_workspace_1_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_2], getValue("move_window_workspace_2_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_3], getValue("move_window_workspace_3_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_4], getValue("move_window_workspace_4_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_5], getValue("move_window_workspace_5_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_6], getValue("move_window_workspace_6_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_7], getValue("move_window_workspace_7_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_8], getValue("move_window_workspace_8_key", rc));
    parseKeyString(dpy, &params.keys[KEY_MOVE_WORKSPACE_9], getValue("move_window_workspace_9_key", rc));
    ungrabKeys(dpy, gnome_win);
    grabKey(dpy, &params.keys[KEY_CYCLE_WINDOWS], gnome_win);
    grabKey(dpy, &params.keys[KEY_NEXT_WORKSPACE], gnome_win);
    grabKey(dpy, &params.keys[KEY_PREV_WORKSPACE], gnome_win);
    grabKey(dpy, &params.keys[KEY_ADD_WORKSPACE], gnome_win);
    grabKey(dpy, &params.keys[KEY_NEXT_WORKSPACE], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_1], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_2], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_3], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_4], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_5], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_6], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_7], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_8], gnome_win);
    grabKey(dpy, &params.keys[KEY_WORKSPACE_9], gnome_win);
    return TRUE;
}

gboolean loadSettings(void)
{
    Settings rc[] = {
        {"active_text_color", NULL, FALSE},
        {"inactive_text_color", NULL, FALSE},
        {"active_border_color", NULL, FALSE},
        {"inactive_border_color", NULL, FALSE},
        {"active_color_1", NULL, FALSE},
        {"active_hilight_1", NULL, FALSE},
        {"active_shadow_1", NULL, FALSE},
        {"active_mid_1", NULL, FALSE},
        {"active_color_2", NULL, FALSE},
        {"active_hilight_2", NULL, FALSE},
        {"active_shadow_2", NULL, FALSE},
        {"active_mid_2", NULL, FALSE},
        {"inactive_color_1", NULL, FALSE},
        {"inactive_hilight_1", NULL, FALSE},
        {"inactive_shadow_1", NULL, FALSE},
        {"inactive_mid_1", NULL, FALSE},
        {"inactive_color_2", NULL, FALSE},
        {"inactive_hilight_2", NULL, FALSE},
        {"inactive_shadow_2", NULL, FALSE},
        {"inactive_mid_2", NULL, FALSE},
        {"theme", NULL, TRUE},
        {"keytheme", NULL, TRUE},
        {"title_font", NULL, FALSE},
        {"title_alignment", NULL, TRUE},
        {"full_width_title", NULL, TRUE},
        {"title_shadow_active", NULL, TRUE},
        {"title_shadow_inactive", NULL, TRUE},
        {"button_layout", NULL, TRUE},
        {"button_spacing", NULL, TRUE},
        {"title_vertical_offset_active", NULL, TRUE},
        {"title_vertical_offset_inactive", NULL, TRUE},
        {"title_horizontal_offset", NULL, TRUE},
        {"button_offset", NULL, TRUE},
        {"double_click_action", NULL, TRUE},
        {"box_move", NULL, TRUE},
        {"box_resize", NULL, TRUE},
        {"click_to_focus", NULL, TRUE},
        {"focus_hint", NULL, TRUE},
        {"focus_new", NULL, TRUE},
        {"raise_on_focus", NULL, TRUE},
        {"raise_delay", NULL, TRUE},
        {"snap_to_border", NULL, TRUE},
        {"snap_width", NULL, TRUE},
        {"dbl_click_time", NULL, TRUE},
        {"workspace_count", NULL, TRUE},
        {"wrap_workspaces", NULL, TRUE},
        {"close_window_key", NULL, TRUE},
        {"hide_window_key", NULL, TRUE},
        {"maximize_window_key", NULL, TRUE},
        {"maximize_vert_key", NULL, TRUE},
        {"maximize_horiz_key", NULL, TRUE},
        {"shade_window_key", NULL, TRUE},
        {"cycle_windows_key", NULL, TRUE},
        {"move_window_up_key", NULL, TRUE},
        {"move_window_down_key", NULL, TRUE},
        {"move_window_left_key", NULL, TRUE},
        {"move_window_right_key", NULL, TRUE},
        {"resize_window_up_key", NULL, TRUE},
        {"resize_window_down_key", NULL, TRUE},
        {"resize_window_left_key", NULL, TRUE},
        {"resize_window_right_key", NULL, TRUE},
        {"next_workspace_key", NULL, TRUE},
        {"prev_workspace_key", NULL, TRUE},
        {"add_workspace_key", NULL, TRUE},
        {"del_workspace_key", NULL, TRUE},
        {"stick_window_key", NULL, TRUE},
        {"workspace_1_key", NULL, TRUE},
        {"workspace_2_key", NULL, TRUE},
        {"workspace_3_key", NULL, TRUE},
        {"workspace_4_key", NULL, TRUE},
        {"workspace_5_key", NULL, TRUE},
        {"workspace_6_key", NULL, TRUE},
        {"workspace_7_key", NULL, TRUE},
        {"workspace_8_key", NULL, TRUE},
        {"workspace_9_key", NULL, TRUE},
        {"move_window_next_workspace_key", NULL, TRUE},
        {"move_window_prev_workspace_key", NULL, TRUE},
        {"move_window_workspace_1_key", NULL, TRUE},
        {"move_window_workspace_2_key", NULL, TRUE},
        {"move_window_workspace_3_key", NULL, TRUE},
        {"move_window_workspace_4_key", NULL, TRUE},
        {"move_window_workspace_5_key", NULL, TRUE},
        {"move_window_workspace_6_key", NULL, TRUE},
        {"move_window_workspace_7_key", NULL, TRUE},
        {"move_window_workspace_8_key", NULL, TRUE},
        {"move_window_workspace_9_key", NULL, TRUE},
        {"raise_on_click", NULL, TRUE},
        {NULL, NULL, FALSE}
    };
    GValue tmp_val = { 0, };

    DBG("entering loadSettings\n");

    loadRcData(rc);
    loadMcsData(rc);
    loadTheme(rc);

    if(!loadKeyBindings(rc))
    {
        freeRc(rc);
        return FALSE;
    }

    params.box_resize = !g_ascii_strcasecmp("true", getValue("box_resize", rc));
    params.box_move = !g_ascii_strcasecmp("true", getValue("box_move", rc));

    params.click_to_focus = !g_ascii_strcasecmp("true", getValue("click_to_focus", rc));
    params.focus_hint = !g_ascii_strcasecmp("true", getValue("focus_hint", rc));
    params.focus_new = !g_ascii_strcasecmp("true", getValue("focus_new", rc));
    params.raise_on_focus = !g_ascii_strcasecmp("true", getValue("raise_on_focus", rc));
    params.raise_delay = abs(TOINT(getValue("raise_delay", rc)));
    params.raise_on_click = !g_ascii_strcasecmp("true", getValue("raise_on_click", rc));

    params.snap_to_border = !g_ascii_strcasecmp("true", getValue("snap_to_border", rc));
    params.snap_width = abs(TOINT(getValue("snap_width", rc)));
    params.dbl_click_time = abs(TOINT(getValue("dbl_click_time", rc)));
    g_value_init(&tmp_val, G_TYPE_INT);
    if(gdk_setting_get("gtk-double-click-time", &tmp_val))
    {
        params.dbl_click_time = abs(g_value_get_int(&tmp_val));
    }

    if(!g_ascii_strcasecmp("shade", getValue("double_click_action", rc)))
    {
        params.double_click_action = ACTION_SHADE;
    }
    else if(!g_ascii_strcasecmp("hide", getValue("double_click_action", rc)))
    {
        params.double_click_action = ACTION_HIDE;
    }
    else if(!g_ascii_strcasecmp("maximize", getValue("double_click_action", rc)))
    {
        params.double_click_action = ACTION_MAXIMIZE;
    }
    else
    {
        params.double_click_action = ACTION_NONE;
    }

    if(params.workspace_count < 0)
    {
        params.workspace_count = abs(TOINT(getValue("workspace_count", rc)));
        setGnomeHint(dpy, root, win_workspace_count, params.workspace_count);
	setNetHint(dpy, root, net_number_of_desktops, params.workspace_count);
    }

    params.wrap_workspaces = !g_ascii_strcasecmp("true", getValue("wrap_workspaces", rc));
    freeRc(rc);
    return TRUE;
}

static void unloadTheme(void)
{
    int i;
    DBG("entering unloadTheme\n");

    for(i = 0; i < 3; i++)
    {
        freePixmap(dpy, &params.sides[i][ACTIVE]);
        freePixmap(dpy, &params.sides[i][INACTIVE]);
    }
    for(i = 0; i < 4; i++)
    {
        freePixmap(dpy, &params.corners[i][ACTIVE]);
        freePixmap(dpy, &params.corners[i][INACTIVE]);
    }
    for(i = 0; i < BUTTON_COUNT; i++)
    {
        freePixmap(dpy, &params.buttons[i][ACTIVE]);
        freePixmap(dpy, &params.buttons[i][INACTIVE]);
        freePixmap(dpy, &params.buttons[i][PRESSED]);
    }
    for(i = 0; i < 5; i++)
    {
        freePixmap(dpy, &params.title[i][ACTIVE]);
        freePixmap(dpy, &params.title[i][INACTIVE]);
    }
    if(params.box_gc != None)
    {
        XFreeGC(dpy, params.box_gc);
        params.box_gc = None;
    }
}

gboolean reloadSettings(int mask)
{
    DBG("entering reloadSettings\n");

    unloadTheme();
    if(!loadSettings())
    {
        return FALSE;
    }
    if(mask)
    {
        clientUpdateAllFrames(mask);
    }

    return TRUE;
}

gboolean initSettings(void)
{
    int i;
    long val = 0;

    DBG("entering initSettings\n");

    mcs_initted = FALSE;
    params.box_gc = None;
    params.black_gc = NULL;
    params.white_gc = NULL;
    params.title_colors[ACTIVE].gc = NULL;
    params.title_colors[ACTIVE].allocated = FALSE;
    params.title_colors[INACTIVE].gc = NULL;
    params.title_colors[INACTIVE].allocated = FALSE;
    params.workspace_count = -1;

    for(i = 0; i < 3; i++)
    {
        initPixmap(&params.sides[i][ACTIVE]);
        initPixmap(&params.sides[i][INACTIVE]);
    }
    for(i = 0; i < 4; i++)
    {
        initPixmap(&params.corners[i][ACTIVE]);
        initPixmap(&params.corners[i][INACTIVE]);
    }
    for(i = 0; i < BUTTON_COUNT; i++)
    {
        initPixmap(&params.buttons[i][ACTIVE]);
        initPixmap(&params.buttons[i][INACTIVE]);
        initPixmap(&params.buttons[i][PRESSED]);
    }
    for(i = 0; i < 5; i++)
    {
        initPixmap(&params.title[i][ACTIVE]);
        initPixmap(&params.title[i][INACTIVE]);
    }
    if(getNetHint(dpy, root, net_number_of_desktops, &val))
    {
        params.workspace_count = (int) val;
	setGnomeHint(dpy, root, win_workspace_count, params.workspace_count);
    }
    else if(getGnomeHint(dpy, root, win_workspace_count, &val))
    {
        params.workspace_count = (int) val;
	setNetHint(dpy, root, net_number_of_desktops, params.workspace_count);
    }
    if(!mcs_client_check_manager(dpy, screen, "xfce-mcs-manager"))
    {
        g_warning("MCS manager not running");
    }
    client = mcs_client_new(dpy, screen, notify_cb, watch_cb, NULL);
    if(client)
    {
        mcs_client_add_channel(client, CHANNEL);
    }
    else
    {
        g_warning("Cannot create MCS client channel");
    }

    mcs_initted = TRUE;

    if(!loadSettings())
    {
        return FALSE;
    }

    return TRUE;
}

void unloadSettings(void)
{
    DBG("entering unloadSettings\n");

    unloadTheme();
}
