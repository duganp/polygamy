// game.cpp

#include "shared.h"  // Precompiled header; obligatory
#include "game.h"    // Our public interface


// Whether the global profiling mode is enabled
bool g_profiling = false;

// Current high-water mark for search depth
int g_current_search_depth = 0;


#ifndef MINIMAX_TRACE
    #define MINIMAX_TRACE 0  // Minimax algorithm logging
#endif
#if MINIMAX_TRACE
    #define MXTRACE(p) p
#else
    #define MXTRACE(p)
#endif


//
// Global counters
//

#if MINIMAX_STATISTICS
    unsigned __int64 g_minimax_calls = 0;
    unsigned __int64 g_moves_applied = 0;
    unsigned __int64 g_evaluated_nodes = 0;
    unsigned __int64 g_beta_cutoffs = 0;
    unsigned __int64 g_total_evaluated_nodes = 0;
    unsigned __int64 g_total_beta_cutoffs = 0;
#endif


//
// Game registration stuff
//

const GameDesc** g_game_list = NULL;
int g_num_games = 0;

int register_game(const char* name, GameCreator* game_creator, PlayerCreator* player_creator)
{
    GameDesc* game_desc = new GameDesc;  // FIXME: Never freed, but don't care
    game_desc->m_name = name;
    game_desc->create_game = game_creator;
    game_desc->create_player = player_creator;

    const GameDesc** new_game_list = new const GameDesc*[g_num_games + 1];
    memcpy(new_game_list, g_game_list, g_num_games * sizeof(GameDesc*));
    delete g_game_list;
    g_game_list = new_game_list;
    g_game_list[g_num_games] = game_desc;

    return ++g_num_games;
};


//
// The base destructors are defined here because the C++ Standard states that
// pure virtual methods cannot be defined inline.
// FIXME: Remove after confirming this builds with GCC
//

//Player::~Player() {}
//GameState::~GameState() {}


//
// Reset the base object's state.  Note that most derived game classes must
// implement their own reset() method, and when they do, it must be sure to
// call this one using GameState::reset().
//

void GameState::reset()
{
    TRACE_VOID_METHOD();

    m_move_number = 0;
    m_player_up = 0;

    if (m_initial_node)
    {
        prune_tree(m_initial_node);
    }

    m_current_node = m_initial_node = new GameNode(0);
}


//
// Find the appropriate position for a move in an ordered child list
//

FORCEINLINE void GameState::adjust_node_position(GameNode::Child* list, int list_length)
{
    Value value = list[list_length].resulting_node->value;

    int insert_pos = 0;
    while (insert_pos < list_length && worse_or_equal(value, list[insert_pos].resulting_node->value))
    {
        ++insert_pos;
    }

    if (insert_pos != list_length)
    {
        GameNode::Child moving_child = list[list_length];

        // Shift remaining nodes in the list to the right to make room for the new one
        memmove(list + insert_pos + 1, list + insert_pos, (list_length - insert_pos) * sizeof GameNode::Child);

        list[insert_pos] = moving_child;
    }
}


//
// Generate a list of legal moves sorted by immediate value
//
// FIXME: might just want to make get_possible_moves() only return actually valid moves
// FIXME: if we do that, we may be able to eliminate apply_passing_move() altogether
//

FORCEINLINE void GameState::generate_move_list(GameNode* node)
{
    if (node->child_count == -1)
    {
        ASSERT(node->continuations == NULL);
        ASSERT(node->explored_depth == -1);
        node->child_count = 0;
        node->explored_depth = 0;

        GameNode::Child child_list[1000];  // FIXME: magic number (max no. of moves available ever in any game)

        GameMove* possible_moves = get_possible_moves();

        for (GameMove* move = possible_moves; *move; ++move)
        {
            if (FAILED(apply_move(*move))) continue;
            child_list[node->child_count].move = *move;
            child_list[node->child_count].resulting_node = new GameNode(position_val());
            undo_last_move();
            adjust_node_position(child_list, node->child_count);
            ++node->child_count;
            ASSERT(node->child_count < ARRAYSIZE(child_list));
            #if MINIMAX_STATISTICS
                ++g_moves_applied;
                ++g_evaluated_nodes;
            #endif
        }

        if (node->child_count == 0 && SUCCEEDED(apply_passing_move()))
        {
            node->child_count = 1;
            child_list[0].move = PASSING_MOVE;
            child_list[0].resulting_node = new GameNode(position_val());
            undo_last_move();
            #if MINIMAX_STATISTICS
                ++g_moves_applied;
                ++g_evaluated_nodes;
            #endif
        }

        if (node->child_count == 0)
        {
            node->value = game_over_val();
            node->explored_depth = FULLY_ANALYZED;
        }
        else
        {
            node->value = child_list[0].resulting_node->value;
            node->continuations = new GameNode::Child[node->child_count];
            memcpy(node->continuations, child_list, node->child_count * sizeof GameNode::Child);
        }

        delete[] possible_moves;
    }
}


