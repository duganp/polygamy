// connect4.h

#pragma once
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


class Connect4Player : public Player
{
public:

    // Factory function
    static Player* creator(bool, int);

    // Player method overrides
    const char* get_side_name() const {return m_id == eBlue ? "Blue" : "Red";}

private:

    Connect4Player(bool human, int id =0, const char* name =0, bool attributes =0)
      : Player(human, id, name, attributes) {}
};


class Connect4GameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    void reset();
    GameMove* get_possible_moves() const;
    RESULT apply_move(GameMove);
    void undo_last_move();
    bool game_over();
    void display(size_t size, __out_ecount(size) char*) const;
    void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    int get_rows() const {return CONNECT4_ROWS;}
    int get_columns() const {return CONNECT4_COLUMNS;}
    int get_cell_states_count() const {return 3;}  // Blue, red and empty
    const char* get_cell_state_image_name(int state) const;
    int get_cell_state(int row, int column) const {return m_board[column][CONNECT4_ROWS-1-row];}

    // Move management
    GameMove read_move(const char*) const;
    void write_move(GameMove, int size, __in_ecount(size) char*) const;
    bool valid_move(GameMove);

    // Position value management
    Value position_val() const;
    const Player* player_ahead() const {return m_winner;}

private:

    const Player* m_winner;  // FIXME: Could get rid of this (and then victory_val() and defeat_val()?)
    CellState m_board[CONNECT4_COLUMNS][CONNECT4_ROWS];
    Cell m_move_history[CONNECT4_COLUMNS * CONNECT4_ROWS];

    Connect4GameState() {reset();}
};
