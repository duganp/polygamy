// othello.cpp

#include "shared.h"   // Precompiled header; obligatory
#include "othello.h"  // Our public interface

#define BLACK_SYMBOL '\xf9'  // Looks right in console window
#define WHITE_SYMBOL '\x4f'  // Looks right in console window


// Game registration stuff

GameState* OthelloGameState::creator()
{
    // FIXME temporary big fat hack:
    ComponentTraceBegin();
    TRACE(INFO, "Managed wrapper for Othello launched");

    return new OthelloGameState;
}

static int othello_registered =
    register_game
    (
        "Othello",
        OthelloGameState::creator
    );


Result OthelloGameState::set_initial_position(size_t position_size, __in_bcount(position_size) const char* position)
{
    static const size_t expected_size = (OTH_DIMENSION * OTH_DIMENSION + 10) * sizeof CellState;

    if (position == NULL || position_size < expected_size)
    {
        return Result::Fail;
    }

    m_initial_position = new char[expected_size];

    memcpy(m_initial_position, position, expected_size);

    reset();

    return Result::OK;  // FIXME: Could validate position data and convert it to a clearer internal form
}


void OthelloGameState::reset()
{
    GameState::reset();

    memset(m_move_history, 0, sizeof m_move_history);
    memset(m_value_history, 0, sizeof m_value_history);
    memset(m_player_cells_history, 0, sizeof m_player_cells_history);

    #if OTH_GAME_STATE_LIST
        for (int n = 0; n < countof(m_boards); ++n)
        {
            m_boards[n].m_status = Board::eDirty;
            memset(m_boards[n].m_cells, eEmpty, sizeof m_boards[n].m_cells);
        }
    #else
        memset(m_board, eEmpty, sizeof m_board);
        memset(m_undo_history, 0, sizeof m_undo_history);
    #endif

    if (m_initial_position)
    {
        const char* read_pointer = m_initial_position;
        int black_cells = 0, white_cells = 0;

        for (int i = 0; i < OTH_DIMENSION; ++i)
        {
            for (int j = 0; j < OTH_DIMENSION; ++j)
            {
                int symbol = toupper(*read_pointer++);
                if (symbol == 'X')
                {
                    m_board[i+1][j+1] = eBlack;
                    ++black_cells;
                }
                else if (symbol == 'O')
                {
                    m_board[i+1][j+1] = eWhite;
                    ++white_cells;
                }
            }
        }

        set_player_up(read_pointer[1] == 'W');  // Skip newline character
        m_player_cells_history[0][eBlack] = black_cells;
        m_player_cells_history[0][eWhite] = white_cells;
        m_cells_available = OTH_DIMENSION * OTH_DIMENSION - black_cells - white_cells;
    }
    else
    {
        cell(OTH_DIMENSION/2, OTH_DIMENSION/2) = cell(OTH_DIMENSION/2 + 1, OTH_DIMENSION/2 + 1) = eWhite;
        cell(OTH_DIMENSION/2, OTH_DIMENSION/2 + 1) = cell(OTH_DIMENSION/2 + 1, OTH_DIMENSION/2) = eBlack;
        m_cells_available = OTH_DIMENSION * OTH_DIMENSION - 4;
        m_player_cells_history[0][eBlack] = 2;
        m_player_cells_history[0][eWhite] = 2;
    }
}


GameMove* OthelloGameState::get_possible_moves() const
{
    #if RANDOMIZE
        int random_offset = rand();
    #endif

    GameMove move_array[OTH_DIMENSION * OTH_DIMENSION - 4];
    GameMove* current_move = move_array;
    int cells_left_to_inspect = OTH_DIMENSION * OTH_DIMENSION;
    int moves_found = 0;

    while (cells_left_to_inspect--)
    {
        #if RANDOMIZE
            int x = ((cells_left_to_inspect + random_offset) / OTH_DIMENSION) % OTH_DIMENSION + 1;
            int y = (cells_left_to_inspect + random_offset) % OTH_DIMENSION + 1;
        #else
            int x = cells_left_to_inspect / OTH_DIMENSION + 1;
            int y = cells_left_to_inspect % OTH_DIMENSION + 1;
        #endif

        if (cell(x, y) == eEmpty)
        {
            ++moves_found;
            *current_move++ = GameMove(x | (y << 16));  // Equivalent to Cell(x, y) but faster
        }
    }

    GameMove* possible_moves = new GameMove[moves_found + 1];
    memcpy(possible_moves, move_array, moves_found * sizeof GameMove);
    possible_moves[moves_found] = INVALID_MOVE;  // Terminate move list for caller convenience

    return possible_moves;
}


