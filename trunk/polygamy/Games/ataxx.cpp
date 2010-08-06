// ataxx.cpp

#include "shared.h"  // Precompiled header; obligatory
#include "ataxx.h"   // Our public interface

#define RED_SYMBOL " \xf9 "   // Looks right in console window
#define BLUE_SYMBOL " \x4f "  // Looks right in console window


// Game registration stuff

GameState* AtaxxGameState::creator() {return new AtaxxGameState;}

static int ataxx_registered =
    register_game
    (
        "Ataxx",
        AtaxxGameState::creator
    );


Result AtaxxGameState::set_initial_position(size_t position_size, __in_bcount(position_size) const char* position)
{
    static const size_t expected_size = (ATAXX_COLUMNS + 1) * ATAXX_ROWS;  // The +1 allows for newline characters

    if (position == NULL || position_size < expected_size)
    {
        return Result::Fail;
    }

    m_initial_position = new char[expected_size];
    // FIXME: maybe make this a fixed-size array in AtaxxGameState and drop the 'new'

    memcpy(m_initial_position, position, expected_size);

    reset();

    return Result::OK;  // FIXME: Could validate position data and convert it to a clearer internal form
}


void AtaxxGameState::reset()
{
    GameState::reset();

    memset(m_move_history, 0, sizeof m_move_history);
    memset(m_player_cells_history, 0, sizeof m_player_cells_history);

    for (int n = 0; n < countof(m_boards); ++n)  // FIXME: Probably don't need to set up EVERY board (same for Othello)
    {
        Board& brd = m_boards[n];  // Abbreviation
        brd.m_status = Board::eDirty;
        memset(brd.m_cells, eEmpty, sizeof brd.m_cells);

        // Set up a double layer of sentinel cells around the board
        for (int x = 0; x < countof(brd.m_cells); ++x)
        {
            for (int y = 0; y < countof(brd.m_cells[x]); ++y)
            {
                brd.m_cells[x][0] = brd.m_cells[x][ATAXX_ROWS+2] =
                brd.m_cells[x][1] = brd.m_cells[x][ATAXX_ROWS+3] =
                brd.m_cells[0][y] = brd.m_cells[ATAXX_COLUMNS+2][y] =
                brd.m_cells[1][y] = brd.m_cells[ATAXX_COLUMNS+3][y] =
                eBlocked;
            }
        }
    }

    if (m_initial_position)
    {
        const char* read_pointer = m_initial_position;
        int blue_cells = 0, red_cells = 0, blocked_cells = 0;

        for (int row = 0; row < ATAXX_ROWS; ++row)
        {
            for (int col = 0; col < ATAXX_COLUMNS; ++col)
            {
                int symbol = toupper(*read_pointer++);
                if (symbol == 'B')
                {
                    cell(col+2, ATAXX_ROWS-row+1) = eBlue;
                    ++blue_cells;
                }
                else if (symbol == 'R')
                {
                    cell(col+2, ATAXX_ROWS-row+1) = eRed;
                    ++red_cells;
                }
                else if (symbol == 'X')
                {
                    cell(col+2, ATAXX_ROWS-row+1) = eBlocked;
                    ++blocked_cells;
                }
                else
                {
                    cell(col+2, ATAXX_ROWS-row+1) = eEmpty;
                }
            }
            ++read_pointer;  // Skip newline character
        }
        m_player_cells_history[0][eBlue] = blue_cells;
        m_player_cells_history[0][eRed] = red_cells;
        m_cells_available = ATAXX_COLUMNS * ATAXX_ROWS - blue_cells - red_cells - blocked_cells;
    }
    else
    {
        cell(2, 2) = cell(ATAXX_COLUMNS+1, ATAXX_ROWS+1) = eRed;
        cell(ATAXX_COLUMNS+1, 2) = cell(2, ATAXX_ROWS+1) = eBlue;
        m_player_cells_history[0][eBlue] = 2;
        m_player_cells_history[0][eRed] = 2;
        m_cells_available = ATAXX_COLUMNS * ATAXX_ROWS - 4;
    }
}


