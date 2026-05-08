/*
 * Exact Draughts rules engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Kindle-specific implementation for the Exact Draughts adaptation. Project
 * attribution and bundled artwork provenance are documented in
 * docs/PROVENANCE.md.
 */

#include "draughts_engine.h"

#include <stdio.h>
#include <string.h>

static const int dirs[4][2] = {
    {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
};

static gboolean in_game_bounds(const DraughtsGame *game, int x, int y)
{
    return x >= 0 && x < game->size && y >= 0 && y < game->size;
}

static gboolean is_dark_square(int x, int y)
{
    return ((x + y) & 1) == 1;
}

static DraughtsPlayer piece_owner(DraughtsPiece piece)
{
    if (piece == DRAUGHTS_RED_MAN || piece == DRAUGHTS_RED_KING)
        return DRAUGHTS_RED;
    if (piece == DRAUGHTS_BLACK_MAN || piece == DRAUGHTS_BLACK_KING)
        return DRAUGHTS_BLACK;
    return 0;
}

static gboolean direction_allowed(DraughtsPiece piece, int dy)
{
    if (draughts_piece_is_king(piece))
        return TRUE;
    if (piece == DRAUGHTS_RED_MAN)
        return dy < 0;
    if (piece == DRAUGHTS_BLACK_MAN)
        return dy > 0;
    return FALSE;
}

static gboolean capture_direction_allowed(const DraughtsGame *game, DraughtsPiece piece, int dy)
{
    if (game->variant == DRAUGHTS_VARIANT_INTERNATIONAL)
        return TRUE;
    return direction_allowed(piece, dy);
}

static void save_history(DraughtsGame *game)
{
    if (game->move_count > DRAUGHTS_MAX_MOVES)
        return;

    memcpy(game->history[game->move_count], game->board, sizeof(game->board));
    game->turn_history[game->move_count] = game->turn;
    game->winner_history[game->move_count] = game->winner;
    game->continue_history[game->move_count] = game->must_continue_capture;
    game->forced_x_history[game->move_count] = game->forced_x;
    game->forced_y_history[game->move_count] = game->forced_y;
}

void draughts_update_counts(DraughtsGame *game)
{
    int x;
    int y;

    game->red_count = 0;
    game->black_count = 0;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (draughts_piece_belongs_to(game->board[y][x], DRAUGHTS_RED))
                game->red_count++;
            else if (draughts_piece_belongs_to(game->board[y][x], DRAUGHTS_BLACK))
                game->black_count++;
        }
    }
}

void draughts_game_init(DraughtsGame *game)
{
    draughts_game_init_variant(game, DRAUGHTS_VARIANT_ENGLISH);
}

void draughts_game_init_variant(DraughtsGame *game, DraughtsVariant variant)
{
    int x;
    int y;
    int rows;

    memset(game, 0, sizeof(*game));
    game->variant = variant;
    game->size = variant == DRAUGHTS_VARIANT_INTERNATIONAL ? 10 : 8;
    rows = variant == DRAUGHTS_VARIANT_INTERNATIONAL ? 4 : 3;

    for (y = 0; y < rows; y++) {
        for (x = 0; x < game->size; x++) {
            if (is_dark_square(x, y))
                game->board[y][x] = DRAUGHTS_BLACK_MAN;
        }
    }

    for (y = game->size - rows; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (is_dark_square(x, y))
                game->board[y][x] = DRAUGHTS_RED_MAN;
        }
    }

    game->turn = DRAUGHTS_RED;
    game->winner = DRAUGHTS_NO_PLAYER;
    game->forced_x = -1;
    game->forced_y = -1;
    draughts_update_counts(game);
    save_history(game);
}

DraughtsPlayer draughts_other_player(DraughtsPlayer player)
{
    return player == DRAUGHTS_RED ? DRAUGHTS_BLACK : DRAUGHTS_RED;
}

gboolean draughts_piece_belongs_to(DraughtsPiece piece, DraughtsPlayer player)
{
    return piece_owner(piece) == player;
}