PlayerCode OthelloGameState::player_ahead() const
{
    int black = m_player_cells_history[move_counter()][eBlack];
    int white = m_player_cells_history[move_counter()][eWhite];
    return black > white ? eBlack : black < white ? eWhite : -1;
}


GameMove OthelloGameState::read_move(const char* move_string) const
{
    if (toupper(*move_string) == 'P')
    {
        return PASSING_MOVE;
    }
    else
    {
        int row = -1;
        char col = -1;
        sscanf_s(move_string, "%c%d", &col, 1, &row);
        return Cell(OTH_DIMENSION + 1 - row, toupper(col) + 1 - 'A');
    }
}


void OthelloGameState::write_move(GameMove move, int move_string_size,
                                  __in_ecount(move_string_size) char* move_string) const
{
    if (move == PASSING_MOVE)
    {
        sprintf_s(move_string, move_string_size, "Pass");
    }
    else
    {
        sprintf_s(move_string, move_string_size, "%c%d",
                  'A' + Cell(move).y - 1,
                  OTH_DIMENSION + 1 - Cell(move).x);
    }
}


bool OthelloGameState::valid_move(GameMove move)
{
    ASSERT(m_cells_available);

    if (move == PASSING_MOVE)
    {
        ASSERT(move_counter() == 0 || m_move_history[move_counter()-1] != PASSING_MOVE);

        GameMove* possible_moves = get_possible_moves();
        for (GameMove* m = possible_moves; *m; ++m)
        {
            if (apply_move(*m).ok())
            {
                undo_last_move();
                char move_string[MAX_MOVE_STRING_SIZE];
                write_move(*m, sizeof move_string, move_string);
                output("Cannot pass; at least one move is available (%s).\n", move_string);
                return false;
            }
        }
        return true;  // Passing allowed
    }

    // Return true if X and Y are on the board and the cell is available
    return Cell(move).x > 0 && Cell(move).x <= OTH_DIMENSION &&
           Cell(move).y > 0 && Cell(move).y <= OTH_DIMENSION &&
           cell(Cell(move).x, Cell(move).y) == eEmpty;
}


Result OthelloGameState::apply_move(GameMove move)
{
    ASSERT(move_counter() < countof(m_move_history));

    const short x = Cell(move).x;
    const short y = Cell(move).y;
    const PlayerCode opponent = (player_up() == eWhite) ? eBlack : eWhite;
    bool valid_move = false;
    int flipped_count = 0;

    #if OTH_GAME_STATE_LIST

        const CellState (&cur_board)[OTH_DIMENSION+2][OTH_DIMENSION+2] = m_boards[move_counter()].m_cells;
        CellState (&next_board)[OTH_DIMENSION+2][OTH_DIMENSION+2] = m_boards[move_counter()+1].m_cells;

        if (m_boards[move_counter()+1].m_status == Board::eDirty)
        {
            memcpy(next_board, cur_board, sizeof cur_board);
            m_boards[move_counter()+1].m_status = Board::eClean;
        }

        #define CHECK_DIRECTION(dx, dy)                                     \
        {                                                                   \
            int flipped = 0;                                                \
            for (int tx = x+dx, ty = y+dy;                                  \
                 cur_board[tx][ty] != eEmpty;  /* Sentinels keep us on the board */ \
                 tx += dx, ty += dy)                                        \
            {                                                               \
                if (cur_board[tx][ty] == opponent)                          \
                {                                                           \
                    ++flipped;                                              \
                }                                                           \
                else  /* cur_board[tx][ty] must be player_up() */           \
                {                                                           \
                    if (flipped)                                            \
                    {                                                       \
                        valid_move = true;                                  \
                        flipped_count += flipped;                           \
                        while (flipped--)                                   \
                        {                                                   \
                            tx -= dx; ty -= dy;                             \
                            next_board[tx][ty] = player_up();               \
                        }                                                   \
                    }                                                       \
                    break;                                                  \
                }                                                           \
            }                                                               \
        }

    #else // OTH_GAME_STATE_LIST

        UndoRecord& undo = m_undo_history[move_counter()];

        #define CHECK_DIRECTION(dx, dy)                                     \
        {                                                                   \
            int flipped = 0;                                                \
            int tx = x, ty = y;                                             \
            for (;;)                                                        \
            {                                                               \
                tx += dx, ty += dy;                                         \
                if (m_board[tx][ty] == opponent) ++flipped;                 \
                else break;                                                 \
            }                                                               \
            if (flipped && m_board[tx][ty] == player_up())                  \
            {                                                               \
                valid_move = true;                                          \
                while (flipped--)                                           \
                {                                                           \
                    tx -= dx; ty -= dy;                                     \
                    m_board[tx][ty] = player_up();                          \
                    undo.flipped_pieces[flipped_count].x = short(tx);       \
                    undo.flipped_pieces[flipped_count].y = short(ty);       \
                    ++flipped_count;                                        \
                }                                                           \
            }                                                               \
        }

    #endif // OTH_GAME_STATE_LIST

    // Iterate through the 8 possible directions
    CHECK_DIRECTION(-1, -1);
    CHECK_DIRECTION(-1,  0);
    CHECK_DIRECTION(-1,  1);
    CHECK_DIRECTION( 0, -1);
    CHECK_DIRECTION( 0,  1);
    CHECK_DIRECTION( 1, -1);
    CHECK_DIRECTION( 1,  0);
    CHECK_DIRECTION( 1,  1);

    if (valid_move)
    {
        m_move_history[move_counter()].x = x;
        m_move_history[move_counter()].y = y;
        advance_move_counter();

        #if OTH_GAME_STATE_LIST
            m_boards[move_counter()].m_cells[x][y] = player_up();
            m_boards[move_counter()].m_status = Board::eDirty;
            m_boards[move_counter()+1].m_status = Board::eDirty;
        #else
            // Add the new piece and update the undo history
            m_board[x][y] = player_up();
            undo.added_piece.x = x;
            undo.added_piece.y = y;
            undo.flipped_pieces[flipped_count].x = -1;  // End marker
        #endif

        m_player_cells_history[move_counter()][player_up()] = m_player_cells_history[move_counter()-1][player_up()] + flipped_count + 1;
        m_player_cells_history[move_counter()][opponent] = m_player_cells_history[move_counter()-1][opponent] - flipped_count;

        switch_player_up();
        --m_cells_available;

        return Result::OK;
    }

    return Result::Fail;
}


