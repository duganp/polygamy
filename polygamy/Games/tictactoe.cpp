// tictactoe.cpp

#include "shared.h"     // Precompiled header; obligatory
#include "tictactoe.h"  // Our public interface


// Game registration stuff

GameState* TicTacToeGameState::creator() {return new TicTacToeGameState;}
Player* TicTacToePlayer::creator(bool human, int player_num) {return new TicTacToePlayer(human, player_num);}

static int tictactoe_registered =
    register_game
    (
        "Tic-tac-toe",
        TicTacToeGameState::creator,
        TicTacToePlayer::creator
    );


void TicTacToeGameState::reset()
{
    GameState::reset();

    memset(m_cells, eEmpty, sizeof m_cells);
    memset(m_value_history, 0, sizeof m_value_history);
}


GameMove* TicTacToeGameState::get_possible_moves() const
{
    GameMove* possible_moves = new GameMove[TTT_DIMENSION * TTT_DIMENSION + 1];
    GameMove* current_move = possible_moves;

    // Only return any moves if the game isn't over
    if (position_val() == 0)
    {
        int cells_left_to_inspect = TTT_DIMENSION * TTT_DIMENSION;
        while (cells_left_to_inspect--)
        {
            int x = cells_left_to_inspect / TTT_DIMENSION;
            int y = cells_left_to_inspect % TTT_DIMENSION;
            if (m_cells[m_move_number][x][y] == eEmpty)
            {
                *current_move++ = Cell(x+1, y+1);
            }
        }
    }

    *current_move = INVALID_MOVE;  // Terminate move list for caller convenience

    return possible_moves;
}


GameMove TicTacToeGameState::read_move(const char* move_string) const
{
    int row = -1;
    char col = -1;
    sscanf_s(move_string, "%c%d", &col, 1, &row);
    return GameMove(Cell(TTT_DIMENSION - row + 1, toupper(col) - 'A' + 1));
}


void TicTacToeGameState::write_move(GameMove move, int move_string_size,
                                    __in_ecount(move_string_size) char* move_string) const
{
    sprintf_s(move_string, move_string_size,
              "%c%d", 'A' + Cell(move).y - 1,
              TTT_DIMENSION - Cell(move).x + 1);
}


bool TicTacToeGameState::valid_move(GameMove move)
{
    short x = Cell(move).x - 1;
    short y = Cell(move).y - 1;

    return x >= 0 && x < TTT_DIMENSION &&
           y >= 0 && y < TTT_DIMENSION &&
           m_cells[m_move_number][x][y] == eEmpty;
}


RESULT TicTacToeGameState::apply_move(GameMove move)
{
    ASSERT(valid_move(move));

    short x = Cell(move).x - 1;
    short y = Cell(move).y - 1;

    ++m_move_number;
    CellState (&cells)[TTT_DIMENSION][TTT_DIMENSION] = m_cells[m_move_number];
    memcpy(cells, m_cells[m_move_number-1], sizeof cells);
    cells[x][y] = m_player_up;

    // Check for victory
    bool row = true, col = true;
    Value new_value = 0;

    for (int i = 1; i < TTT_DIMENSION; ++i)
    {
        row &= (cells[x][(y+i) % TTT_DIMENSION] == m_player_up);
        col &= (cells[(x+i) % TTT_DIMENSION][y] == m_player_up);
    }
    if (row || col)
    {
        new_value = victory_val();
    }
    else
    {
        bool diag1 = false;
        if (x == y)
        {
            diag1 = true;
            for (int i = 1; i < TTT_DIMENSION; ++i)
            {
                diag1 &= (cells[(x+i) % TTT_DIMENSION][(y+i) % TTT_DIMENSION] == m_player_up);
            }
        }
        if (diag1)
        {
            new_value = victory_val();
        }
        else if (x+y == TTT_DIMENSION-1)
        {
            bool diag2 = true;
            for (int i = 1; i < TTT_DIMENSION; ++i)
            {
                diag2 &= (cells[(x+i) % TTT_DIMENSION][(y+TTT_DIMENSION-i) % TTT_DIMENSION] == m_player_up);
            }
            if (diag2)
            {
                new_value = victory_val();
            }
        }
    }

    m_value_history[m_move_number] = new_value;

    m_player_up = (m_player_up == eCross) ? eNought : eCross;

    return OK;
}


void TicTacToeGameState::undo_last_move()
{
    ASSERT(m_move_number > 0);
    --m_move_number;
    m_player_up = (m_player_up == eCross) ? eNought : eCross;
}


void TicTacToeGameState::display(size_t output_size, __out_ecount(output_size) char* output) const
{
    for (int i = 0; i < TTT_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  %c", i ? 'Ç' : 'É');
        for (int j = 0; j < TTT_DIMENSION; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "%s%c", i ? "ÄÄÄ" : "ÍÍÍ", i ? (j == TTT_DIMENSION-1 ? '¶' : 'Å')
                                                            : (j == TTT_DIMENSION-1 ? '»' : 'Ñ'));
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n%d", TTT_DIMENSION - i);
        for (int j = 0; j < TTT_DIMENSION; ++j)
        {
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               " %c %c", j ? '³' : 'º',
                               m_cells[m_move_number][i][j] == eCross ? 'X' :
                               m_cells[m_move_number][i][j] == eNought ? 'O' : ' ');
        }
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " º\n");
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "  È");
    for (int i = 0; i < TTT_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "ÍÍÍ%c", i == TTT_DIMENSION-1 ? '¼' : 'Ï');
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n ");
    for (int i = 0; i < TTT_DIMENSION; ++i)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "   %c", 'A' + i);
    }
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\n\n");
}


void TicTacToeGameState::display_score_sheet(bool, size_t output_size, __out_ecount(output_size) char* output) const
{
    StringCchPrintfExA(output, output_size, &output, &output_size, 0, "%s. Final board state:\n", position_val() > 0 ? "Crosses won" : position_val() < 0 ? "Noughts won" : "Tie");
    display(output_size, output);
}


const char* TicTacToeGameState::get_cell_state_image_name(int state) const
{
    ASSERT(state == eCross || state == eNought || state == eEmpty);

    return state == eCross  ? "TicTacToeCross"  :
           state == eNought ? "TicTacToeNought" : "TicTacToeEmpty";
}