GameMove* AtaxxGameState::get_possible_moves() const
{
    // Possible moves < cells on board * 24 places each can be approached from (5*5-1)
    static GameMove move_array[ATAXX_COLUMNS * ATAXX_ROWS * 24];
    int moves_found = 0;

    if (m_player_cells_history[move_counter()][eBlue] != 0 &&
        m_player_cells_history[move_counter()][eRed] != 0 &&
        m_cells_available != 0)
    {
        #if RANDOMIZE
            int random_offset = rand();
        #endif

        GameMove* current_move = move_array;
        int cells_left_to_inspect = ATAXX_COLUMNS * ATAXX_ROWS;

        while (cells_left_to_inspect--)
        {
            #if RANDOMIZE
                int x = ((cells_left_to_inspect + random_offset) / ATAXX_ROWS) % ATAXX_COLUMNS + 2;
                int y = (cells_left_to_inspect + random_offset) % ATAXX_ROWS + 2;
            #else
                int x = cells_left_to_inspect / ATAXX_ROWS + 2;
                int y = cells_left_to_inspect % ATAXX_ROWS + 2;
            #endif

            if (cell(x, y) == eEmpty)
            {
                // First look for jump moves from any cell two steps away
                #define CHECK_JUMP_MOVE(i, j) \
                    if (cell(i, j) == player_up()) \
                        {++moves_found; *current_move++ = encode_move(i, j, x, y);}

                CHECK_JUMP_MOVE(x-2, y-2);
                CHECK_JUMP_MOVE(x-2, y-1);
                CHECK_JUMP_MOVE(x-2, y-0);
                CHECK_JUMP_MOVE(x-2, y+1);
                CHECK_JUMP_MOVE(x-2, y+2);

                CHECK_JUMP_MOVE(x+2, y-2);
                CHECK_JUMP_MOVE(x+2, y-1);
                CHECK_JUMP_MOVE(x+2, y-0);
                CHECK_JUMP_MOVE(x+2, y+1);
                CHECK_JUMP_MOVE(x+2, y+2);

                CHECK_JUMP_MOVE(x-1, y-2);
                CHECK_JUMP_MOVE(x+0, y-2);
                CHECK_JUMP_MOVE(x+1, y-2);

                CHECK_JUMP_MOVE(x-1, y+2);
                CHECK_JUMP_MOVE(x+0, y+2);
                CHECK_JUMP_MOVE(x+1, y+2);

                // Now see if player_up() is on any neighboring cell.  If so, there's
                // a clone move to this cell, and we can just pick the first one found,
                // since all 8 possibilities are equivalent.
                #define CHECK_CLONE_MOVE(i, j) \
                    if (cell(i, j) == player_up()) \
                        {++moves_found; *current_move++ = encode_move(i, j, x, y); continue;}

                CHECK_CLONE_MOVE(x-1, y-1);
                CHECK_CLONE_MOVE(x-1, y+0);
                CHECK_CLONE_MOVE(x-1, y+1);

                CHECK_CLONE_MOVE(x-0, y-1);
                CHECK_CLONE_MOVE(x-0, y+1);

                CHECK_CLONE_MOVE(x+1, y-1);
                CHECK_CLONE_MOVE(x+1, y+0);
                CHECK_CLONE_MOVE(x+1, y+1);
            }
        }
    }

    GameMove* possible_moves = new GameMove[moves_found + 1];
    memcpy(possible_moves, move_array, moves_found * sizeof GameMove);
    possible_moves[moves_found] = INVALID_MOVE;  // Terminate move list for caller convenience

    return possible_moves;
}


GameMove AtaxxGameState::read_move(const char* move_string) const
{
    if (toupper(*move_string) == 'P')
    {
        return PASSING_MOVE;
    }
    else
    {
        char source_x = -1; int source_y = -1;
        char target_x = -1; int target_y = -1;
        sscanf_s(move_string, "%c%d%c%d", &source_x, 1, &source_y, &target_x, 1, &target_y);
        return encode_move(toupper(source_x) + 2 - 'A', source_y + 1,
                           toupper(target_x) + 2 - 'A', target_y + 1);
    }
}