Result OthelloGameState::apply_passing_move()
{
    if (m_cells_available == 0 ||  // Must not be at end of game
        m_move_history[move_counter()-1] == PASSING_MOVE)  // Can't pass twice in a row
    {
        return Result::Fail;
    }

    #if OTH_GAME_STATE_LIST

        const CellState (&cur_board)[OTH_DIMENSION+2][OTH_DIMENSION+2] = m_boards[move_counter()].m_cells;
        CellState (&next_board)[OTH_DIMENSION+2][OTH_DIMENSION+2] = m_boards[move_counter()+1].m_cells;
        if (m_boards[move_counter()+1].m_status == Board::eDirty)
        {
            memcpy(next_board, cur_board, sizeof cur_board);
            m_boards[move_counter()+1].m_status = Board::eClean;
        }
        m_boards[move_counter()+2].m_status = Board::eDirty;

    #else

        UndoRecord& undo = m_undo_history[move_counter()];
        undo.added_piece = PASSING_MOVE;
        undo.flipped_pieces[0] = -1;  // End marker

    #endif

    m_move_history[move_counter()] = PASSING_MOVE;
    m_player_cells_history[move_counter()+1][eBlack] = m_player_cells_history[move_counter()][eBlack];
    m_player_cells_history[move_counter()+1][eWhite] = m_player_cells_history[move_counter()][eWhite];
    switch_player_up();
    advance_move_counter();

    return Result::OK;
}


void OthelloGameState::undo_last_move()
{
    ASSERT(move_counter() > 0);
    retreat_move_counter();

    if (m_move_history[move_counter()] != PASSING_MOVE)
    {
        #if OTH_GAME_STATE_LIST
            ASSERT(m_boards[move_counter()+1].m_status == Board::eDirty);
        #else
            UndoRecord& undo = m_undo_history[move_counter()];
            m_board[undo.added_piece.x][undo.added_piece.y] = eEmpty;
            Cell* cell = undo.flipped_pieces;
            do {m_board[cell->x][cell->y] = player_up(); ++cell;}
            while (cell->x != -1);
        #endif

        ++m_cells_available;
    }

    switch_player_up();
}


Result OthelloGameState::set_value_function(int n)
{
    if (n < eEvaluatorCount)
    {
        m_position_evaluator = Evaluator(n);
        return Result::OK;
    }

    return Result::Fail;
}


