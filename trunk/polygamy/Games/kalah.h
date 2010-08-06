// kalah.h

#ifndef GAMES_KALAH_H
#define GAMES_KALAH_H

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


class KalahGameState : public GameState
{
public:

    // Factory function
    static GameState* creator();

    // GameState method overrides
    virtual void reset();
    virtual GameMove* get_possible_moves() const;
    virtual Result apply_move(GameMove);
    virtual Result apply_passing_move();
    virtual void undo_last_move();
    virtual bool game_over() {return m_states[move_counter()][KALAH_PITS] + m_states[move_counter()][2 * KALAH_PITS + 1] ==
                                     2 * KALAH_PITS * KALAH_SEEDS;}  // This could be easier on the eyes...
    virtual void display(size_t size, __out_ecount(size) char*) const;
    virtual void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    virtual int get_rows() const {return 3;}
    virtual int get_columns() const {return KALAH_PITS + 2;}  // For the two stores
    virtual int get_cell_states_count() const {return KALAH_CELL_TYPE_IMAGES;}
    virtual const char* get_cell_state_image_name(int state) const;
    virtual int get_cell_state(int row, int column) const;

    // Move management
    virtual GameMove read_move(const char*) const;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const;
    virtual bool valid_move(GameMove);

    // Position value management
    virtual Value position_val() const
    {
        int player0_store = m_states[move_counter()][KALAH_PITS];
        int player1_store = m_states[move_counter()][2 * KALAH_PITS + 1];
        return player0_store - player1_store;
    }

private:

    // Data
    #define KALAH_MAX_GAME_LENGTH (5 * KALAH_PITS * KALAH_SEEDS)  // More than enough
    int m_states[KALAH_MAX_GAME_LENGTH][2 * KALAH_PITS + 2];
    GameMove m_move_history[KALAH_MAX_GAME_LENGTH];
    bool m_forced_pass;

    // Internal methods
    KalahGameState() {reset();}
};

#endif // GAMES_KALAH_H
