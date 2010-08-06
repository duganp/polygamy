// connect4.cpp

#include "shared.h"         // Precompiled header; obligatory
#include "connect4.h"       // Our public interface

#define RED_SYMBOL '\xf9'   // Looks right in console window
#define BLUE_SYMBOL '\x4f'  // Looks right in console window


// Game registration stuff

GameState* Connect4GameState::creator() {return new Connect4GameState;}

static int connect4_registered =
    register_game
    (
        "Connect 4",
        Connect4GameState::creator
    );


void Connect4GameState::reset()
{
    GameState::reset();

    m_winner = -1;
    memset(m_move_history, 0, sizeof m_move_history);
    memset(m_board, eEmpty, sizeof m_board);
}


GameMove* Connect4GameState::get_possible_moves() const
{
    GameMove* possible_moves = new GameMove[CONNECT4_COLUMNS + 1];
    GameMove* current_move = possible_moves;

    // There are no legal moves if someone has already won
    if (m_winner == -1)
    {
        #if RANDOMIZE
            int start_col = rand() % CONNECT4_COLUMNS;
        #else
            int start_col = CONNECT4_COLUMNS / 2;
        #endif

        for (int col = start_col; col < CONNECT4_COLUMNS; ++col)
            if (m_board[col][CONNECT4_ROWS-1] == eEmpty)
                *current_move++ = GameMove(col+1);

        for (int col = 0; col < start_col; ++col)
            if (m_board[col][CONNECT4_ROWS-1] == eEmpty)
                *current_move++ = GameMove(col+1);
    }

    *current_move = INVALID_MOVE;  // Terminate move list for caller convenience

    return possible_moves;
}


GameMove Connect4GameState::read_move(const char* move_string) const
{
    while (int c = toupper(*move_string++))
    {
        if (c >= 'A' && c < 'A' + CONNECT4_COLUMNS) return GameMove(c - 'A' + 1);
        if (c > '0' && c <= '0' + CONNECT4_COLUMNS) return GameMove(c - '0');
    }
    return INVALID_MOVE;
}

void Connect4GameState::write_move(GameMove move, int move_string_size,
                                   __in_ecount(move_string_size) char* move_string) const
{
    sprintf_s(move_string, move_string_size, "#%d", Cell(move).x);
}


bool Connect4GameState::valid_move(GameMove move)
{
    return move > 0 && move <= CONNECT4_COLUMNS &&
           m_board[move-1][CONNECT4_ROWS-1] == eEmpty;
}


Result Connect4GameState::apply_move(GameMove move)
{
    ASSERT(valid_move(move));
    ASSERT(m_winner == -1);
    ASSERT(move_counter() < CONNECT4_COLUMNS * CONNECT4_ROWS);

    // Locate the first free cell in the requested move column
    const int x = move - 1;
    int y = 0;
    while (m_board[x][y] != eEmpty) ++y;
    ASSERT(y < CONNECT4_ROWS);

    m_board[x][y] = player_up();

    // Check for victory condition (4 in a row vertically, horizontally or diagonally)
    bool victorious = false;

    // Vertical test
    if (y >= 3 && m_board[x][y-1] == player_up() && m_board[x][y-2] == player_up() && m_board[x][y-3] == player_up())
    {
        victorious = true;
    }

    // Horizontal test
    if (!victorious)
    {
        int left_bound = (x > 3) ? (x - 3) : (0);
        int right_bound = (x < CONNECT4_COLUMNS-3) ? (x + 3) : (CONNECT4_COLUMNS-1);

        for (int horiz_count = 0, n = left_bound; n <= right_bound; ++n)
        {
            if (m_board[n][y] == player_up())
            {
                if (++horiz_count == 4) {victorious = true; break;}
            }
            else
            {
                horiz_count = 0;
            }
        }
    }

    // First diagonal test
    if (!victorious)
    {
        int room_on_left = 3, room_on_right = 3;
        if (room_on_left > x) room_on_left = x;
        if (room_on_left > y) room_on_left = y;
        if (room_on_right > CONNECT4_COLUMNS - x - 1) room_on_right = CONNECT4_COLUMNS - x - 1;
        if (room_on_right > CONNECT4_ROWS - y - 1) room_on_right = CONNECT4_ROWS - y - 1;

        for (int diag1_count = 0, n = -room_on_left; n <= room_on_right; ++n)
        {
            if (m_board[x+n][y+n] == player_up())
            {
                if (++diag1_count == 4) {victorious = true; break;}
            }
            else
            {
                diag1_count = 0;
            }
        }
    }

    // Second diagonal test
    if (!victorious)
    {
        int room_on_left = 3, room_on_right = 3;
        if (room_on_left > x) room_on_left = x;
        if (room_on_left > CONNECT4_ROWS - y - 1) room_on_left = CONNECT4_ROWS - y - 1;
        if (room_on_right > CONNECT4_COLUMNS - x - 1) room_on_right = CONNECT4_COLUMNS - x - 1;
        if (room_on_right > y) room_on_right = y;

        for (int diag2_count = 0, n = -room_on_left; n <= room_on_right; ++n)
        {
            if (m_board[x+n][y-n] == player_up())
            {
                if (++diag2_count == 4) {victorious = true; break;}
            }
            else
            {
                diag2_count = 0;
            }
        }
    }

    if (victorious)
    {
        m_winner = player_up();
    }

    m_move_history[move_counter()].x = short(x);
    m_move_history[move_counter()].y = short(y);
    advance_move_counter();
    switch_player_up();

    return Result::OK;
}