RESULT GameState::perform_move(GameMove move)
{
    TRACE_VOID_METHOD();

    // If this is the first move of the game, m_current_node may not have been
    // populated yet.
    generate_move_list(m_current_node);
    ASSERT(m_current_node->child_count > 0);

    RESULT hr = (move == PASSING_MOVE) ? apply_passing_move() : apply_move(move);

    if (SUCCEEDED(hr))
    {
        // In case this is a human move and doesn't match the computer's chosen
        // optimal move, we move it to the head of the current node's move list.
        // This way the game tree provides a record of the course of the game,
        // and stores the alternative moves that were available at each position
        // along with their estimated values (but not their entire subtrees).

        GameNode* new_current_node = NULL;

        // FIXME: computer may generate B1A1 as a valid move in the list
        // (to stand in for B1A1, B2A1 and A2A1) and if the user then does
        // a different variant (e.g. A2A1) this code won't find it in the
        // list and the ASSERT below will fire.

        // FIXME: Could remove the 'equivalent_moves' stuff completely if it
        // isn't applicable to games other than Ataxx, by representing clone
        // moves using only the destination cell (not the source cell)

        for (int n = 0; n < m_current_node->child_count; ++n)
        {
            if (equivalent_moves(move, m_current_node->continuations[n].move))
            {
                // Found the continuation that was actually used; place it at
                // the head of the list (a no-op if n is 0).
                GameNode::Child path_taken = m_current_node->continuations[n];
                new_current_node = path_taken.resulting_node;

                // Shift remaining nodes in the list to the right to make room for the new one
                memmove(m_current_node->continuations + 1, m_current_node->continuations, n * sizeof GameNode::Child);

                m_current_node->continuations[0] = path_taken;

                // Note that this leaves the remainder of the list (beyond
                // continuations[n]) undisturbed, so this loop can proceed.
            }
            else
            {
                // Found an alternative continuation; remove its subtrees,
                // but leave its value and explored depth intact.
                GameNode* path_not_taken = m_current_node->continuations[n].resulting_node;

                for (int m = 0; m < m_current_node->continuations[n].resulting_node->child_count; ++m)
                {
                    prune_tree(path_not_taken->continuations[m].resulting_node);
                }

                delete[] path_not_taken->continuations;
                path_not_taken->continuations = NULL;
                path_not_taken->child_count = 0;
            }
        }

        ASSERT(new_current_node != NULL);
        m_current_node = new_current_node;
    }

    return hr;
}


void GameState::revert_move()
{
    TRACE_VOID_METHOD();

    undo_last_move();
    prune_tree(m_current_node);
    m_current_node = new GameNode(0);
}


//
// Wrapper for minimax()
//
//  target_depth: target search depth
//  maximum_analysis_time: ...
//  ret_move: returns best move found, or NULL if none available
//  lower_bound: FIXME
//  upper_bound: FIXME
//

