// ataxx.h

#ifndef GAMES_ATAXX_H
#define GAMES_ATAXX_H

#include "game.h"  // Base class

// Game configuration
#ifndef ATAXX_COLUMNS
    #define ATAXX_COLUMNS 7
#endif
#ifndef ATAXX_ROWS
    #define ATAXX_ROWS 7
#endif

// CellState values specific to Ataxx
#define eBlue    CellState(0)
#define eRed     CellState(1)
#define eEmpty   CellState(2)
#define eBlocked CellState(3)


class AtaxxGameState : public GameState
{
public:

    // Factory function and destructor

    static GameState* creator();
    ~AtaxxGameState() {delete[] m_initial_position;}

    // GameState method overrides

    virtual const char* get_player_name(PlayerCode p) const {return p == eBlue ? "Blue" : "Red";}
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
    virtual int get_rows() const {return ATAXX_ROWS;}
    virtual int get_columns() const {return ATAXX_COLUMNS;}
    virtual int get_cell_states_count() const {return 4;}  // Blue, red, empty and blocked
    virtual const char* get_cell_state_image_name(int state) const;
    virtual int get_cell_state(int row, int col) const {return cell(col+2, ATAXX_ROWS+1-row);}

    // Move management

    virtual GameMove read_move(const char*) const;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const;
    virtual bool valid_move(GameMove);
    virtual bool equivalent_moves(GameMove m1, GameMove m2) const
    {
        int source_x_1, source_y_1, target_x_1, target_y_1;
        int source_x_2, source_y_2, target_x_2, target_y_2;
        decode_move(m1, &source_x_1, &source_y_1, &target_x_1, &target_y_1);
        decode_move(m2, &source_x_2, &source_y_2, &target_x_2, &target_y_2);
        bool clone_move_1 = source_x_1 >= target_x_1 - 1 && source_x_1 <= target_x_1 + 1 &&
                            source_y_1 >= target_y_1 - 1 && source_y_1 <= target_y_1 + 1;
        bool clone_move_2 = source_x_2 >= target_x_2 - 1 && source_x_2 <= target_x_2 + 1 &&
                            source_y_2 >= target_y_2 - 1 && source_y_2 <= target_y_2 + 1;
        return (target_x_1 == target_x_2 && target_y_1 == target_y_2) &&
               ((clone_move_1 && clone_move_2) ||
                (source_x_1 == source_x_2 && source_y_1 == source_y_2));
    }
    FORCEINLINE GameMove encode_move(int source_x, int source_y, int target_x, int target_y) const
    {
        return GameMove(target_x | (target_y << 8) | (source_x << 16) | (source_y << 24));
    }
    FORCEINLINE void decode_move(GameMove move, int* source_x, int* source_y, int* target_x, int* target_y) const
    {
        *target_x = move & 0xff; *target_y = (move >> 8) & 0xff;
        *source_x = (move >> 16) & 0xff; *source_y = (move >> 24);
    }

    // Position value management

    virtual Value position_val() const;
    virtual Value game_over_val() const
    {
        int blue_advantage = m_player_cells_history[move_counter()][eBlue] - m_player_cells_history[move_counter()][eRed];
        return blue_advantage + (blue_advantage > 0 ? VICTORY_VALUE : blue_advantage < 0 ? -VICTORY_VALUE : 0);
    }

private:

    #define ATAXX_MAX_GAME_LENGTH 999  // FIXME: There really isn't a maximum for Ataxx

    char* m_initial_position;
    int m_cells_available;
    GameMove m_move_history[ATAXX_MAX_GAME_LENGTH];
    int m_player_cells_history[ATAXX_MAX_GAME_LENGTH][2];

    struct Board
    {
        enum {eClean, eDirty} m_status;
        CellState m_cells[ATAXX_COLUMNS+4][ATAXX_ROWS+4];  // Double layer of sentinels
    };
    Board m_boards[ATAXX_MAX_GAME_LENGTH];

    FORCEINLINE CellState& cell(int x, int y) {return m_boards[move_counter()].m_cells[x][y];}
    FORCEINLINE CellState cell(int x, int y) const {return m_boards[move_counter()].m_cells[x][y];}

    AtaxxGameState() : m_initial_position(NULL) {reset();}
};

#endif // GAMES_ATAXX_H