void Connect4GameState::undo_last_move()
{
    ASSERT(move_counter() > 0);
    retreat_move_counter();

    m_board[m_move_history[move_counter()].x][m_move_history[move_counter()].y] = eEmpty;
    m_winner = -1;
    switch_player_up();
}


Value Connect4GameState::position_val() const
{
    return m_winner == -1 ? 0 :
           m_winner == player_up() ? victory_val() :
           defeat_val();
}


bool Connect4GameState::game_over()
{
    // Game is over if we have a winner or the board is full
    return (m_winner != -1) || (move_counter() == CONNECT4_COLUMNS * CONNECT4_ROWS);
}


void Connect4GameState::display(size_t output_size, __out_ecount(output_size) char* output) const
{
    for (int i = 0; i < CONNECT4_ROWS; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " %c", i ? 'Ç' : 'É');
        for (int j = 0; j < CONNECT4_COLUMNS; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "%s%c", i ? "ÄÄÄ" : "ÍÍÍ", i ? (j == CONNECT4_COLUMNS-1 ? '¶' : 'Å')
                                                            : (j == CONNECT4_COLUMNS-1 ? '»' : 'Ñ'));
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n");
        for (int j = 0; j < CONNECT4_COLUMNS; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               " %c %c", j ? '³' : 'º',
                               m_board[j][CONNECT4_ROWS-1-i] == eBlue ? BLUE_SYMBOL :
                               m_board[j][CONNECT4_ROWS-1-i] == eRed ? RED_SYMBOL : ' ');
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " º\n");
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, " È");
    for (int i = 0; i < CONNECT4_COLUMNS; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                           "ÍÍÍ%c", i == CONNECT4_COLUMNS-1 ? '¼' : 'Ï');
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, " (move %u)\n", move_counter());
    for (int i = 0; i < CONNECT4_COLUMNS; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "   %c", '1' + i);
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n\n");
}


void Connect4GameState::display_score_sheet(bool include_moves, size_t output_size, __out_ecount(output_size) char* output) const
{
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s won", get_player_name(m_winner));

    if (include_moves)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " in %d moves:\n", move_counter());
        for (int n = 0; n < move_counter(); ++n)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\t%d. %s #%d\n", n + 1, (n % 2) ? "Red" : "Blue", m_move_history[n].x + 1);
        }

        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\nFinal board state:\n");
        display(output_size, output);
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, ".\n");
    }
}


const char* Connect4GameState::get_cell_state_image_name(int state) const
{
    ASSERT(state == eBlue || state == eRed || state == eEmpty);

    return state == eBlue ? "Connect4Blue" :
           state == eRed  ? "Connect4Red"  : "Connect4Empty";
}
