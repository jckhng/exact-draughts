/*
 * Exact Draughts
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unofficial Kindle-focused adaptation using artwork from Draughts by
 * Thiago Fernandes (https://github.com/tobagin/Draughts), GPL-3.0-or-later.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkcairo.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>

#include "draughts_engine.h"

#define KINDLE_WINDOW_TITLE "L:A_N:application_ID:exactdraughts_PC:N_O:URL"
#define KINDLE_WINDOW_TITLE_TOPBAR "L:A_N:application_PC:T_ID:exactdraughts_O:URL"
#define LOG_PATH "/mnt/us/exact-draughts.log"
#define SAVE_PATH "/mnt/us/extensions/exact-draughts/exact-draughts.save"
#define LEGACY_SAVE_PATH "/mnt/us/exact-draughts.save"
#define SAVE_MAGIC "KDRAUGHTS2"
#define KINDLE_APP_WIDTH 1072
#define KINDLE_APP_HEIGHT 1448

typedef enum {
    MODE_PLAY_RED = 0,
    MODE_PLAY_BLACK = 1,
    MODE_TWO_PLAYER = 2,
    MODE_AI_DEMO = 3
} AppMode;

static const char *kindle_window_title(void)
{
    const char *value = g_getenv("KINDLE_SHOW_TOPBAR");
    return (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0) ? KINDLE_WINDOW_TITLE_TOPBAR
                                                                          : KINDLE_WINDOW_TITLE;
}

typedef struct {
    GtkWidget *window;
    GtkWidget *board;
    GtkWidget *status;
    GtkWidget *red_score;
    GtkWidget *black_score;
    GtkWidget *moves_label;
    GtkWidget *history_sidebar;
    GtkWidget *history_toggle_button;
    GtkWidget *mode_combo;
    GtkWidget *variant_combo;
    GtkWidget *level_combo;
    GtkWidget *history_first_button;
    GtkWidget *history_prev_button;
    GtkWidget *history_next_button;
    GtkWidget *history_latest_button;
    DraughtsGame game;
    AppMode mode;
    DraughtsVariant variant;
    DraughtsAiLevel ai_level;
    gboolean suppress_option_callbacks;
    int view_ply;
    guint ai_source;
    int selected_x;
    int selected_y;
    DraughtsMove selected_moves[DRAUGHTS_MAX_LEGAL_MOVES];
    int selected_move_count;
    GdkPixbuf *red_man;
    GdkPixbuf *red_king;
    GdkPixbuf *black_man;
    GdkPixbuf *black_king;
    char message[180];
    gboolean history_visible;
} AppState;

static AppState app;

typedef struct {
    char magic[12];
    DraughtsGame game;
} SaveFile;

static gboolean is_ai_turn(void);
static gboolean ai_timeout(gpointer data);
static void update_ui(void);
static void new_game(void);

static void toggle_history_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    app.history_visible = !app.history_visible;
    if (app.history_visible) {
        gtk_widget_show(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Hide Moves");
    } else {
        gtk_widget_hide(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Show Moves");
    }
    gtk_widget_queue_resize(app.board);
    gtk_widget_queue_draw(app.board);
}

static void app_log(const char *message)
{
    FILE *f = fopen(LOG_PATH, "a");
    if (!f)
        return;
    fprintf(f, "[app] %s\n", message);
    fclose(f);
}

static void app_apply_high_contrast(GtkWidget *widget)
{
    GdkColor black = {0, 0x0000, 0x0000, 0x0000};
    GdkColor white = {0, 0xffff, 0xffff, 0xffff};

    gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_text(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_base(widget, GTK_STATE_SELECTED, &black);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_bg(widget, GTK_STATE_SELECTED, &black);
}

static void app_install_kindle_style(void)
{
    gtk_rc_parse_string(
        "style \"kindle_high_contrast\" {\n"
        "  fg[NORMAL] = \"#000000\"\n"
        "  fg[ACTIVE] = \"#000000\"\n"
        "  fg[PRELIGHT] = \"#ffffff\"\n"
        "  fg[SELECTED] = \"#ffffff\"\n"
        "  text[NORMAL] = \"#000000\"\n"
        "  text[ACTIVE] = \"#000000\"\n"
        "  text[SELECTED] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\"\n"
        "  base[ACTIVE] = \"#ffffff\"\n"
        "  base[SELECTED] = \"#000000\"\n"
        "  bg[NORMAL] = \"#ffffff\"\n"
        "  bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[PRELIGHT] = \"#000000\"\n"
        "  bg[SELECTED] = \"#000000\"\n"
        "}\n"
        "gtk-button-images = 0\n"
        "gtk-menu-images = 0\n"
        "class \"GtkComboBox\" style \"kindle_high_contrast\"\n"
        "class \"GtkCellView\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenu\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenuItem\" style \"kindle_high_contrast\"\n"
        "class \"GtkButton\" style \"kindle_high_contrast\"\n"
        "class \"GtkLabel\" style \"kindle_high_contrast\"\n");
}

static void set_message(const char *message)
{
    g_strlcpy(app.message, message, sizeof(app.message));
}

static GdkPixbuf *load_piece_asset(const char *name)
{
    char path[512];
    GdkPixbuf *pixbuf;
    GError *error = NULL;

    snprintf(path, sizeof(path), "assets/%s", name);
    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (pixbuf)
        return pixbuf;
    if (error)
        g_error_free(error);

    error = NULL;
    snprintf(path, sizeof(path), "/mnt/us/extensions/exact-draughts/assets/%s", name);
    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (!pixbuf && error) {
        app_log(error->message);
        g_error_free(error);
    }
    return pixbuf;
}

static void load_assets(void)
{
    app.red_man = load_piece_asset("red-checker.png");
    app.red_king = load_piece_asset("red-king.png");
    app.black_man = load_piece_asset("black-checker.png");
    app.black_king = load_piece_asset("black-king.png");
}

static void free_assets(void)
{
    if (app.red_man)
        g_object_unref(app.red_man);
    if (app.red_king)
        g_object_unref(app.red_king);
    if (app.black_man)
        g_object_unref(app.black_man);
    if (app.black_king)
        g_object_unref(app.black_king);
}

static void clear_selection(void)
{
    app.selected_x = -1;
    app.selected_y = -1;
    app.selected_move_count = 0;
}

static int current_view_ply(void)
{
    if (app.view_ply < 0 || app.view_ply > app.game.move_count)
        return app.game.move_count;
    return app.view_ply;
}

static gboolean is_viewing_latest(void)
{
    return current_view_ply() == app.game.move_count;
}

static DraughtsPiece viewed_piece_at(int x, int y)
{
    return app.game.history[current_view_ply()][y][x];
}

static gboolean last_move_cells(int *from_x, int *from_y, int *to_x, int *to_y)
{
    int ply = current_view_ply();
    const char *move;
    int fy;
    int ty;

    if (ply <= 0 || ply > app.game.move_count)
        return FALSE;
    move = app.game.moves[ply - 1];
    if (strlen(move) < 5)
        return FALSE;
    fy = move[1] - '0';
    ty = move[4] - '0';
    if (move[0] < 'a' || move[0] >= 'a' + app.game.size ||
        move[3] < 'a' || move[3] >= 'a' + app.game.size ||
        fy < 1 || fy > app.game.size || ty < 1 || ty > app.game.size)
        return FALSE;

    *from_x = move[0] - 'a';
    *from_y = app.game.size - fy;
    *to_x = move[3] - 'a';
    *to_y = app.game.size - ty;
    return TRUE;
}

static int board_size(void)
{
    return app.game.size > 0 ? app.game.size : DRAUGHTS_SIZE;
}

static gboolean is_ai_turn(void)
{
    if (app.game.winner != DRAUGHTS_NO_PLAYER || !is_viewing_latest())
        return FALSE;

    if (app.mode == MODE_TWO_PLAYER)
        return FALSE;
    if (app.mode == MODE_AI_DEMO)
        return TRUE;
    if (app.mode == MODE_PLAY_RED)
        return app.game.turn == DRAUGHTS_BLACK;
    return app.game.turn == DRAUGHTS_RED;
}

static gboolean selected_dest_is_legal(int x, int y)
{
    int i;

    for (i = 0; i < app.selected_move_count; i++) {
        if (app.selected_moves[i].to_x == x && app.selected_moves[i].to_y == y)
            return TRUE;
    }

    return FALSE;
}

static DraughtsMove *selected_move_to(int x, int y)
{
    int i;

    for (i = 0; i < app.selected_move_count; i++) {
        if (app.selected_moves[i].to_x == x && app.selected_moves[i].to_y == y)
            return &app.selected_moves[i];
    }

    return NULL;
}

static void select_square(int x, int y)
{
    app.selected_move_count = draughts_collect_moves_for_piece(&app.game, x, y, app.selected_moves);
    if (app.selected_move_count > 0) {
        app.selected_x = x;
        app.selected_y = y;
        snprintf(app.message, sizeof(app.message), "%s selected. Tap a highlighted square.",
                 draughts_player_name(app.game.turn));
    } else {
        clear_selection();
        if (app.game.must_continue_capture)
            set_message("A capture chain is in progress. Continue with the highlighted piece.");
        else if (draughts_has_capture(&app.game, app.game.turn))
            set_message("Capture is mandatory.");
        else
            set_message("That piece has no legal move.");
    }
}

static void draw_text_center(cairo_t *cr, const char *text, double cx, double cy, double size)
{
    cairo_text_extents_t extents;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, cx - extents.width / 2.0 - extents.x_bearing,
                  cy - extents.height / 2.0 - extents.y_bearing);
    cairo_show_text(cr, text);
}

static void draw_piece_fallback(cairo_t *cr, DraughtsPiece piece, double cx, double cy, double radius)
{
    gboolean red = draughts_piece_belongs_to(piece, DRAUGHTS_RED);
    double x;

    cairo_save(cr);
    cairo_arc(cr, cx + radius * 0.09, cy + radius * 0.11, radius, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, radius, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, red ? 0.93 : 0.04, red ? 0.93 : 0.04, red ? 0.90 : 0.04);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, radius * 0.10);
    cairo_stroke(cr);

    cairo_arc(cr, cx, cy, radius * 0.70, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, red ? 0.78 : 0.22, red ? 0.78 : 0.22, red ? 0.74 : 0.22);
    cairo_set_line_width(cr, radius * 0.07);
    cairo_stroke(cr);

    if (red) {
        cairo_arc(cr, cx, cy, radius * 0.82, 0, 2 * G_PI);
        cairo_clip(cr);
        cairo_new_path(cr);
        cairo_set_source_rgb(cr, 0.18, 0.18, 0.18);
        cairo_set_line_width(cr, MAX(1.5, radius * 0.045));
        for (x = cx - radius * 1.2; x < cx + radius * 1.2; x += radius * 0.28) {
            cairo_move_to(cr, x, cy + radius);
            cairo_line_to(cr, x + radius, cy - radius);
        }
        cairo_stroke(cr);
    }
    cairo_restore(cr);

    cairo_arc(cr, cx, cy, radius, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, red ? 0.0 : 1.0, red ? 0.0 : 1.0, red ? 0.0 : 1.0);
    cairo_set_line_width(cr, MAX(2.0, radius * 0.055));
    cairo_stroke(cr);

    cairo_arc(cr, cx, cy, radius * 0.48, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, red ? 0.96 : 1.0, red ? 0.96 : 1.0, red ? 0.92 : 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, MAX(1.5, radius * 0.045));
    cairo_stroke(cr);

    if (draughts_piece_is_king(piece)) {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_line_width(cr, MAX(2.0, radius * 0.08));
        cairo_arc(cr, cx, cy, radius * 0.61, 0, 2 * G_PI);
        cairo_stroke(cr);
        draw_text_center(cr, "K", cx, cy - radius * 0.02, radius * 0.78);
    } else {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        draw_text_center(cr, red ? "R" : "B", cx, cy + radius * 0.02, radius * 0.72);
    }
}

static void draw_piece(cairo_t *cr, DraughtsPiece piece, int x, int y, int cell)
{
    draw_piece_fallback(cr, piece, x + cell / 2.0, y + cell / 2.0, cell * 0.38);
}

static gboolean board_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    cairo_t *cr = gdk_cairo_create(widget->window);
    GtkAllocation allocation;
    int cell;
    int board_px;
    int ox;
    int oy;
    int x;
    int y;
    int last_from_x = -1;
    int last_from_y = -1;
    int last_to_x = -1;
    int last_to_y = -1;
    gboolean has_last;

    (void)event;
    (void)data;

    gtk_widget_get_allocation(widget, &allocation);
    cell = MIN(allocation.width, allocation.height) / board_size();
    cell -= 4;
    board_px = cell * board_size();
    ox = (allocation.width - board_px) / 2;
    oy = (allocation.height - board_px) / 2;
    has_last = last_move_cells(&last_from_x, &last_from_y, &last_to_x, &last_to_y);

    cairo_set_source_rgb(cr, 0.98, 0.98, 0.95);
    cairo_paint(cr);

    for (y = 0; y < app.game.size; y++) {
        for (x = 0; x < app.game.size; x++) {
            int px = ox + x * cell;
            int py = oy + y * cell;

            if (((x + y) & 1) == 1)
                cairo_set_source_rgb(cr, 0.58, 0.58, 0.53);
            else
                cairo_set_source_rgb(cr, 0.96, 0.96, 0.91);
            cairo_rectangle(cr, px, py, cell, cell);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);
            cairo_set_line_width(cr, 1.0);
            cairo_rectangle(cr, px + 0.5, py + 0.5, cell - 1, cell - 1);
            cairo_stroke(cr);

            if (has_last && ((x == last_from_x && y == last_from_y) || (x == last_to_x && y == last_to_y))) {
                gboolean current = x == last_to_x && y == last_to_y;
                cairo_rectangle(cr, px + 4, py + 4, cell - 8, cell - 8);
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, current ? 0.20 : 0.10);
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                cairo_set_line_width(cr, current ? 4.0 : 2.5);
                cairo_stroke(cr);
            }

            if (is_viewing_latest() && x == app.selected_x && y == app.selected_y) {
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                cairo_set_line_width(cr, 6.0);
                cairo_rectangle(cr, px + 5, py + 5, cell - 10, cell - 10);
                cairo_stroke(cr);
            }

            if (is_viewing_latest() && selected_dest_is_legal(x, y)) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.55);
                cairo_arc(cr, px + cell / 2.0, py + cell / 2.0, cell * 0.16, 0, 2 * G_PI);
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                cairo_set_line_width(cr, 2.0);
                cairo_stroke(cr);
            }

            if (is_viewing_latest() && app.game.must_continue_capture &&
                x == app.game.forced_x && y == app.game.forced_y) {
                cairo_set_source_rgb(cr, 0, 0, 0);
                cairo_set_line_width(cr, 5.0);
                cairo_rectangle(cr, px + 7, py + 7, cell - 14, cell - 14);
                cairo_stroke(cr);
            }

            if (viewed_piece_at(x, y) != DRAUGHTS_EMPTY) {
                draw_piece(cr, viewed_piece_at(x, y), px, py, cell);
                if (has_last && x == last_to_x && y == last_to_y) {
                    cairo_new_path(cr);
                    cairo_arc(cr, px + cell / 2.0, py + cell / 2.0, cell * 0.43, 0, 2 * G_PI);
                    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                    cairo_set_line_width(cr, 5.0);
                    cairo_stroke_preserve(cr);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    cairo_set_line_width(cr, 2.0);
                    cairo_stroke(cr);
                }
            }
        }
    }

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 3.0);
    cairo_rectangle(cr, ox, oy, board_px, board_px);
    cairo_stroke(cr);

    cairo_destroy(cr);
    return FALSE;
}

static void update_after_move(DraughtsMoveState state)
{
    app.view_ply = -1;

    if (state == DRAUGHTS_CONTINUE_CAPTURE) {
        if (is_ai_turn()) {
            clear_selection();
            set_message("AI continues the capture chain.");
        } else {
            select_square(app.game.forced_x, app.game.forced_y);
            set_message("Capture again with the same piece.");
        }
    } else {
        clear_selection();
        if (state == DRAUGHTS_GAME_OVER)
            snprintf(app.message, sizeof(app.message), "Game over. %s wins.", draughts_player_name(app.game.winner));
        else if (state == DRAUGHTS_RUNNING)
            snprintf(app.message, sizeof(app.message), "%s to move.", draughts_player_name(app.game.turn));
        else
            set_message("Illegal move.");
    }

    update_ui();
}

static gboolean board_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    GtkAllocation allocation;
    int cell;
    int board_px;
    int ox;
    int oy;
    int x;
    int y;
    DraughtsMove *move;

    (void)data;

    if (event->button != 1 || !is_viewing_latest() || is_ai_turn() || app.game.winner != DRAUGHTS_NO_PLAYER)
        return FALSE;

    gtk_widget_get_allocation(widget, &allocation);
    cell = MIN(allocation.width, allocation.height) / board_size() - 4;
    board_px = cell * board_size();
    ox = (allocation.width - board_px) / 2;
    oy = (allocation.height - board_px) / 2;

    if (event->x < ox || event->x >= ox + board_px || event->y < oy || event->y >= oy + board_px)
        return FALSE;

    x = ((int)event->x - ox) / cell;
    y = ((int)event->y - oy) / cell;
    if (x < 0 || x >= app.game.size || y < 0 || y >= app.game.size)
        return FALSE;

    move = selected_move_to(x, y);
    if (move) {
        update_after_move(draughts_apply_move(&app.game, move));
        return TRUE;
    }

    if (draughts_piece_belongs_to(app.game.board[y][x], app.game.turn)) {
        select_square(x, y);
        update_ui();
        return TRUE;
    }

    set_message("Tap one of your pieces, then tap a highlighted destination.");
    update_ui();
    return TRUE;
}

static gboolean ai_timeout(gpointer data)
{
    DraughtsMove move;

    (void)data;
    app.ai_source = 0;

    if (!is_ai_turn())
        return FALSE;

    if (draughts_ai_pick_move_level(&app.game, app.ai_level, &move))
        update_after_move(draughts_apply_move(&app.game, &move));
    else
        update_ui();

    return FALSE;
}

static void update_history_label(void)
{
    GString *text = g_string_new("");
    int view_ply = current_view_ply();
    int i;

    g_string_append_printf(text, "%c Start\n", view_ply == 0 ? '>' : ' ');
    for (i = 0; i < app.game.move_count; i++) {
        const char *player = app.game.turn_history[i] == DRAUGHTS_RED ? "Red" : "Black";
        g_string_append_printf(text, "%c%2d. %-5s %s\n", view_ply == i + 1 ? '>' : ' ',
                               i + 1, player, app.game.moves[i]);
    }

    gtk_label_set_text(GTK_LABEL(app.moves_label), text->str);
    g_string_free(text, TRUE);
}

static void update_ui(void)
{
    char buf[160];
    int view_ply = current_view_ply();
    DraughtsGame viewed = app.game;

    memcpy(viewed.board, app.game.history[view_ply], sizeof(viewed.board));
    draughts_update_counts(&viewed);

    if (is_viewing_latest()) {
        gtk_label_set_text(GTK_LABEL(app.status), app.message);
    } else {
        snprintf(buf, sizeof(buf), "Viewing move %d of %d. Tap >| to resume play.", view_ply, app.game.move_count);
        gtk_label_set_text(GTK_LABEL(app.status), buf);
    }

    snprintf(buf, sizeof(buf), "Red: %d", viewed.red_count);
    gtk_label_set_text(GTK_LABEL(app.red_score), buf);
    snprintf(buf, sizeof(buf), "Black: %d  %s", viewed.black_count, draughts_variant_name(app.game.variant));
    gtk_label_set_text(GTK_LABEL(app.black_score), buf);
    update_history_label();
    gtk_widget_queue_draw(app.board);

    gtk_widget_set_sensitive(app.history_first_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_prev_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_next_button, view_ply < app.game.move_count);
    gtk_widget_set_sensitive(app.history_latest_button, view_ply < app.game.move_count);

    if (is_ai_turn() && app.ai_source == 0)
        app.ai_source = g_timeout_add(300, ai_timeout, NULL);
}

static void new_game(void)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    draughts_game_init_variant(&app.game, app.variant);
    app.view_ply = -1;
    clear_selection();
    set_message("Red to move. Captures are mandatory.");
    update_ui();
}

static void new_game_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    new_game();
}

static void save_cb(GtkWidget *widget, gpointer data)
{
    SaveFile save;
    FILE *f;

    (void)widget;
    (void)data;

    memset(&save, 0, sizeof(save));
    g_strlcpy(save.magic, SAVE_MAGIC, sizeof(save.magic));
    save.game = app.game;

    f = fopen(SAVE_PATH, "wb");
    if (!f) {
        set_message("Could not save the game.");
        update_ui();
        return;
    }

    if (fwrite(&save, sizeof(save), 1, f) != 1) {
        fclose(f);
        set_message("Could not save the game.");
        update_ui();
        return;
    }

    fclose(f);
    set_message("Game saved.");
    update_ui();
}

static void load_cb(GtkWidget *widget, gpointer data)
{
    SaveFile save;
    FILE *f;

    (void)widget;
    (void)data;

    f = fopen(SAVE_PATH, "rb");
    if (!f)
        f = fopen(LEGACY_SAVE_PATH, "rb");
    if (!f) {
        set_message("No saved game found.");
        update_ui();
        return;
    }

    if (fread(&save, sizeof(save), 1, f) != 1 || strncmp(save.magic, SAVE_MAGIC, sizeof(save.magic)) != 0) {
        fclose(f);
        set_message("Saved game is not compatible.");
        update_ui();
        return;
    }
    fclose(f);

    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    app.game = save.game;
    app.variant = app.game.variant;
    app.view_ply = -1;
    clear_selection();
    app.suppress_option_callbacks = TRUE;
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.variant_combo), app.variant);
    app.suppress_option_callbacks = FALSE;
    snprintf(app.message, sizeof(app.message), "%s to move.", draughts_player_name(app.game.turn));
    update_ui();
}

static void undo_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (draughts_undo(&app.game)) {
        app.view_ply = -1;
        while (is_ai_turn() && draughts_undo(&app.game)) {
            /* Keep rewinding AI chain moves so Undo returns control to the human. */
        }
        clear_selection();
        snprintf(app.message, sizeof(app.message), "%s to move.", draughts_player_name(app.game.turn));
    } else {
        set_message("Nothing to undo.");
    }
    update_ui();
}