Value OthelloGameState::position_val() const
{
    // NOTE: Could easily expand this code to return game_over_val() if it detects
    // game end (no moves available and can't pass, etc), but it isn't necessary,
    // as game.cpp automatically converts the value returned here to game_over_val()
    // if it can't find any legal moves to apply.

    #if DISABLED_CODE  // This speeds things up by less than 0.05%
        // First a quick check for end-of-game conditions.  This gives a tiny
        // performance improvement vs. just letting game.cpp flag end-of-game
        // when it finds no valid moves.
        if (m_cells_available == 0 ||
            m_player_cells_history[move_counter()][eBlack] == 0 ||
            m_player_cells_history[move_counter()][eWhite] == 0)
        {
            return m_value_history[move_counter()] = game_over_val();
        }
    #endif

    // Factors used to calculate position value:
    // 1. Raw piece count (kicks in at end of game, outweighing everything else)
    // 2. Key square ownership
    // 3. Piece mobility

    Value value = 0;

    if (m_cells_available < g_current_search_depth - 4)  // FIXME: magic number.  Included because switching to the pure-piece-count eval adds several depth levels
    {
        value = m_player_cells_history[move_counter()][eBlack] - m_player_cells_history[move_counter()][eWhite];

        #if OTH_DISPLAY_EVALUATION
            output("\nPiece count %d (%d black - %d white)\n", value, m_player_cells_history[move_counter()][eBlack], m_player_cells_history[move_counter()][eWhite]);
        #endif
    }
    else
    {
        // Key square ownership

        int black_corners = (cell(1,1) == eBlack) + (cell(OTH_DIMENSION,OTH_DIMENSION) == eBlack) +
                            (cell(1,OTH_DIMENSION) == eBlack) + (cell(OTH_DIMENSION,1) == eBlack);

        int white_corners = (cell(1,1) == eWhite) + (cell(OTH_DIMENSION,OTH_DIMENSION) == eWhite) +
                            (cell(1,OTH_DIMENSION) == eWhite) + (cell(OTH_DIMENSION,1) == eWhite);

        int black_dangers = (cell(1,1) == eEmpty && cell(2,2) == eBlack) +
                            (cell(1,OTH_DIMENSION) == eEmpty && cell(2,OTH_DIMENSION-1) == eBlack) +
                            (cell(OTH_DIMENSION,1) == eEmpty && cell(OTH_DIMENSION-1,2) == eBlack) +
                            (cell(OTH_DIMENSION,OTH_DIMENSION) == eEmpty && cell(OTH_DIMENSION-1,OTH_DIMENSION-1) == eBlack);

        int white_dangers = (cell(1,1) == eEmpty && cell(2,2) == eWhite) +
                            (cell(1,OTH_DIMENSION) == eEmpty && cell(2,OTH_DIMENSION-1) == eWhite) +
                            (cell(OTH_DIMENSION,1) == eEmpty && cell(OTH_DIMENSION-1,2) == eWhite) +
                            (cell(OTH_DIMENSION,OTH_DIMENSION) == eEmpty && cell(OTH_DIMENSION-1,OTH_DIMENSION-1) == eWhite);

        value = 2000 * (black_corners - white_corners) - 1000 * (black_dangers - white_dangers);  // More magic numbers

        if (m_cells_available - g_current_search_depth - 10)  // FIXME: magic number 10
        {
            // Move availability

            #define TEST_MOVE(player, opponent, x, y, dx, dy, counter)              \
            {                                                                       \
                int flipped = 0, tx = x + dx, ty = y + dy;                          \
                while (cell(tx, ty) == opponent) {++flipped; tx += dx, ty += dy;}   \
                if (flipped && cell(tx, ty) == player) {++counter; continue;}       \
            }

            int black_move_count = 0, white_move_count = 0;
            int cells_left_to_inspect = OTH_DIMENSION * OTH_DIMENSION;

            while (cells_left_to_inspect--)
            {
                int x = cells_left_to_inspect / OTH_DIMENSION + 1;
                int y = cells_left_to_inspect % OTH_DIMENSION + 1;
                if (cell(x, y) == eEmpty)
                {
                    TEST_MOVE(eBlack, eWhite, x, y, -1, -1, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y, -1,  0, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y, -1,  1, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y,  0, -1, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y,  0,  1, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y,  1, -1, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y,  1,  0, black_move_count);
                    TEST_MOVE(eBlack, eWhite, x, y,  1,  1, black_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y, -1, -1, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y, -1,  0, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y, -1,  1, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y,  0, -1, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y,  0,  1, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y,  1, -1, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y,  1,  0, white_move_count);
                    TEST_MOVE(eWhite, eBlack, x, y,  1,  1, white_move_count);
                }
            }

            int mobility = black_move_count - white_move_count;
            value += 10 * mobility;

            #if OTH_DISPLAY_EVALUATION
                output("\nKey square score %d (black %d corners, %d dangers; white %d, %d)\n", key_squares, black_corners, black_dangers, white_corners, white_dangers);
                output("Mobility %d (%d moves available for black, %d for white)\n", mobility, black_move_count, white_move_count);
                output("==> Position value = %d (%d*1000 + %d*10)\n", value, key_squares, mobility);
            #endif
        }
    }

    return m_value_history[move_counter()] = value;
}


