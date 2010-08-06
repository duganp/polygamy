// othello.h

#ifndef GAMES_OTHELLO_H
#define GAMES_OTHELLO_H

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


class OthelloGameState : public GameState
{
public:

    // Factory function and destructor

    static GameState* creator();
    virtual ~OthelloGameState() {delete[] m_initial_position;}

    // GameState method overrides

    virtual GameAttributes game_attributes() const {return eGreedy;}
    virtual const char* get_player_name(PlayerCode p) const {return p == eBlack ? "Black" : "White";}
    virtual Result set_initial_position(size_t n, __in_bcount(n) const char*);
    virtual void reset();
    virtual GameMove* get_possible_moves() const;
    virtual Result apply_move(GameMove);
    virtual Result apply_passing_move();
    virtual void undo_last_move();
    virtual bool game_over();
    virtual void display(size_t size, __out_ecount(size) char*) const;
    virtual void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const;

    // For use by the GUI frontend only
    virtual int get_rows() const {return OTH_DIMENSION;}
    virtual int get_columns() const {return OTH_DIMENSION;}
    virtual int get_cell_states_count() const {return 3;}  // Black, white and empty
    virtual const char* get_cell_state_image_name(int state) const;
    virtual int get_cell_state(int row, int column) const {return cell(row+1, column+1);}

    // Move management

    virtual GameMove read_move(const char*) const;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const;
    virtual bool valid_move(GameMove);

    // Position value management

    virtual Result set_value_function(int);
    virtual Value position_val() const;
    virtual Value game_over_val() const
    {
        int black_advantage = m_player_cells_history[move_counter()][eBlack] - m_player_cells_history[move_counter()][eWhite];
        return black_advantage + (black_advantage > 0 ? VICTORY_VALUE : black_advantage < 0 ? -VICTORY_VALUE : 0);
    }
    virtual PlayerCode player_ahead() const;

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
        FORCEINLINE CellState& cell(int x, int y) {return m_boards[move_counter()].m_cells[x][y];}
        FORCEINLINE CellState cell(int x, int y) const {return m_boards[move_counter()].m_cells[x][y];}
    #else
        FORCEINLINE CellState& cell(int x, int y) {return m_board[x][y];}
        FORCEINLINE CellState cell(int x, int y) const {return m_board[x][y];}
    #endif
};

#endif // GAMES_OTHELLO_H
