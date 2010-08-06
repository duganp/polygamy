// connect4.h

#ifndef GAMES_CONNECT4_H
#define GAMES_CONNECT4_H

#include "game.h"  // Base class

// Game configuration
#ifndef CONNECT4_COLUMNS
    #define CONNECT4_COLUMNS 7
#endif
#ifndef CONNECT4_ROWS
    #define CONNECT4_ROWS 6
#endif

// CellState values specific to Connect 4
#define eBlue  CellState(0)
#define eRed   CellState(1)
#define eEmpty CellState(2)


class Connect4GameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    virtual const char* get_player_name(PlayerCode p) const {return p == eBlue ? "Blue" : "Red";}
    virtual void reset();
    virtual GameMove* get_possible_moves() const;
    virtual Result apply_move(GameMove);
    virtual void undo_last_move();
    virtual bool game_over();
    virtual void display(size_t size, __out_ecount(size) char*) const;
    virtual void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    virtual int get_rows() const {return CONNECT4_ROWS;}
    virtual int get_columns() const {return CONNECT4_COLUMNS;}
    virtual int get_cell_states_count() const {return 3;}  // Blue, red and empty
    virtual const char* get_cell_state_image_name(int state) const;
    virtual int get_cell_state(int row, int column) const {return m_board[column][CONNECT4_ROWS-1-row];}

    // Move management
    virtual GameMove read_move(const char*) const;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const;
    virtual bool valid_move(GameMove);

    // Position value management
    virtual Value position_val() const;
    virtual PlayerCode player_ahead() const {return m_winner;}

private:

    PlayerCode m_winner;
    CellState m_board[CONNECT4_COLUMNS][CONNECT4_ROWS];
    Cell m_move_history[CONNECT4_COLUMNS * CONNECT4_ROWS];

    Connect4GameState() {reset();}
};

#endif // GAMES_CONNECT4_H
