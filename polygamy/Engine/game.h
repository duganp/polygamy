// game.h

#pragma once

// FIXME: Stuff needed to get this fucker to compile as managed C++
#include <windows.h>  // For CRITICAL_SECTION
#include <sal.h>
//#include <specstrings.h>  FIXME - which?
typedef long RESULT;
#define OK RESULT(0)
#define FAIL RESULT(0x88888888)
#define NULL 0


#define MAXIMIZE_VICTORY 1  // FIXME remove


// A type used to represent position values
typedef int Value;  // Note: this limits us to two-player games

#define LIMIT_VALUE 200000     // Upper bound on legal position values; must be
                               // greater than the greatest value that can ever
                               // be returned by position_val().

#define VICTORY_VALUE 100000   // Victory threshold; values >= this represent
                               // a victory condition for one of the players.
                               // Must be greater than the greatest non-winning
                               // value ever returned by position_val().

#define INVALID_VALUE (int(-1) >> 1)  // Maximum positive int; used for uninitialized or ignored values


// A type used for both players and cell states, interchangeably.  It would
// ideally be an enum, but enums are word-sized and we need a byte-sized type.
typedef char PlayerCode, CellState;

// An opaque type representing game moves.  Each game implementation
// uses it to encode valid moves in a game-specific way.
typedef int GameMove;

#define INVALID_MOVE GameMove(0)
#define PASSING_MOVE GameMove(-1)

// FIXME: static consts are preferred, but I'm not convinced they're as fast
// (But measure the perf impact of these things; intelligible code matters
// more than a 1% perf improvement!)
// static const GameMove g_invalid_move = GameMove(0);
// static const GameMove g_passing_move = GameMove(-1);

// Some maximum string buffer sizes
#define MAX_MOVE_STRING_SIZE 20

// Whether the global profiling mode is enabled
extern bool g_profiling;

// Current high-water mark for search depth
extern int g_current_search_depth;


// Global counters

#if MINIMAX_STATISTICS
    extern unsigned __int64 g_minimax_calls;
    extern unsigned __int64 g_moves_applied;
    extern unsigned __int64 g_evaluated_nodes;
    extern unsigned __int64 g_beta_cutoffs;
    extern unsigned __int64 g_total_evaluated_nodes;
    extern unsigned __int64 g_total_beta_cutoffs;
#endif


// Game registration stuff

typedef class GameState* GameCreator();
typedef class Player* PlayerCreator(bool human, int player_num);

struct GameDesc
{
    const char* m_name;
    GameCreator* create_game;
    PlayerCreator* create_player;
};

extern const GameDesc** g_game_list;
extern int g_num_games;

int register_game(const char* name, GameCreator*, PlayerCreator*);


// Abstract base classes to be implemented for each game

class Player
{
public:

    Player(bool human, int id =0, const char* name =0, bool attributes =0)
      : m_human(human), m_id(id), m_name(name), m_attributes(attributes) {}
    virtual ~Player() {}

    //UNUSED: void set_player_name(const char* name) {m_name = name;}  // Should allocate local copy
    //UNUSED: const char* get_player_name() const {return m_name;}  // E.g. "Fred", "IntelCore2Duo3GhzStrategy7"
    //UNUSED: int get_attributes() const {return m_attributes;}
    //UNUSED: int get_id() const {return m_id;}

    bool is_human() const {return m_human;}

    virtual const char* get_side_name() const {return m_id == 0 ? "First player" : "Second player";}

    // Override this function to return more descriptive side names for games
    // that use them (e.g. "black", "crosses")

protected:

    bool m_human;
    int m_id;
    const char* m_name;
    int m_attributes;
};


// FIXME: clean up, or remove altogether if it doesn't help.
// Would be better to make the millions of allocations unnecessary in the first place.
#if USE_GAMENODE_HEAP
    #define USING_GAMENODE_HEAP() \
        FORCEINLINE __bcount(bytes) void* operator new(size_t bytes) \
        { \
            ASSERT(VALID_HANDLE(g_gamenode_heap)); \
            void* pointer = HeapAlloc(g_gamenode_heap, 0, bytes); \
            VERIFY_PTR(pointer); \
            ++g_allocations; \
            return pointer; \
        } \
        FORCEINLINE void operator delete(__in void* pointer) \
        { \
            ASSERT(VALID_HANDLE(g_gamenode_heap)); \
            --g_allocations; \
            VERIFY_TRUE(HeapFree(g_gamenode_heap, 0, pointer)); \
        }
#else
    #define USING_GAMENODE_HEAP()
#endif


class GameState
{
public:  // Used by frontend.cpp

    virtual ~GameState() {}

    // Implemented or overriden by derived classes
    virtual RESULT set_initial_position(size_t n, __in_bcount(n) const char*) {n; return FAIL;}
    virtual RESULT set_value_function(int) {return OK;}
    virtual const Player* player_ahead() const =0;
    virtual GameMove read_move(const char*) const =0;
    virtual void write_move(GameMove, int size, __in_ecount(size) char*) const =0;
    virtual bool valid_move(GameMove) =0;
    virtual bool equivalent_moves(GameMove m1, GameMove m2) const {return m1 == m2;}
    virtual void display(size_t size, __out_ecount(size) char*) const =0;
    virtual void display_score_sheet(bool, size_t size, __out_ecount(size) char*) const =0;
    virtual bool game_over() =0;
    virtual void reset();

