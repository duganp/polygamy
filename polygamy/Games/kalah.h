// kalah.h

#pragma once
#include "game.h"  // Base class

// Game configuration
#ifndef KALAH_PITS
    #define KALAH_PITS 6  // Numbers of pits (houses) on each side side
#endif
#ifndef KALAH_SEEDS
    #define KALAH_SEEDS 4  // Number of seeds in each pit at start of game
#endif

// Hardcoded constant (cannot change without corresponding code changes):
// number of image files used to represent Kalah pit states (see
#define KALAH_CELL_TYPE_IMAGES 23


// FIXME: note how completely pointless the Player abstraction is in this case. SIMPLIFY OR KILL IT

class KalahPlayer : public Player
{
public:

    // Factory function
    static Player* creator(bool, int);

private:

    KalahPlayer(bool human, int id =0, const char* name =0, bool attributes =0)
      : Player(human, id, name, attributes) {}
};


class KalahGameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    void reset();
    GameMove* get_possible_moves() const;
    RESULT apply_move(GameMove);
    RESULT apply_passing_move();
    void undo_last_move();
    bool game_over() {return m_states[m_move_number][KALAH_PITS] + m_states[m_move_number][2 * KALAH_PITS + 1] == 2 * KALAH_PITS * KALAH_SEEDS;}  // FIXME: could be easier on the eyes...
    // FIXME: Can this be collapsed altogether into get_possible_moves()?  Or just wrap it?  Don't have 2 subtly different ways to do the same thing...
    void display(size_t size, __out_ecount(size) char*) const;
    void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    int get_rows() const {return 3;}
    int get_columns() const {return KALAH_PITS + 2;}  // For the two stores
    int get_cell_states_count() const {return KALAH_CELL_TYPE_IMAGES;}
    const char* get_cell_state_image_name(int state) const;
    int get_cell_state(int row, int column) const;

    // Move management
    GameMove read_move(const char*) const;
    void write_move(GameMove, int size, __in_ecount(size) char*) const;
    bool valid_move(GameMove);

    // Position value management
    Value position_val() const
    {
        int player0_store = m_states[m_move_number][KALAH_PITS];
        int player1_store = m_states[m_move_number][2 * KALAH_PITS + 1];
        return player0_store - player1_store;
    }
    const Player* player_ahead() const {return position_val() > 0 ? m_players[0] : position_val() < 0 ? m_players[1] : NULL;}

private:

    // Data
    #define KALAH_MAX_GAME_LENGTH 100*   (2 * KALAH_PITS * KALAH_SEEDS)  // FIXME: check
    int m_states[KALAH_MAX_GAME_LENGTH][2 * KALAH_PITS + 2];
    GameMove m_move_history[KALAH_MAX_GAME_LENGTH];
    bool m_forced_pass;

    // Internal methods
    KalahGameState() {reset();}
};