gboolean draughts_piece_is_king(DraughtsPiece piece)
{
    return piece == DRAUGHTS_RED_KING || piece == DRAUGHTS_BLACK_KING;
}

static int collect_captures_for_piece(const DraughtsGame *game, int x, int y, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES])
{
    DraughtsPiece piece = game->board[y][x];
    DraughtsPlayer player = piece_owner(piece);
    DraughtsPlayer other = draughts_other_player(player);
    int count = 0;
    int i;

    if (piece == DRAUGHTS_EMPTY)
        return 0;

    for (i = 0; i < 4; i++) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];

        if (!capture_direction_allowed(game, piece, dy))
            continue;

        if (game->variant == DRAUGHTS_VARIANT_INTERNATIONAL && draughts_piece_is_king(piece)) {
            int sx = x + dx;
            int sy = y + dy;
            int cx = -1;
            int cy = -1;

            while (in_game_bounds(game, sx, sy)) {
                DraughtsPiece seen = game->board[sy][sx];
                if (seen == DRAUGHTS_EMPTY) {
                    if (cx != -1 && count < DRAUGHTS_MAX_LEGAL_MOVES) {
                        moves[count].from_x = x;
                        moves[count].from_y = y;
                        moves[count].to_x = sx;
                        moves[count].to_y = sy;
                        moves[count].capture_x = cx;
                        moves[count].capture_y = cy;
                        moves[count].is_capture = TRUE;
                        count++;
                    }
                } else if (draughts_piece_belongs_to(seen, player)) {
                    break;
                } else {
                    if (cx != -1)
                        break;
                    cx = sx;
                    cy = sy;
                }
                sx += dx;
                sy += dy;
            }
        } else {
            int mx = x + dx;
            int my = y + dy;
            int tx = x + dx * 2;
            int ty = y + dy * 2;

            if (!in_game_bounds(game, tx, ty) || game->board[ty][tx] != DRAUGHTS_EMPTY)
                continue;
            if (!draughts_piece_belongs_to(game->board[my][mx], other))
                continue;

            moves[count].from_x = x;
            moves[count].from_y = y;
            moves[count].to_x = tx;
            moves[count].to_y = ty;
            moves[count].capture_x = mx;
            moves[count].capture_y = my;
            moves[count].is_capture = TRUE;
            count++;
        }
    }

    return count;
}

gboolean draughts_has_capture(const DraughtsGame *game, DraughtsPlayer player)
{
    DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES];
    int x;
    int y;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (draughts_piece_belongs_to(game->board[y][x], player) &&
                collect_captures_for_piece(game, x, y, moves) > 0) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static int max_capture_chain_after(DraughtsGame game, const DraughtsMove *move)
{
    DraughtsMove next[DRAUGHTS_MAX_LEGAL_MOVES];
    int count;
    int best = 0;
    int i;

    if (!move->is_capture)
        return 0;

    game.board[move->to_y][move->to_x] = game.board[move->from_y][move->from_x];
    game.board[move->from_y][move->from_x] = DRAUGHTS_EMPTY;
    game.board[move->capture_y][move->capture_x] = DRAUGHTS_EMPTY;

    count = collect_captures_for_piece(&game, move->to_x, move->to_y, next);
    for (i = 0; i < count; i++) {
        int depth = 1 + max_capture_chain_after(game, &next[i]);
        if (depth > best)
            best = depth;
    }

    return best;
}

static int filter_longest_captures(const DraughtsGame *game, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES], int count)
{
    int lengths[DRAUGHTS_MAX_LEGAL_MOVES];
    int best = 0;
    int out = 0;
    int i;

    if (game->variant != DRAUGHTS_VARIANT_INTERNATIONAL || count <= 1)
        return count;

    for (i = 0; i < count; i++) {
        lengths[i] = moves[i].is_capture ? 1 + max_capture_chain_after(*game, &moves[i]) : 0;
        if (lengths[i] > best)
            best = lengths[i];
    }
    if (best <= 0)
        return count;

    for (i = 0; i < count; i++) {
        if (lengths[i] == best)
            moves[out++] = moves[i];
    }

    return out;
}

