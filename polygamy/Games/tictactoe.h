// tictactoe.h

#ifndef GAMES_TICTACTOE_H
#define GAMES_TICTACTOE_H

#include "game.h"  // Base class

// Game configuration
#ifndef TTT_DIMENSION
    #define TTT_DIMENSION 3  // Board size
#endif

// CellState values specific to Tic-tac-toe
#define eCross  CellState(0)
#define eNought CellState(1)
#define eEmpty  CellState(2)


class TicTacToeGameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    virtual const char* get_player_name(PlayerCode p) const {return p == eCross ? "Crosses" : "Noughts";}
    virtual void reset();
    virtual GameMove* get_possible_moves() const;
    virtual Result apply_move(GameMove);
    virtual void undo_last_move();
    virtual bool game_over() {return position_val() != 0 || move_counter() >= TTT_DIMENSION*TTT_DIMENSION;}
    virtual void display(size_t size, __out_ecount(size) char*) const;
    virtual void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    virtual int get_rows() const {return TTT_DIMENSION;}
    virtual int get_columns() const {return TTT_DIMENSION;}
    virtual int get_cell_states_count() const {return 3;}  // Nought, cross and empty
    virtual const char* get_cell_state_image_name(int state) const;
    virtual int get_cell_state(int row, int column) const {return m_cells[move_counter()][row][column];}

    // Move management
    virtual GameMove read_move(const char*) const;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const;
    virtual bool valid_move(GameMove);

    // Position value management
    virtual Value position_val() const {return m_value_history[move_counter()];}

private:

    // Data
    #define TTT_MAX_GAME_LENGTH (TTT_DIMENSION*TTT_DIMENSION + 1)
    CellState m_cells[TTT_MAX_GAME_LENGTH][TTT_DIMENSION][TTT_DIMENSION];
    Value m_value_history[TTT_MAX_GAME_LENGTH];

    // Internal methods
    TicTacToeGameState() {reset();}
};

#endif // GAMES_TICTACTOE_H