void AtaxxGameState::write_move(GameMove move, int move_string_size,
                                __in_ecount(move_string_size) char* move_string) const
{
    if (move == PASSING_MOVE)
    {
        sprintf_s(move_string, move_string_size, "Pass");
    }
    else
    {
        sprintf_s(move_string, move_string_size, "%c%d%c%d",
                  ((move >> 16) & 0xff) + 'A' - 2, ((move >> 24) & 0xff) - 1,
                  ((move >>  0) & 0xff) + 'A' - 2, ((move >>  8) & 0xff) - 1);
    }
}


bool AtaxxGameState::valid_move(GameMove move)
{
    ASSERT(!game_over());

    if (move == PASSING_MOVE)
    {
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

    int source_x, source_y, target_x, target_y;
    decode_move(move, &source_x, &source_y, &target_x, &target_y);

    if (cell(source_x, source_y) != player_up())
    {
        TRACE(INFO, "Invalid move; source cell is not occupied by current player");
        return false;
    }
    if (source_x < target_x-2 || source_x > target_x+2 ||
        source_y < target_y-2 || source_y > target_y+2)
    {
        TRACE(INFO, "Invalid move: target cell is not within 2 steps of source cell");
        return false;
    }
    if (cell(target_x, target_y) != eEmpty)
    {
        TRACE(INFO, "Invalid move: target cell is not empty");
        return false;
    }

    return true;
}


Result AtaxxGameState::apply_move(GameMove move)
{
    ASSERT(valid_move(move));

    int source_x, source_y, target_x, target_y;
    decode_move(move, &source_x, &source_y, &target_x, &target_y);

    if (m_boards[move_counter()+1].m_status == Board::eDirty)
    {
        memcpy(m_boards[move_counter()+1].m_cells,
               m_boards[move_counter()].m_cells,
               sizeof m_boards[move_counter()].m_cells);
        m_boards[move_counter()+1].m_status = Board::eClean;
    }

    m_move_history[move_counter()] = move;
    advance_move_counter();

    int player_up_gain = 0;
    int opponent_loss = 0;

    cell(target_x, target_y) = player_up();
    if (source_x == target_x-2 || source_x == target_x+2 ||
        source_y == target_y-2 || source_y == target_y+2)
    {
        cell(source_x, source_y) = eEmpty;
    }
    else
    {
        ++player_up_gain;
        --m_cells_available;
    }

    // FUTURE: Could unroll this too
    const PlayerCode opponent = (player_up() == eRed) ? eBlue : eRed;
    for (int x = target_x-1; x <= target_x+1; ++x)
    {
        for (int y = target_y-1; y <= target_y+1; ++y)
        {
            if (cell(x, y) == opponent)
            {
                cell(x, y) = player_up();
                ++player_up_gain;
                ++opponent_loss;
            }
        }
    }

    m_boards[move_counter()].m_status = Board::eDirty;
    m_boards[move_counter()+1].m_status = Board::eDirty;

    m_player_cells_history[move_counter()][player_up()] = m_player_cells_history[move_counter()-1][player_up()] + player_up_gain;
    m_player_cells_history[move_counter()][opponent] = m_player_cells_history[move_counter()-1][opponent] - opponent_loss;

    switch_player_up();

    return Result::OK;
}


Result AtaxxGameState::apply_passing_move()
{
    if (m_cells_available == 0)
    {
        return Result::Fail;
    }

    if (m_boards[move_counter()+1].m_status == Board::eDirty)
    {
        memcpy(m_boards[move_counter()+1].m_cells,
               m_boards[move_counter()].m_cells,
               sizeof m_boards[0].m_cells);
        m_boards[move_counter()+1].m_status = Board::eClean;
    }
    m_boards[move_counter()+2].m_status = Board::eDirty;

    m_move_history[move_counter()] = PASSING_MOVE;
    m_player_cells_history[move_counter()+1][eBlue] = m_player_cells_history[move_counter()][eBlue];
    m_player_cells_history[move_counter()+1][eRed] = m_player_cells_history[move_counter()][eRed];
    switch_player_up();
    advance_move_counter();

    return Result::OK;
}


void AtaxxGameState::undo_last_move()
{
    ASSERT(move_counter() > 0);
    retreat_move_counter();

    if (m_move_history[move_counter()] != PASSING_MOVE)
    {
        int source_x, source_y, target_x, target_y;
        decode_move(m_move_history[move_counter()], &source_x, &source_y, &target_x, &target_y);
        if (source_x >= target_x-1 && source_x <= target_x+1 &&
            source_y >= target_y-1 && source_y <= target_y+1)
        {
            ++m_cells_available;
        }
    }

    switch_player_up();
}


// Simplistic position evaluation, but good enough to trounce most humans
Value AtaxxGameState::position_val() const
{
    return m_player_cells_history[move_counter()][eBlue] - m_player_cells_history[move_counter()][eRed];
}


bool AtaxxGameState::game_over()
{
    // Game is over when someone has been wiped out or the board is full
    return m_player_cells_history[move_counter()][eBlue] == 0 ||
           m_player_cells_history[move_counter()][eRed] == 0 ||
           m_cells_available == 0;
}


void AtaxxGameState::display(size_t output_size, __out_ecount(output_size) char* output) const
{
    for (int row = 0; row < ATAXX_ROWS; ++row)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  %c", row ? 'Ç' : 'É');
        for (int col = 0; col < ATAXX_COLUMNS; ++col)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "%s%c", row ? "ÄÄÄ" : "ÍÍÍ", row ? (col == ATAXX_COLUMNS-1 ? '¶' : 'Å')
                                                                : (col == ATAXX_COLUMNS-1 ? '»' : 'Ñ'));
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n%d ", ATAXX_ROWS - row);
        for (int col = 0; col < ATAXX_COLUMNS; ++col)
        {
            CellState state = cell(col + 2, ATAXX_ROWS+1-row);
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "%c%s", col ? '³' : 'º', state == eBlue ? BLUE_SYMBOL :
                                                        state == eRed ? RED_SYMBOL :
                                                        state == eBlocked ? "ÛÛÛ" : "   ");
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "º\n");
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  È");
    for (int col = 0; col < ATAXX_COLUMNS; ++col)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "ÍÍÍ%c", col == ATAXX_COLUMNS-1 ? '¼' : 'Ï');
    }

    if (move_counter() != 0)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " (move %d; ", move_counter());
        int red = m_player_cells_history[move_counter()][eRed];
        int blue = m_player_cells_history[move_counter()][eBlue];
        if (red == blue)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0, "tied at %d cells each)\n ", red);
        }
        else
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s ahead by %d cells)\n ", red > blue ? "red" : "blue", abs(red - blue));
        }
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n ");
    }

    for (int col = 0; col < ATAXX_COLUMNS; ++col)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "   %c", 'A' + col);
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n\n");
}