    // For use by the GUI frontend only
    virtual int get_rows() const =0;
    virtual int get_columns() const =0;
    virtual int get_cell_states_count() const =0;
    virtual const char* get_cell_state_image_name(int) const =0;
    virtual int get_cell_state(int, int) const =0;

    // Implemented by game.cpp or inline
    void set_player(int n, const Player* player) {m_players[n] = player;}
    const Player* player_to_move() const {return m_players[m_player_up];}
    Value analyze(int target_depth, int max_analysis_time, __out GameMove* ret_move,
                  Value lower_bound =INVALID_VALUE, Value upper_bound =INVALID_VALUE);
    #if MAXIMIZE_VICTORY
        Value maximize_victory(__out GameMove* ret_move);
    #endif
    RESULT perform_move(GameMove);
    void revert_move();
    void set_output_buffer(char* buffer, CRITICAL_SECTION* buffer_access_protector)
    {
        m_output_buffer = buffer;
        m_output_buffer_protector = buffer_access_protector;
    }

protected:  // Used by derived classes only

    GameState() : m_initial_node(NULL), m_current_node(NULL), m_output_buffer(NULL), m_output_buffer_protector(NULL)
        {m_players[0] = m_players[1] = NULL;}

    enum GameAttributes  // Aspects of interest to the frontend or the engine
    {
        eNone = 0x0,     // Absence of traits
        eGreedy = 0x1,   // Try to win by a devastating margin
        eAttributeCount
    };
    virtual GameAttributes game_attributes() const {return eNone;}

    virtual GameMove* get_possible_moves() const =0;
    virtual RESULT apply_move(GameMove) =0;
    virtual RESULT apply_passing_move() {return FAIL;}  // No passing by default
    virtual void undo_last_move() =0;
    virtual Value position_val() const =0;
    virtual Value game_over_val() const {return position_val();}  // Value of the position if the game has ended (FIXME: explain better)

    // FIXME: could remove these; see note on m_winner in connect4.h
    Value victory_val() const {return m_player_up == 0 ? VICTORY_VALUE : -VICTORY_VALUE;}  // A value representing victory for the player to move
    Value defeat_val() const {return -victory_val();}
    void output(__in __format_string PCSTR pFormat, ...) const;

    // For board-based games, "Cell" represents a location on the board
    #pragma warning(disable:4201)  // Don't warn on nameless structs/unions below
    struct Cell
    {
        union {int m_representation; struct {short x; short y;};};
        Cell() : m_representation(-1) {}
        Cell(int n) : m_representation(n) {}
        Cell(int _x, int _y) : x(short(_x)), y(short(_y)) {}
        operator GameMove() const {return m_representation;}
    };

    const Player* m_players[2];
    PlayerCode m_player_up;
    int m_move_number;
    char* m_output_buffer;
    CRITICAL_SECTION* m_output_buffer_protector;

private:

    #define FULLY_ANALYZED 1000000  // Assuming no games can be 1e6 moves long

    struct GameNode
    {
        Value value;         // Minimum value of this position for the player due to move
        int explored_depth;  // Depth of the analysis performed on this position so far
                             // (or FULLY_ANALYZED if already exhaustively searched)
        int child_count;     // Number of moves available in this position

        // Possible continuations of the game from this position, sorted by their
        // estimated value to the player to move.
        struct Child
        {
            GameMove move;
            GameNode* resulting_node;
            USING_GAMENODE_HEAP();
        } *continuations;

        GameNode(Value v) : value(v), explored_depth(-1), child_count(-1), continuations(NULL) {}
        ~GameNode() {delete[] continuations;}

        USING_GAMENODE_HEAP();
    };

    GameNode* m_initial_node;  // Top-level node; beginning of the game
    GameNode* m_current_node;  // Points to the current game state

    FORCEINLINE Value max_val() const {return m_player_up == 0 ? LIMIT_VALUE : -LIMIT_VALUE;}  // An impossibly high value for the player to move
    FORCEINLINE Value min_val() const {return m_player_up == 0 ? -LIMIT_VALUE : LIMIT_VALUE;}  // An impossibly low value for the player to move
    FORCEINLINE bool better(Value v1, Value v2) const {return m_player_up == 0 ? (v1 > v2) : (v1 < v2);}
    FORCEINLINE bool worse(Value v1, Value v2) const {return m_player_up == 1 ? (v1 > v2) : (v1 < v2);}
    FORCEINLINE bool better_or_equal(Value v1, Value v2) const {return !worse(v1, v2);}
    FORCEINLINE bool worse_or_equal(Value v1, Value v2) const {return !better(v1, v2);}
    FORCEINLINE bool is_victory(Value v) const {return m_player_up == 0 ? (v >= VICTORY_VALUE) : (v <= -VICTORY_VALUE);}
    FORCEINLINE bool is_defeat(Value v) const {return m_player_up == 1 ? (v >= VICTORY_VALUE) : (v <= -VICTORY_VALUE);}

    FORCEINLINE void adjust_node_position(GameNode::Child* list, int list_length);
    FORCEINLINE void generate_move_list(GameNode* node);

    Value minimax(int depth, GameNode* node, Value floor, Value ceiling);

    void prune_tree(GameNode* node)
    {
        for (int n = 0; n < node->child_count; ++n)
            prune_tree(node->continuations[n].resulting_node);
        delete node;
    }

    // For debugging
    public: void dump_tree(int depth =3, GameNode* node =NULL, int indentation =0) const;
};