static void mode_changed(GtkComboBox *combo, gpointer data)
{
    (void)data;
    if (app.suppress_option_callbacks)
        return;
    app.mode = (AppMode)gtk_combo_box_get_active(combo);
    new_game();
}

static void variant_changed(GtkComboBox *combo, gpointer data)
{
    (void)data;
    if (app.suppress_option_callbacks)
        return;
    app.variant = (DraughtsVariant)gtk_combo_box_get_active(combo);
    new_game();
}

static void level_changed(GtkComboBox *combo, gpointer data)
{
    (void)data;
    if (app.suppress_option_callbacks)
        return;
    app.ai_level = (DraughtsAiLevel)gtk_combo_box_get_active(combo);
    update_ui();
}

static void set_view_ply(int ply)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (ply < 0)
        ply = 0;
    if (ply > app.game.move_count)
        ply = app.game.move_count;

    app.view_ply = (ply == app.game.move_count) ? -1 : ply;
    clear_selection();
    update_ui();
}

static void history_first_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(0);
}

static void history_prev_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(current_view_ply() - 1);
}

static void history_next_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(current_view_ply() + 1);
}

static void history_latest_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    set_view_ply(app.game.move_count);
}

static void quit_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;
    gtk_main_quit();
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    (void)widget;
    (void)data;

    if (event->keyval == GDK_q || event->keyval == GDK_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    if (event->keyval == GDK_n) {
        new_game();
        return TRUE;
    }
    if (event->keyval == GDK_u) {
        undo_cb(NULL, NULL);
        return TRUE;
    }

    return FALSE;
}