void AtaxxGameState::display_score_sheet(bool include_moves, size_t output_size, __out_ecount(output_size) char* output) const
{
    int red = m_player_cells_history[move_counter()][eRed];
    int blue = m_player_cells_history[move_counter()][eBlue];

    if (red == blue)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "Tie");
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s won by %d", red > blue ? "Red" : "Blue", abs(red - blue));
    }

    StringCchPrintfExA(output, output_size, &output, &output_size, 0, " (%d cells blue, %d red).\n", blue, red);

    if (include_moves)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "In %d moves:\n", move_counter());
        for (int n = 0; n < move_counter(); ++n)
        {
            char move_string[MAX_MOVE_STRING_SIZE];
            write_move(m_move_history[n], sizeof move_string, move_string);
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "\t%d. %s %s\n", n + 1, (n % 2) ? "Red" : "Blue", move_string);
        }

        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\nFinal board state:\n");
        display(output_size, output);
    }
}


const char* AtaxxGameState::get_cell_state_image_name(int state) const
{
    ASSERT(state == eBlue || state == eRed || state == eEmpty || state == eBlocked);

    return state == eBlue  ? "AtaxxBlue"  :
           state == eRed   ? "AtaxxRed"   :
           state == eEmpty ? "AtaxxEmpty" : "AtaxxBlocked";
}