int draughts_collect_moves_for_piece(const DraughtsGame *game, int x, int y, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES])
{
    DraughtsPiece piece;
    gboolean any_capture;
    int count = 0;
    int i;

    if (!in_game_bounds(game, x, y))
        return 0;

    piece = game->board[y][x];
    if (!draughts_piece_belongs_to(piece, game->turn))
        return 0;

    if (game->must_continue_capture && (x != game->forced_x || y != game->forced_y))
        return 0;

    count = collect_captures_for_piece(game, x, y, moves);
    if (count > 0 || game->must_continue_capture)
        return filter_longest_captures(game, moves, count);

    any_capture = draughts_has_capture(game, game->turn);
    if (any_capture)
        return 0;

    for (i = 0; i < 4; i++) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int tx = x + dx;
        int ty = y + dy;

        if (!direction_allowed(piece, dy))
            continue;
        if (!in_game_bounds(game, tx, ty) || game->board[ty][tx] != DRAUGHTS_EMPTY)
            continue;

        moves[count].from_x = x;
        moves[count].from_y = y;
        moves[count].to_x = tx;
        moves[count].to_y = ty;
        moves[count].capture_x = -1;
        moves[count].capture_y = -1;
        moves[count].is_capture = FALSE;
        count++;

        if (game->variant == DRAUGHTS_VARIANT_INTERNATIONAL && draughts_piece_is_king(piece)) {
            tx += dx;
            ty += dy;
            while (in_game_bounds(game, tx, ty) && game->board[ty][tx] == DRAUGHTS_EMPTY && count < DRAUGHTS_MAX_LEGAL_MOVES) {
                moves[count].from_x = x;
                moves[count].from_y = y;
                moves[count].to_x = tx;
                moves[count].to_y = ty;
                moves[count].capture_x = -1;
                moves[count].capture_y = -1;
                moves[count].is_capture = FALSE;
                count++;
                tx += dx;
                ty += dy;
            }
        }
    }

    return count;
}

int draughts_collect_legal_moves(const DraughtsGame *game, DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES])
{
    int count = 0;
    int x;
    int y;

    if (game->winner != DRAUGHTS_NO_PLAYER)
        return 0;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            if (draughts_piece_belongs_to(game->board[y][x], game->turn)) {
                DraughtsMove piece_moves[DRAUGHTS_MAX_LEGAL_MOVES];
                int piece_count = draughts_collect_moves_for_piece(game, x, y, piece_moves);
                int i;

                for (i = 0; i < piece_count && count < DRAUGHTS_MAX_LEGAL_MOVES; i++)
                    moves[count++] = piece_moves[i];
            }
        }
    }

    return filter_longest_captures(game, moves, count);
}

gboolean draughts_is_legal_move(const DraughtsGame *game, const DraughtsMove *move)
{
    DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES];
    int count = draughts_collect_moves_for_piece(game, move->from_x, move->from_y, moves);
    int i;

    for (i = 0; i < count; i++) {
        if (moves[i].to_x == move->to_x && moves[i].to_y == move->to_y &&
            moves[i].from_x == move->from_x && moves[i].from_y == move->from_y) {
            return TRUE;
        }
    }

    return FALSE;
}

static void promote_if_needed(DraughtsGame *game, int x, int y)
{
    if (game->board[y][x] == DRAUGHTS_RED_MAN && y == 0)
        game->board[y][x] = DRAUGHTS_RED_KING;
    else if (game->board[y][x] == DRAUGHTS_BLACK_MAN && y == game->size - 1)
        game->board[y][x] = DRAUGHTS_BLACK_KING;
}

