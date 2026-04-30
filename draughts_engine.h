/*
 * Kindle Draughts rules engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KINDLE_DRAUGHTS_ENGINE_H
#define KINDLE_DRAUGHTS_ENGINE_H

#include <glib.h>

#define DRAUGHTS_MAX_SIZE 10
#define DRAUGHTS_SIZE 8
#define DRAUGHTS_MAX_MOVES 240
#define DRAUGHTS_MAX_LEGAL_MOVES 256

typedef enum {
    DRAUGHTS_VARIANT_ENGLISH = 0,
    DRAUGHTS_VARIANT_INTERNATIONAL = 1
} DraughtsVariant;

typedef enum {
    DRAUGHTS_AI_EASY = 0,
    DRAUGHTS_AI_MEDIUM = 1,
    DRAUGHTS_AI_HARD = 2
} DraughtsAiLevel;

typedef enum {
    DRAUGHTS_EMPTY = 0,
    DRAUGHTS_RED_MAN,
    DRAUGHTS_RED_KING,
    DRAUGHTS_BLACK_MAN,
    DRAUGHTS_BLACK_KING
} DraughtsPiece;

typedef enum {
    DRAUGHTS_NO_PLAYER = 0,
    DRAUGHTS_RED = 1,
    DRAUGHTS_BLACK = 2
} DraughtsPlayer;

typedef enum {
    DRAUGHTS_INVALID = 0,
    DRAUGHTS_RUNNING,
    DRAUGHTS_CONTINUE_CAPTURE,
    DRAUGHTS_GAME_OVER
} DraughtsMoveState;

typedef struct {
    int from_x;
    int from_y;
    int to_x;
    int to_y;
    int capture_x;
    int capture_y;
    gboolean is_capture;
} DraughtsMove;

typedef struct {
    DraughtsVariant variant;
    int size;
    DraughtsPiece board[DRAUGHTS_MAX_SIZE][DRAUGHTS_MAX_SIZE];
    DraughtsPlayer turn;
    DraughtsPlayer winner;
    int red_count;
    int black_count;
    int move_count;
    gboolean must_continue_capture;
    int forced_x;
    int forced_y;
    char moves[DRAUGHTS_MAX_MOVES][32];
    DraughtsPiece history[DRAUGHTS_MAX_MOVES + 1][DRAUGHTS_MAX_SIZE][DRAUGHTS_MAX_SIZE];
    DraughtsPlayer turn_history[DRAUGHTS_MAX_MOVES + 1];
    DraughtsPlayer winner_history[DRAUGHTS_MAX_MOVES + 1];
    gboolean continue_history[DRAUGHTS_MAX_MOVES + 1];
    int forced_x_history[DRAUGHTS_MAX_MOVES + 1];
    int forced_y_history[DRAUGHTS_MAX_MOVES + 1];
} DraughtsGame;

void draughts_game_init(DraughtsGame *game);
void draughts_game_init_variant(DraughtsGame *game, DraughtsVariant variant);
DraughtsPlayer draughts_other_player(DraughtsPlayer player);
gboolean draughts_piece_belongs_to(DraughtsPiece piece, DraughtsPlayer player);
gboolean draughts_piece_is_king(DraughtsPiece piece);
int draughts_collect_moves_for_piece(const DraughtsGame *game, int x, int y, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES]);
int draughts_collect_legal_moves(const DraughtsGame *game, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES]);
gboolean draughts_has_capture(const DraughtsGame *game, DraughtsPlayer player);
gboolean draughts_is_legal_move(const DraughtsGame *game, const DraughtsMove *move);
DraughtsMoveState draughts_apply_move(DraughtsGame *game, const DraughtsMove *move);
gboolean draughts_ai_pick_move(const DraughtsGame *game, DraughtsMove *move);
gboolean draughts_ai_pick_move_level(const DraughtsGame *game, DraughtsAiLevel level, DraughtsMove *move);
gboolean draughts_undo(DraughtsGame *game);
const char *draughts_player_name(DraughtsPlayer player);
const char *draughts_variant_name(DraughtsVariant variant);
void draughts_update_counts(DraughtsGame *game);

#endif