Value GameState::analyze(int target_depth, int maximum_analysis_time, __out GameMove* ret_move, Value lower_bound, Value upper_bound)
{
    TRACE_VOID_METHOD();

    if (lower_bound == INVALID_VALUE) lower_bound = min_val();
    if (upper_bound == INVALID_VALUE) upper_bound = (game_attributes() & eGreedy) ? max_val() : victory_val();

    ASSERT(ret_move != NULL);
    *ret_move = INVALID_MOVE;

    // Populate the move list if necessary
    generate_move_list(m_current_node);
    GameNode::Child* children = m_current_node->continuations;

    // We can skip analysis and return a move right away in 3 cases:
    //
    // 1. We've already explored the game tree to the requested depth.
    //    (The same result may be produced by the code below, but it
    //    does no harm to skip it altogether for clarity - FIXME?)
    //
    // 2. There is only one valid move.
    //
    // 3. We can achieve the requested value immediately.

    if (m_current_node->explored_depth >= target_depth ||
        m_current_node->child_count == 1 ||
        better_or_equal(m_current_node->value, upper_bound))
    {
        *ret_move = children[0].move;
        return m_current_node->value;
    }

    m_current_node->value = INVALID_VALUE;  // Make sure this is never used [FIXME: remove?]
    bool already_bragged = false;

    DELAY_CHECKPOINT();

    // In each iteration up to the requested search depth we call minimax() for
    // each move in our ordered list, and re-order it to optimize the next pass.

    for (int current_depth = 0; current_depth < target_depth; ++current_depth)
    {
        // Used by some games' evaluation functions
        g_current_search_depth = max(g_current_search_depth, current_depth);

        #if MINIMAX_TRACE
            output("\b\b\b\b\b\b\b\b\bMINIMAX: Depth %d move order: ", current_depth + 1);
            for (int n = 0; n < m_current_node->child_count; ++n)
            {
                char move_string[MAX_MOVE_STRING_SIZE];
                write_move(children[n].move, sizeof move_string, move_string);
                output("%s (%d) ", move_string, children[n].resulting_node->value);
            }
            output("\nMINIMAX: ");
        #endif

        // Used to detect when deeper searches would be redundant
        bool position_fully_analyzed = true;  // Falsified as needed below

        Value best_value_so_far = lower_bound;

        for (int n = 0; n < m_current_node->child_count; ++n)
        {
            MXTRACE(char move_string[MAX_MOVE_STRING_SIZE];
                    write_move(children[n].move, sizeof move_string, move_string);
                    output("%s", move_string));
            VERIFY(children[n].move == PASSING_MOVE ? apply_passing_move() : apply_move(children[n].move));
            #if MINIMAX_STATISTICS
                ++g_moves_applied;
            #endif

            // Call minimax with an upside-down target range (floor=upper_bound, ceiling=best_value_so_far)
            Value new_value = minimax(current_depth, children[n].resulting_node, upper_bound, best_value_so_far);

            undo_last_move();
            //MXTRACE(output(": value %d, %.3f ms                                                 \nMINIMAX: ", new_value, DELAY_MEASURED()));
            MXTRACE(output(": value %d                                                          \nMINIMAX: ", new_value));  // FIXME - for AB comparisons (no ms)

            // Move this node to the appropriate position in the ordered child list,
            // and refresh the best value so far from the head of the list.
            adjust_node_position(children, n);
            best_value_so_far = children[0].resulting_node->value;

            position_fully_analyzed &= (children[n].resulting_node->explored_depth == FULLY_ANALYZED);

            if (better_or_equal(new_value, upper_bound)) break;  // Reached target value
        }
        // End of move loop

        if (!g_profiling && current_depth > 1 && is_victory(best_value_so_far) && !already_bragged)
        {
            output("Winning within %d moves.\n", current_depth / 2 + 1);
            already_bragged = true;
            #if MAXIMIZE_VICTORY
                return maximize_victory(ret_move);
            #endif
        }

        if (position_fully_analyzed)
        {
            output("Position fully analyzed.\n");
            break;
        }

        float total_seconds = TOTAL_DELAY_MEASURED() / 1000;
        if (total_seconds > float(maximum_analysis_time))
        {
            #if MINIMAX_STATISTICS
                output("Cutting off analysis at depth %d after %f seconds (target = %d)\n\n", current_depth + 1, total_seconds, maximum_analysis_time);
            #endif
            break;
        }
    }
    // End of depth loop

    #if MINIMAX_STATISTICS
        output("Move %d: %I64u nodes evaluated, %I64u moves applied, %I64u minimax calls, %I64u beta cutoffs\n",
               m_move_number + 1, g_evaluated_nodes, g_moves_applied, g_minimax_calls, g_beta_cutoffs);
        g_total_evaluated_nodes += g_evaluated_nodes;
        g_total_beta_cutoffs += g_beta_cutoffs;
        g_evaluated_nodes = g_moves_applied = g_minimax_calls = g_beta_cutoffs = 0;
    #endif

    // Observe that all codepaths above lead to 'children[0].move' containing the
    // best move found so far and 'children[0].resulting_node->value' its value.
    *ret_move = children[0].move;
    return m_current_node->value = children[0].resulting_node->value;
}