DraughtsMoveState draughts_apply_move(DraughtsGame *game, const DraughtsMove *move)
{
    DraughtsPiece piece;
    DraughtsMove captures[DRAUGHTS_MAX_LEGAL_MOVES];
    int more_captures;

    if (game->winner != DRAUGHTS_NO_PLAYER || !draughts_is_legal_move(game, move))
        return DRAUGHTS_INVALID;

    piece = game->board[move->from_y][move->from_x];
    game->board[move->from_y][move->from_x] = DRAUGHTS_EMPTY;
    game->board[move->to_y][move->to_x] = piece;

    if (move->is_capture)
        game->board[move->capture_y][move->capture_x] = DRAUGHTS_EMPTY;

    promote_if_needed(game, move->to_x, move->to_y);
    draughts_update_counts(game);

    if (game->move_count < DRAUGHTS_MAX_MOVES) {
        snprintf(game->moves[game->move_count], sizeof(game->moves[game->move_count]),
                 "%c%d-%c%d%s",
                 'a' + move->from_x, game->size - move->from_y,
                 'a' + move->to_x, game->size - move->to_y,
                 move->is_capture ? "x" : "");
    }
    game->move_count++;

    game->must_continue_capture = FALSE;
    game->forced_x = -1;
    game->forced_y = -1;

    if (move->is_capture) {
        more_captures = collect_captures_for_piece(game, move->to_x, move->to_y, captures);
        if (more_captures > 0) {
            game->must_continue_capture = TRUE;
            game->forced_x = move->to_x;
            game->forced_y = move->to_y;
            save_history(game);
            return DRAUGHTS_CONTINUE_CAPTURE;
        }
    }

    if (game->red_count == 0)
        game->winner = DRAUGHTS_BLACK;
    else if (game->black_count == 0)
        game->winner = DRAUGHTS_RED;
    else
        game->turn = draughts_other_player(game->turn);

    if (game->winner == DRAUGHTS_NO_PLAYER) {
        DraughtsMove legal[DRAUGHTS_MAX_LEGAL_MOVES];
        if (draughts_collect_legal_moves(game, legal) == 0)
            game->winner = draughts_other_player(game->turn);
    }

    save_history(game);
    return game->winner == DRAUGHTS_NO_PLAYER ? DRAUGHTS_RUNNING : DRAUGHTS_GAME_OVER;
}

static int piece_value(DraughtsPiece piece)
{
    if (piece == DRAUGHTS_RED_KING || piece == DRAUGHTS_BLACK_KING)
        return 500;
    if (piece == DRAUGHTS_RED_MAN || piece == DRAUGHTS_BLACK_MAN)
        return 300;
    return 0;
}

static int evaluate_position(const DraughtsGame *game, DraughtsPlayer player)
{
    int x;
    int y;
    int score = 0;

    if (game->winner == player)
        return 100000;
    if (game->winner == draughts_other_player(player))
        return -100000;

    for (y = 0; y < game->size; y++) {
        for (x = 0; x < game->size; x++) {
            DraughtsPiece piece = game->board[y][x];
            DraughtsPlayer owner = piece_owner(piece);
            int value;

            if (owner == 0)
                continue;

            value = piece_value(piece);
            if (!draughts_piece_is_king(piece)) {
                if (piece == DRAUGHTS_RED_MAN)
                    value += (game->size - 1 - y) * 18;
                else
                    value += y * 18;
            }
            if (x > 0 && x < game->size - 1)
                value += 8;

            score += owner == player ? value : -value;
        }
    }

    return score;
}

static int search_position(const DraughtsGame *game, DraughtsPlayer player, int depth, int alpha, int beta)
{
    DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES];
    int count;
    int i;

    if (depth <= 0 || game->winner != DRAUGHTS_NO_PLAYER)
        return evaluate_position(game, player);

    count = draughts_collect_legal_moves(game, moves);
    if (count <= 0)
        return evaluate_position(game, player);

    if (game->turn == player) {
        int best = -1000000;
        for (i = 0; i < count; i++) {
            DraughtsGame trial = *game;
            int score;
            draughts_apply_move(&trial, &moves[i]);
            score = search_position(&trial, player, depth - 1, alpha, beta);
            if (score > best)
                best = score;
            if (best > alpha)
                alpha = best;
            if (alpha >= beta)
                break;
        }
        return best;
    } else {
        int best = 1000000;
        for (i = 0; i < count; i++) {
            DraughtsGame trial = *game;
            int score;
            draughts_apply_move(&trial, &moves[i]);
            score = search_position(&trial, player, depth - 1, alpha, beta);
            if (score < best)
                best = score;
            if (best < beta)
                beta = best;
            if (alpha >= beta)
                break;
        }
        return best;
    }
}

