// othello.h

#pragma once
#include "game.h"  // Base class

// Game configuration
#ifndef OTH_DIMENSION
    #define OTH_DIMENSION 8  // Board size
#endif

// CellState values specific to Othello
#define eBlack CellState(0)
#define eWhite CellState(1)
#define eEmpty CellState(2)

// NOTE: eBlack and eWhite must be 0 and 1 as they are used as array indices


class OthelloPlayer : public Player
{
public:

    // Factory function
    static Player* creator(bool, int);

    // Player method overrides
    const char* get_side_name() const {return m_id == eBlack ? "Black" : "White";}

private:

    OthelloPlayer(bool human, int id =0, const char* name =0, bool attributes =0)
      : Player(human, id, name, attributes) {}
};


class OthelloGameState : public GameState
{
public:

    // Factory function and destructor

    static GameState* creator();
    ~OthelloGameState() {delete[] m_initial_position;}

    // GameState method overrides

    GameAttributes game_attributes() const {return eGreedy;}  // FIXME: This doesn't do much
    RESULT set_initial_position(size_t n, __in_bcount(n) const char*);
    void reset();
    GameMove* get_possible_moves() const;
    RESULT apply_move(GameMove);
    RESULT apply_passing_move();
    void undo_last_move();
    bool game_over();
    void display(size_t size, __out_ecount(size) char*) const;
    void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    int get_rows() const {return OTH_DIMENSION;}
    int get_columns() const {return OTH_DIMENSION;}
    int get_cell_states_count() const {return 3;}  // Black, white and empty
    const char* get_cell_state_image_name(int state) const;
    int get_cell_state(int row, int column) const {return cell(row+1, column+1);}

    // Move management

    GameMove read_move(const char*) const;
    void write_move(GameMove, int size, __in_ecount(size) char*) const;
    bool valid_move(GameMove);

    // Position value management

    RESULT set_value_function(int);
    Value position_val() const;
    Value game_over_val() const
    {
        int black_advantage = m_player_cells_history[m_move_number][eBlack] - m_player_cells_history[m_move_number][eWhite];
        return black_advantage + (black_advantage > 0 ? VICTORY_VALUE : black_advantage < 0 ? -VICTORY_VALUE : 0);
    }
    const Player* player_ahead() const;

private:

    // Position evaluation heuristic selection - currently unused
    enum Evaluator {eSmart, eStupid, eEvaluatorCount} m_position_evaluator;

    #define OTH_MAX_GAME_LENGTH (2 * OTH_DIMENSION*OTH_DIMENSION)  // Allows for an impossible number of passing moves

    #if OTH_GAME_STATE_LIST

        struct Board
        {
            enum {eClean, eDirty} m_status;
            CellState m_cells[OTH_DIMENSION + 2][OTH_DIMENSION + 2];  // +2 for edge sentinels
        };
        Board m_boards[OTH_MAX_GAME_LENGTH];

    #else

        CellState m_board[OTH_DIMENSION + 2][OTH_DIMENSION + 2];  // +2 for edge sentinels
        struct UndoRecord
        {
            Cell added_piece;  // The new cell that has been occupied in this move
            Cell flipped_pieces[20];  // Up to 19 pieces can flip in one move, plus a sentinel = 20
        }
        m_undo_history[OTH_MAX_GAME_LENGTH];

    #endif

    char* m_initial_position;
    int m_cells_available;

    // Game history data

    Cell m_move_history[OTH_MAX_GAME_LENGTH];
    mutable Value m_value_history[OTH_MAX_GAME_LENGTH];
    int m_player_cells_history[OTH_MAX_GAME_LENGTH][2];

    // Internal methods

    OthelloGameState() : m_initial_position(NULL) {reset();}

    #if OTH_GAME_STATE_LIST
        FORCEINLINE CellState& cell(int x, int y) {return m_boards[m_move_number].m_cells[x][y];}
        FORCEINLINE CellState cell(int x, int y) const {return m_boards[m_move_number].m_cells[x][y];}
    #else
        FORCEINLINE CellState& cell(int x, int y) {return m_board[x][y];}
        FORCEINLINE CellState cell(int x, int y) const {return m_board[x][y];}
    #endif
};
