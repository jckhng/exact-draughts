/*
 * Kindle Draughts smoke tests
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "draughts_engine.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "smoke-test: %s\n", message);
    return 1;
}

static void clear_board(DraughtsGame *game)
{
    memset(game->board, 0, sizeof(game->board));
    game->winner = DRAUGHTS_NO_PLAYER;
    game->must_continue_capture = FALSE;
    game->forced_x = -1;
    game->forced_y = -1;
    game->move_count = 0;
}

int main(void)
{
    DraughtsGame game;
    DraughtsMove moves[DRAUGHTS_MAX_LEGAL_MOVES];
    DraughtsMove move;
    int count;

    draughts_game_init(&game);

    if (game.red_count != 12 || game.black_count != 12)
        return fail("initial counts are wrong");
    if (game.turn != DRAUGHTS_RED)
        return fail("red should move first");

    count = draughts_collect_legal_moves(&game, moves);
    if (count != 7)
        return fail("red should have seven opening moves");

    move.from_x = 0;
    move.from_y = 5;
    move.to_x = 1;
    move.to_y = 4;
    move.is_capture = FALSE;
    if (draughts_apply_move(&game, &move) != DRAUGHTS_RUNNING)
        return fail("failed to apply opening move");
    if (game.turn != DRAUGHTS_BLACK)
        return fail("turn did not switch to black");

    if (!draughts_undo(&game) || game.turn != DRAUGHTS_RED || game.red_count != 12 || game.black_count != 12)
        return fail("undo did not restore opening state");

    draughts_game_init(&game);
    clear_board(&game);
    game.turn = DRAUGHTS_RED;
    game.board[5][0] = DRAUGHTS_RED_MAN;
    game.board[4][1] = DRAUGHTS_BLACK_MAN;
    game.board[2][3] = DRAUGHTS_BLACK_MAN;
    draughts_update_counts(&game);

    count = draughts_collect_legal_moves(&game, moves);
    if (count != 1 || !moves[0].is_capture || moves[0].to_x != 2 || moves[0].to_y != 3)
        return fail("forced capture was not generated");

    if (draughts_apply_move(&game, &moves[0]) != DRAUGHTS_CONTINUE_CAPTURE)
        return fail("multi-capture continuation was not detected");
    if (!game.must_continue_capture || game.forced_x != 2 || game.forced_y != 3)
        return fail("forced continuation square is wrong");

    count = draughts_collect_legal_moves(&game, moves);
    if (count != 1 || moves[0].to_x != 4 || moves[0].to_y != 1)
        return fail("second jump was not generated");

    if (draughts_apply_move(&game, &moves[0]) != DRAUGHTS_GAME_OVER)
        return fail("final capture should end the game");
    if (game.winner != DRAUGHTS_RED)
        return fail("red should win after capturing all black pieces");

    draughts_game_init(&game);
    clear_board(&game);
    game.turn = DRAUGHTS_RED;
    game.board[1][2] = DRAUGHTS_RED_MAN;
    draughts_update_counts(&game);
    move.from_x = 2;
    move.from_y = 1;
    move.to_x = 1;
    move.to_y = 0;
    move.is_capture = FALSE;
    if (draughts_apply_move(&game, &move) != DRAUGHTS_GAME_OVER)
        return fail("promotion move failed");
    if (game.board[0][1] != DRAUGHTS_RED_KING)
        return fail("red man was not promoted");

    draughts_game_init(&game);
    clear_board(&game);
    game.turn = DRAUGHTS_RED;
    game.board[5][0] = DRAUGHTS_RED_MAN;
    game.board[5][4] = DRAUGHTS_RED_MAN;
    game.board[4][1] = DRAUGHTS_BLACK_MAN;
    draughts_update_counts(&game);
    if (!draughts_ai_pick_move(&game, &move))
        return fail("AI did not find a move");
    if (!move.is_capture || move.from_x != 0 || move.from_y != 5 || move.to_x != 2 || move.to_y != 3)
        return fail("AI should choose the mandatory capture");

    draughts_game_init(&game);
    clear_board(&game);
    game.turn = DRAUGHTS_RED;
    game.board[1][2] = DRAUGHTS_RED_MAN;
    game.board[5][4] = DRAUGHTS_RED_MAN;
    draughts_update_counts(&game);
    if (!draughts_ai_pick_move(&game, &move))
        return fail("AI did not find a promotion move");
    if (move.from_x != 2 || move.from_y != 1 || move.to_y != 0)
        return fail("AI should prefer promotion");

    draughts_game_init_variant(&game, DRAUGHTS_VARIANT_INTERNATIONAL);
    if (game.size != 10 || game.red_count != 20 || game.black_count != 20)
        return fail("international setup is wrong");
    count = draughts_collect_legal_moves(&game, moves);
    if (count != 9)
        return fail("international red should have nine opening moves");

    clear_board(&game);
    game.variant = DRAUGHTS_VARIANT_INTERNATIONAL;
    game.size = 10;
    game.turn = DRAUGHTS_RED;
    game.board[5][2] = DRAUGHTS_RED_MAN;
    game.board[6][3] = DRAUGHTS_BLACK_MAN;
    draughts_update_counts(&game);
    count = draughts_collect_legal_moves(&game, moves);
    if (count != 1 || !moves[0].is_capture || moves[0].to_x != 4 || moves[0].to_y != 7)
        return fail("international men should capture backwards");

    clear_board(&game);
    game.variant = DRAUGHTS_VARIANT_INTERNATIONAL;
    game.size = 10;
    game.turn = DRAUGHTS_RED;
    game.board[7][2] = DRAUGHTS_RED_KING;
    game.board[5][4] = DRAUGHTS_BLACK_MAN;
    draughts_update_counts(&game);
    count = draughts_collect_legal_moves(&game, moves);
    if (count < 4)
        return fail("international flying king captures were not generated");

    clear_board(&game);
    game.variant = DRAUGHTS_VARIANT_INTERNATIONAL;
    game.size = 10;
    game.turn = DRAUGHTS_RED;
    game.board[7][0] = DRAUGHTS_RED_MAN;
    game.board[6][1] = DRAUGHTS_BLACK_MAN;
    game.board[4][3] = DRAUGHTS_BLACK_MAN;
    game.board[6][5] = DRAUGHTS_BLACK_MAN;
    draughts_update_counts(&game);
    count = draughts_collect_legal_moves(&game, moves);
    if (count != 1 || moves[0].to_x != 2 || moves[0].to_y != 5)
        return fail("international should filter to longest capture line");

    puts("smoke-test: ok");
    return 0;
}