static int ai_move_score(const DraughtsGame *game, const DraughtsMove *move)
{
    DraughtsGame trial = *game;
    DraughtsPiece piece = game->board[move->from_y][move->from_x];
    DraughtsPiece captured = DRAUGHTS_EMPTY;
    DraughtsMove followups[DRAUGHTS_MAX_LEGAL_MOVES];
    int score = 0;
    int followup_count;

    if (move->is_capture) {
        captured = game->board[move->capture_y][move->capture_x];
        score += 1000 + piece_value(captured) * 50;
    }

    if (!draughts_piece_is_king(piece) &&
        ((piece == DRAUGHTS_RED_MAN && move->to_y == 0) ||
         (piece == DRAUGHTS_BLACK_MAN && move->to_y == game->size - 1))) {
        score += 350;
    }

    if (draughts_piece_is_king(piece))
        score += 90;

    if (draughts_apply_move(&trial, move) == DRAUGHTS_CONTINUE_CAPTURE) {
        followup_count = collect_captures_for_piece(&trial, trial.forced_x, trial.forced_y, followups);
        score += 250 + followup_count * 80;
    }

    if (!draughts_piece_is_king(piece)) {
        if (piece == DRAUGHTS_RED_MAN)
            score += (move->from_y - move->to_y) * 12;
        else
            score += (move->to_y - move->from_y) * 12;
    }

    if (move->to_x > 0 && move->to_x < game->size - 1)
        score += 8;

    /* Stable tie-breaker keeps AI deterministic without random state. */
    score += (game->size - move->to_y) * 2 + move->to_x;

    return score;
}

gboolean draughts_ai_pick_move_level(const DraughtsGame *game, DraughtsAiLevel level, DraughtsMove *move)
{
    DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES];
    int count = draughts_collect_legal_moves(game, moves);
    int best_score = -1000000;
    int best_index = -1;
    DraughtsPlayer player = game->turn;
    int depth = 3;
    int i;

    if (count <= 0)
        return FALSE;

    if (level == DRAUGHTS_AI_EASY)
        depth = 2;
    else if (level == DRAUGHTS_AI_MEDIUM)
        depth = 4;
    else
        depth = game->variant == DRAUGHTS_VARIANT_INTERNATIONAL ? 5 : 6;

    for (i = 0; i < count; i++) {
        DraughtsGame trial = *game;
        int score;
        draughts_apply_move(&trial, &moves[i]);
        score = search_position(&trial, player, depth - 1, -1000000, 1000000);
        score += ai_move_score(game, &moves[i]) / 10;
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    if (best_index < 0)
        return FALSE;

    *move = moves[best_index];
    return TRUE;
}

gboolean draughts_ai_pick_move(const DraughtsGame *game, DraughtsMove *move)
{
    return draughts_ai_pick_move_level(game, DRAUGHTS_AI_MEDIUM, move);
}

gboolean draughts_undo(DraughtsGame *game)
{
    if (game->move_count <= 0)
        return FALSE;

    game->move_count--;
    memcpy(game->board, game->history[game->move_count], sizeof(game->board));
    game->turn = game->turn_history[game->move_count];
    game->winner = game->winner_history[game->move_count];
    game->must_continue_capture = game->continue_history[game->move_count];
    game->forced_x = game->forced_x_history[game->move_count];
    game->forced_y = game->forced_y_history[game->move_count];
    memset(game->moves[game->move_count], 0, sizeof(game->moves[game->move_count]));
    draughts_update_counts(game);
    return TRUE;
}

const char *draughts_player_name(DraughtsPlayer player)
{
    return player == DRAUGHTS_RED ? "Red" : "Black";
}

const char *draughts_variant_name(DraughtsVariant variant)
{
    return variant == DRAUGHTS_VARIANT_INTERNATIONAL ? "International" : "English";
}