bool OthelloGameState::game_over()
{
    // See if there are any possible moves for the current player
    GameMove* possible_moves = get_possible_moves();

    for (GameMove* move = possible_moves; *move; ++move)
    {
        if (apply_move(*move).ok())
        {
            undo_last_move();
            return false;  // There is a move; game has not ended
        }
    }

    // If no moves were available, see if passing is possible
    if (apply_passing_move().ok())
    {
        bool game_is_over = game_over(); // No point allowing the pass move if
                                         // there are no further moves available
        undo_last_move();
        return game_is_over;
    }

    return true;
}


void OthelloGameState::display(size_t output_size, __out_ecount(output_size) char* output) const
{
    for (int i = 0; i < OTH_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  %c", i ? 'Ç' : 'É');
        for (int j = 0; j < OTH_DIMENSION; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "%s%c", i ? "ÄÄÄ" : "ÍÍÍ", i ? (j == OTH_DIMENSION-1 ? '¶' : 'Å')
                                                            : (j == OTH_DIMENSION-1 ? '»' : 'Ñ'));
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n%d", OTH_DIMENSION - i);
        for (int j = 0; j < OTH_DIMENSION; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               " %c %c", j ? '³' : 'º',
                               cell(i+1, j+1) == eBlack ? BLACK_SYMBOL :
                               cell(i+1, j+1) == eWhite ? WHITE_SYMBOL : ' ');
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " º\n");
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  È");
    for (int i = 0; i < OTH_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "ÍÍÍ%c", i == OTH_DIMENSION-1 ? '¼' : 'Ï');
    }

    StringCchPrintfExA(output, output_size, &output, &output_size, 0, " (move %d; ", move_counter());
    int black = m_player_cells_history[move_counter()][eBlack];
    int white = m_player_cells_history[move_counter()][eWhite];
    if (black == white)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "tied at %d cells each; ", black);
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s ahead by %d cells; ", black > white ? "black" : "white", abs(black - white));
    }
    Value value = position_val();
    StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                       "estimated value %d for %s)\n ", abs(value),
                       value > 0 ? "black" : value < 0 ? "white" : "both players");

    for (int i = 0; i < OTH_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "   %c", 'A' + i);
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n\n");
}


void OthelloGameState::display_score_sheet(bool include_moves, size_t output_size, __out_ecount(output_size) char* output) const
{
    int black = m_player_cells_history[move_counter()][eBlack];
    int white = m_player_cells_history[move_counter()][eWhite];

    if (black == white)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "Tie");
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s won by %d", black > white ? "Black" : "White", abs(black - white));
    }

    StringCchPrintfExA(output, output_size, &output, &output_size, 0, " (%d cells black, %d white).\n", black, white);

    if (include_moves)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "In %d moves:\n", move_counter());
        for (int n = 0; n < move_counter(); ++n)
        {
            char move_string[MAX_MOVE_STRING_SIZE];
            write_move(m_move_history[n], sizeof move_string, move_string);
            int pos_val = m_value_history[n+1];
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "\t%d. %s %s: score %d (%d cells black, %d white)\n",
                               n + 1, (n % 2) ? "White" : "Black", move_string, pos_val,
                               m_player_cells_history[n+1][eBlack],
                               m_player_cells_history[n+1][eWhite]);
        }

        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\nFinal board state:\n");
        display(output_size, output);
    }
}


const char* OthelloGameState::get_cell_state_image_name(int state) const
{
    ASSERT(state == eBlack || state == eWhite || state == eEmpty);

    return state == eBlack ? "OthelloBlack" :
           state == eWhite ? "OthelloWhite" : "OthelloEmpty";
}