static GtkWidget *make_button(const char *label, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(button, 150, 68);
    app_apply_high_contrast(button);
    g_signal_connect(button, "clicked", callback, NULL);
    return button;
}

static GtkWidget *add_button(GtkWidget *box, const char *label, GCallback callback)
{
    GtkWidget *button = make_button(label, callback);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    return button;
}

static GtkWidget *add_nav_button(GtkWidget *box, const char *label, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_set_size_request(button, 62, 64);
    app_apply_high_contrast(button);
    g_signal_connect(button, "clicked", callback, NULL);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    return button;
}

static GtkWidget *combo_with_items(const char **items, int active)
{
    GtkWidget *combo = gtk_combo_box_new_text();
    int i;

    for (i = 0; items[i] != NULL; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo), items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    gtk_widget_set_size_request(combo, 270, 68);
    app_apply_high_contrast(combo);
    return combo;
}

int main(int argc, char **argv)
{
    static const char *modes[] = {"Play Red", "Play Black", "2 Players", "AI Demo", NULL};
    static const char *variants[] = {"English", "International", NULL};
    static const char *levels[] = {"Easy", "Medium", "Hard", NULL};
    GtkWidget *root;
    GtkWidget *title;
    GtkWidget *menu_row;
    GtkWidget *options_row;
    GtkWidget *content;
    GtkWidget *sidebar;
    GtkWidget *score_box;
    GtkWidget *history_frame;
    GtkWidget *history_scroll;
    GtkWidget *history_nav_box;

    gtk_init(&argc, &argv);
    app_install_kindle_style();
    memset(&app, 0, sizeof(app));
    app.mode = MODE_PLAY_RED;
    app.variant = DRAUGHTS_VARIANT_ENGLISH;
    app.ai_level = DRAUGHTS_AI_MEDIUM;
    app.view_ply = -1;
    app.selected_x = -1;
    app.selected_y = -1;
    load_assets();

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), kindle_window_title());
    gtk_window_set_default_size(GTK_WINDOW(app.window), KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(key_press), NULL);

    root = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(root), 10);
    gtk_container_add(GTK_CONTAINER(app.window), root);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Exact Draughts</b>");
    gtk_box_pack_start(GTK_BOX(root), title, FALSE, FALSE, 0);
    app_apply_high_contrast(title);

    app.status = gtk_label_new("Red to move.");
    gtk_label_set_ellipsize(GTK_LABEL(app.status), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(app.status), 0.5f, 0.5f);
    gtk_box_pack_start(GTK_BOX(root), app.status, FALSE, FALSE, 0);
    app_apply_high_contrast(app.status);

    menu_row = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(root), menu_row, FALSE, FALSE, 0);
    add_button(menu_row, "New", G_CALLBACK(new_game_cb));
    add_button(menu_row, "Undo", G_CALLBACK(undo_cb));
    add_button(menu_row, "Save", G_CALLBACK(save_cb));
    add_button(menu_row, "Load", G_CALLBACK(load_cb));
    add_button(menu_row, "Quit", G_CALLBACK(quit_cb));

    options_row = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(root), options_row, FALSE, FALSE, 0);
    app.mode_combo = combo_with_items(modes, app.mode);
    gtk_box_pack_start(GTK_BOX(options_row), app.mode_combo, FALSE, FALSE, 0);
    g_signal_connect(app.mode_combo, "changed", G_CALLBACK(mode_changed), NULL);
    app.variant_combo = combo_with_items(variants, app.variant);
    gtk_box_pack_start(GTK_BOX(options_row), app.variant_combo, FALSE, FALSE, 0);
    g_signal_connect(app.variant_combo, "changed", G_CALLBACK(variant_changed), NULL);
    app.level_combo = combo_with_items(levels, app.ai_level);
    gtk_box_pack_start(GTK_BOX(options_row), app.level_combo, FALSE, FALSE, 0);
    g_signal_connect(app.level_combo, "changed", G_CALLBACK(level_changed), NULL);
    app.history_toggle_button = gtk_button_new_with_label("Hide Moves");
    gtk_box_pack_start(GTK_BOX(options_row), app.history_toggle_button, FALSE, FALSE, 0);
    g_signal_connect(app.history_toggle_button, "clicked", G_CALLBACK(toggle_history_cb), NULL);

    content = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(root), content, TRUE, TRUE, 0);

    app.board = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.board, 740, 740);
    gtk_widget_add_events(app.board, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(app.board, "expose-event", G_CALLBACK(board_expose), NULL);
    g_signal_connect(app.board, "button-press-event", G_CALLBACK(board_button), NULL);
    gtk_box_pack_start(GTK_BOX(content), app.board, TRUE, TRUE, 0);

    sidebar = gtk_vbox_new(FALSE, 8);
    app.history_sidebar = sidebar;
    app.history_visible = TRUE;
    gtk_widget_set_size_request(sidebar, 280, -1);
    gtk_box_pack_start(GTK_BOX(content), sidebar, FALSE, TRUE, 0);

    score_box = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(sidebar), score_box, FALSE, FALSE, 0);
    app.red_score = gtk_label_new("Red: 12");
    app.black_score = gtk_label_new("Black: 12");
    gtk_misc_set_alignment(GTK_MISC(app.red_score), 0.0f, 0.5f);
    gtk_misc_set_alignment(GTK_MISC(app.black_score), 0.0f, 0.5f);
    gtk_box_pack_start(GTK_BOX(score_box), app.red_score, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(score_box), app.black_score, FALSE, FALSE, 0);
    app_apply_high_contrast(app.red_score);
    app_apply_high_contrast(app.black_score);

    history_frame = gtk_frame_new("Moves");
    gtk_box_pack_start(GTK_BOX(sidebar), history_frame, TRUE, TRUE, 0);
    history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(history_frame), history_scroll);
    gtk_widget_set_size_request(history_scroll, 280, 640);
    app.moves_label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(app.moves_label), 0.0, 0.0);
    app_apply_high_contrast(app.moves_label);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(history_scroll), app.moves_label);

    history_nav_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), history_nav_box, FALSE, FALSE, 0);
    app.history_first_button = add_nav_button(history_nav_box, "|<", G_CALLBACK(history_first_cb));
    app.history_prev_button = add_nav_button(history_nav_box, "<", G_CALLBACK(history_prev_cb));
    app.history_next_button = add_nav_button(history_nav_box, ">", G_CALLBACK(history_next_cb));
    app.history_latest_button = add_nav_button(history_nav_box, ">|", G_CALLBACK(history_latest_cb));

    new_game();
    gtk_widget_show_all(app.window);
    app_log("started");
    gtk_main();
    free_assets();
    return 0;
}
