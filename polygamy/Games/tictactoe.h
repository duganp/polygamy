// tictactoe.h

#pragma once
#include "game.h"  // Base class

// Game configuration
#ifndef TTT_DIMENSION
    #define TTT_DIMENSION 3  // Board size
#endif

// CellState values specific to Tic-tac-toe
#define eCross  CellState(0)
#define eNought CellState(1)
#define eEmpty  CellState(2)


class TicTacToePlayer : public Player
{
public:

    // Factory function
    static Player* creator(bool, int);

    // Player method overrides
    const char* get_side_name() const {return m_id == eCross ? "Crosses" : "Noughts";}

private:

    TicTacToePlayer(bool human, int id =0, const char* name =0, bool attributes =0)
      : Player(human, id, name, attributes) {}
};


class TicTacToeGameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    void reset();
    GameMove* get_possible_moves() const;
    RESULT apply_move(GameMove);
    void undo_last_move();
    bool game_over() {return position_val() != 0 || m_move_number >= TTT_DIMENSION*TTT_DIMENSION;}
    void display(size_t size, __out_ecount(size) char*) const;
    void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    int get_rows() const {return TTT_DIMENSION;}
    int get_columns() const {return TTT_DIMENSION;}
    int get_cell_states_count() const {return 3;}  // Nought, cross and empty
    const char* get_cell_state_image_name(int state) const;
    int get_cell_state(int row, int column) const {return m_cells[m_move_number][row][column];}

    // Move management
    GameMove read_move(const char*) const;
    void write_move(GameMove, int size, __in_ecount(size) char*) const;
    bool valid_move(GameMove);

    // Position value management
    Value position_val() const {return m_value_history[m_move_number];}
    const Player* player_ahead() const {return position_val() > 0 ? m_players[eCross] : position_val() < 0 ? m_players[eNought] : NULL;}

private:

    // Data
    #define TTT_MAX_GAME_LENGTH (TTT_DIMENSION*TTT_DIMENSION + 1)
    CellState m_cells[TTT_MAX_GAME_LENGTH][TTT_DIMENSION][TTT_DIMENSION];
    Value m_value_history[TTT_MAX_GAME_LENGTH];

    // Internal methods
    TicTacToeGameState() {reset();}
};