//
// Minimax algorithm with alpha-beta optimization
//
//  depth: search depth in plies
//  node: game state to be analyzed
//  floor: base value which this search is trying to improve upon
//  ceiling: abort search if a value equal or better to this is found
//
// Returns: a *floor* for the current position's value to the current player
// (the caller does not need more precision if this floor value is worse for
// it than the best value it can obtain using a different move).
//
// FIXME: clean this up:
//
// Alpha-beta pruning seeks to reduce the number of search tree nodes
// evaluated by the minimax algorithm.  It stops evaluating a move as
// soon as a possibility has been found that proves the move to be no
// better than a previously examined move.  Such moves need not be
// evaluated further.
//
// The algorithm maintains two values, alpha and beta, which represent
// the minimum score that the maximizing player is assured of and the
// maximum score that the minimizing player is assured of, respectively.
// Initially alpha is negative infinity and beta is positive infinity.
// As the recursion progresses the "window" becomes smaller. When beta
// becomes less than alpha, it means that the current position cannot
// be the result of best play by both players and hence need not be
// explored further.
//
// alphabeta(node, depth, A, B)
//     ; B represents previous player's best choice
//     if depth = 0 or node is terminal
//         return node's heuristic value
//     for each child of node
//         A := max(A, -alphabeta(child, depth-1, -B, -A))  // FIXME: not how we do it below
//         if B = A
//             break ; Beta cut-off
//     return A
//
// Initial call: alphabeta(origin, depth, -infinity, +infinity)

Value GameState::minimax(int depth, GameNode* node, Value floor, Value ceiling)
{
    #if MINIMAX_STATISTICS
        ++g_minimax_calls;
    #endif

    // Populate the move list if necessary
    generate_move_list(node);

    // Return if this node has already been analyzed to the requested depth
    // or has been found to be terminal (explored_depth == FULLY_ANALYZED)
    if (depth <= node->explored_depth) return node->value;

    ASSERT(node->child_count != 0);
    GameNode::Child* children = node->continuations;
    ASSERT(node->value == children[0].resulting_node->value);

    node->explored_depth = FULLY_ANALYZED;  // Possibly reduced in loop below

    for (int n = 0; n < node->child_count; ++n)
    {
        MXTRACE(char move_string[MAX_MOVE_STRING_SIZE];
                write_move(children[n].move, sizeof move_string, move_string);
                int backspace_count = strlen(move_string) + 1;
                printf(" %s", move_string));
        VERIFY(children[n].move == PASSING_MOVE ? apply_passing_move() : apply_move(children[n].move));
        #if MINIMAX_STATISTICS
            ++g_moves_applied;
        #endif

        Value new_value = minimax(depth - 1, children[n].resulting_node, ceiling, floor);

        undo_last_move();
        MXTRACE(while (backspace_count--) putchar('\b'));

        // Move this node to the appropriate position in the ordered child list,
        // and refresh the best value so far from the head of the list.
        adjust_node_position(children, n);
        floor = children[0].resulting_node->value;

        // Maintain the invariant that this node's explored depth = 1 + min(child depths)
        node->explored_depth = min(node->explored_depth, 1 + children[n].resulting_node->explored_depth);

        if (better_or_equal(new_value, ceiling))  // Beta cutoff
        {
            #if MINIMAX_STATISTICS
                ++g_beta_cutoffs;
            #endif
            break;
        }
    }

    // If this position is a guaranteed win for either player, we consider it fully analyzed
    // FIXME: I'm not sure this code is strictly needed
//    if (is_victory(floor) || is_defeat(floor))
//    {
//        node->explored_depth = FULLY_ANALYZED;
//    }

    return node->value = floor;
}


#if MAXIMIZE_VICTORY

    Value GameState::maximize_victory(__out GameMove* ret_move)
    {
        ASSERT(ret_move != NULL);
        *ret_move = INVALID_MOVE;

        // Populate the move list if necessary
        generate_move_list(m_current_node);
        GameNode::Child* children = m_current_node->continuations;

        #define TEST_MAX_DEPTH 100
        for (int current_depth = 0; current_depth < TEST_MAX_DEPTH; ++current_depth)
        {
            // Used by some games' evaluation functions
            g_current_search_depth = max(g_current_search_depth, current_depth);

            #if MINIMAX_TRACE
                output("\b\b\b\b\b\b\b\b\b\bMAXIKILL: Depth %d move order: ", current_depth + 1);
                for (int n = 0; n < m_current_node->child_count; ++n)
                {
                    char move_string[MAX_MOVE_STRING_SIZE];
                    write_move(children[n].move, sizeof move_string, move_string);
                    output("%s (%d) ", move_string, children[n].resulting_node->value);
                }
                output("\nMAXIKILL: ");
            #endif

            // Used to detect when deeper searches would be redundant
            bool position_fully_analyzed = true;  // Falsified as needed below

            Value best_value_so_far = min_val();

            for (int n = 0; n < m_current_node->child_count; ++n)
            {
                MXTRACE(char move_string[MAX_MOVE_STRING_SIZE];
                        write_move(children[n].move, sizeof move_string, move_string);
                        output("%s", move_string));
                VERIFY(children[n].move == PASSING_MOVE ? apply_passing_move() : apply_move(children[n].move));
                #if MINIMAX_STATISTICS
                    ++g_moves_applied;
                #endif

                Value new_value = minimax(current_depth, children[n].resulting_node, min_val(), best_value_so_far);
                UNREFERENCED_PARAMETER(new_value);

                undo_last_move();
                MXTRACE(output(": value %d                                                  \nMAXIKILL: ", new_value));

                // Move this node to the appropriate position in the ordered child list,
                // and refresh the best value so far from the head of the list.
                adjust_node_position(children, n);
                best_value_so_far = children[0].resulting_node->value;

                position_fully_analyzed &= (children[n].resulting_node->explored_depth == FULLY_ANALYZED);
            }
            // End of move loop

            if (position_fully_analyzed)
            {
                output("Position fully analyzed.\n");
                break;
            }
        }
        // End of depth loop

        #if MINIMAX_STATISTICS
            output("Move %d: %I64u nodes evaluated, %I64u moves applied, %I64u minimax calls, %I64u beta cutoffs\n",
                   m_move_number + 1, g_evaluated_nodes, g_moves_applied, g_minimax_calls, g_beta_cutoffs);
            g_total_evaluated_nodes += g_evaluated_nodes;
            g_total_beta_cutoffs += g_beta_cutoffs;
            g_evaluated_nodes = g_moves_applied = g_minimax_calls = g_beta_cutoffs = 0;
        #endif

        *ret_move = children[0].move;
        return m_current_node->value = children[0].resulting_node->value;
    }

#endif  // MAXIMIZE_VICTORY


//
// Display a given search tree
//

void GameState::dump_tree(int depth, GameNode* node, int indentation) const
{
    ASSERT(indentation >= 0);
    ASSERT(depth >= 0);

    if (node == NULL)
    {
        node = m_current_node;
    }

    char value_string[100];
    if (node->value >= VICTORY_VALUE)
    {
        sprintf_s(value_string, "%s wins (value %d)", m_players[0]->get_side_name(), node->value);
    }
    else if (node->value <= -VICTORY_VALUE)
    {
        sprintf_s(value_string, "%s wins (value %d)", m_players[1]->get_side_name(), node->value);
    }
    else
    {
        sprintf_s(value_string, "value %d", node->value);
    }

    if (node->child_count == -1)
    {
        output("Unexplored; %s\n", value_string);
    }
    else if (node->child_count == 0)
    {
        if (node->value < VICTORY_VALUE && node->value > -VICTORY_VALUE)
        {
            output("No moves available; ");
        }
        output("%s\n", value_string);
    }
    else
    {
        if (node->explored_depth == FULLY_ANALYZED)
        {
            output("Fully explored; %s", value_string);
        }
        else
        {
            output("Explored to depth %u; %s", node->explored_depth, value_string);
        }

        if (depth > 0)
        {
            output(". %d moves available:\n", node->child_count);
            for (int n = 0; n < node->child_count; ++n)
            {
                for (int i = 0; i < indentation + 3; ++i) putchar(' ');
                char move_string[MAX_MOVE_STRING_SIZE];
                write_move(node->continuations[n].move, sizeof move_string, move_string);
                output("%s: ", move_string);
                dump_tree(depth - 1, node->continuations[n].resulting_node, indentation + 3);
            }
        }
        else
        {
            output("\n");
        }
    }
}


void GameState::output(__in __format_string PCSTR pFormat, ...) const
{
    char message[1024];
    va_list vargs;
    va_start(vargs, pFormat);
    vsprintf_s(message, sizeof message, pFormat, vargs);

    TRACE(INFO, "%s", message);

    printf("%s", message);

    if (m_output_buffer != NULL)
    {
        EnterCriticalSection(m_output_buffer_protector);
        // FIXME: Add a cursor so we don't overwrite already-queued messages
        memcpy(m_output_buffer, message, strlen(message) + 1);
        LeaveCriticalSection(m_output_buffer_protector);
    }
}
