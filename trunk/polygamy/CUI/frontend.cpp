// frontend.cpp

#include "shared.h"   // Precompiled header; obligatory
#include "game.h"     // Base class for game definitions and minimax code
#include "thinker.h"  // Independent thinking thread

// FIXME: the "static int force_initialization" trick isn't working any more,
// so this hack is necessary to force all the games to be included:
#include "..\games\ataxx.h"
#include "..\games\othello.h"
#include "..\games\connect4.h"
#include "..\games\tictactoe.h"
#include "..\games\kalah.h"

static bool all_games_registered =
    (AtaxxGameState::creator != NULL) &&
    (Connect4GameState::creator != NULL) &&
    (OthelloGameState::creator != NULL) &&
    (TicTacToeGameState::creator != NULL) &&
    (KalahGameState::creator != NULL);  // Forces all code to be included

#ifndef DEFAULT_MAXIMUM_DEPTH
    #define DEFAULT_MAXIMUM_DEPTH 99  // Default search depth if unspecified by user
#endif

#ifndef DEFAULT_ANALYSIS_TIME
    #define DEFAULT_ANALYSIS_TIME 5  // Default analysis time if unspecified by user
#endif


#if USE_GAMENODE_HEAP
    HANDLE g_gamenode_heap = 0;
    unsigned __int64 g_allocations = 0;
#endif


static void show_state(GameState* pGameState)
{
    if (!g_profiling)
    {
        char board[1024];
        pGameState->display(sizeof board, board);
        printf("%s", board);
    }
}


static void play(GameState* pGameState, int search_depth, int maximum_analysis_time, int value_functions[2])
{
    // The independent thinking thread
    // FIXME: to do later
    //Thinker thinker;
    //thinker.initialize();  // FIXME maybe: no error checking here, or hardly anywhere else

    show_state(pGameState);

    int current_move = 0;
    int current_value_function_index = 1;
    GameMove move = INVALID_MOVE;
    char move_string[MAX_MOVE_STRING_SIZE];

    while (!pGameState->game_over())
    {
        // Switch strategies for each move
        // FIXME: Any of the commands below will cause a 'continue' which leads
        // 'current_value_function_index' to be toggled extra times.
        pGameState->set_value_function(value_functions[current_value_function_index ^= 1] - 1);

        if (pGameState->player_to_move()->is_human())
        {
            const char* human_player = pGameState->player_to_move()->get_side_name();
            printf("%s move? ", human_player);
            char input_string[20];
            gets_s(input_string);
            int command = toupper(input_string[0]);

            if (command == 'Q')
            {
                printf("%s quits.\n", human_player);
                break;
            }
            if (command == 'T')
            {
                int depth = atoi(input_string + 1);
                if (depth <= 0) depth = 2;
                printf("Displaying first %d levels of game tree.\n", depth);
                pGameState->dump_tree(depth);
                continue;
            }
            if (command == 'S')
            {
                printf("Getting a computer suggestion for %s...\n", human_player);
                Value value = pGameState->analyze(search_depth, maximum_analysis_time, &move);
                if (!move)
                {
                    printf("No valid moves left.\n");
                    break;
                }
                ASSERT(pGameState->valid_move(move));
                pGameState->write_move(move, sizeof move_string, move_string);
                printf("%s move: %s (estimated value %d)\n", human_player, move_string, value);
                VERIFY(pGameState->perform_move(move));
                continue;
            }
            if (command == '-')  // Take back the last two moves (computer and human)
            {
                if (current_move >= 2)
                {
                    pGameState->revert_move();
                    pGameState->revert_move();
                    current_move -= 2;
                    show_state(pGameState);
                }
                else
                {
                    printf("Can't undo the last two moves; %s been played yet.\n", current_move == 0 ? "none have" : "only one has");
                }
                continue;
            }
            if (command == 'X')  // Change maximum search depth
            {
                search_depth = atoi(input_string + 1);
                printf("Set maximum search depth to %d.\n", search_depth);
                continue;
            }
            if (command == 'M')  // Change maximum search time
            {
                maximum_analysis_time = atoi(input_string + 1);
                printf("Set maximum search time to %d seconds.\n", maximum_analysis_time);
                continue;
            }
            #if ZERO_WINDOW_SEARCHES
                if (command == 'Z')  // Zero-window search for a given value
                {
                    int target_value = atoi(input_string + 1);
                    printf("Attempting a zero-window search for value %d...\n", target_value);
                    Value value = pGameState->analyze(search_depth, maximum_analysis_time, &move, target_value, target_value);
                    printf("analyze(depth %d, max time %d, window value %d) returned value %d\n", search_depth, maximum_analysis_time, target_value, value);
                    continue;
                }
            #endif
            #if MAXIMIZE_VICTORY
                if (command == 'W')  // Analyze position looking for the best win
                {
                    printf("Searching for the most devastating win possible for %s...\n", human_player);
                    Value value = pGameState->maximize_victory(&move);
                    ASSERT(pGameState->valid_move(move));
                    pGameState->write_move(move, sizeof move_string, move_string);
                    printf("%s move: %s (estimated value %d)\n", human_player, move_string, value);
                    VERIFY(pGameState->perform_move(move));
                    continue;
                }
            #endif

            move = pGameState->read_move(input_string);

            if (!pGameState->valid_move(move))
            {
                printf("Invalid move.\n");
                continue;
            }
            if (FAILED(pGameState->perform_move(move)))
            {
                printf("Illegal move.\n");
                continue;
            }
        }
        else  // Computer move
        {
            Value value = pGameState->analyze(search_depth, maximum_analysis_time, &move);

            if (!move)
            {
                printf("No valid moves left.\n");
                BREAK_MSG("Should be impossible?\n");
                break;
            }

            if (!g_profiling)
            {
                ASSERT(pGameState->valid_move(move));
                pGameState->write_move(move, sizeof move_string, move_string);
                //printf("%s move: %s (strategy %d; estimated value %d)\n", pGameState->player_to_move()->get_side_name(), move_string, value_functions[current_value_function_index], value);  // FIXME: use or remove strategy stuff
                printf("%s move: %s (estimated value %d)\n", pGameState->player_to_move()->get_side_name(), move_string, value);
            }

            VERIFY(pGameState->perform_move(move));
        }

        show_state(pGameState);
        ++current_move;
    }

    #if MINIMAX_STATISTICS
        printf("TOTAL: %I64u nodes evaluated, %I64u beta cutoffs\n", g_total_evaluated_nodes, g_total_beta_cutoffs);
    #endif
}


void main(int argc, char** argv)
{
    ComponentTraceBegin();
    TRACE(INFO, "%s launched", *argv);

    #if USE_GAMENODE_HEAP
        #define INITIAL_NODE_HEAP_MEGABYTES 10
        g_gamenode_heap = HeapCreate(HEAP_NO_SERIALIZE, INITIAL_NODE_HEAP_MEGABYTES * (1<<20), 0);
        ASSERT(VALID_HANDLE(g_gamenode_heap));
    #endif

    int maximum_depth = DEFAULT_MAXIMUM_DEPTH;
    int maximum_analysis_time = DEFAULT_ANALYSIS_TIME;
    int value_functions[2] = {1, 2};  // Default strategies for 1st and 2nd computer players
    int rng_seed = -1;

    // If only one game is available, just select it and don't force the user to
    int chosen_game = (g_num_games == 1) ? 1 : 0;

    // Default to letting the human play first
    int human_player = 1;

    // Position description optionally read from a file
    char* position_file_name = NULL;
    char position_string[50000] = {0};  // FIXME: ugly magic number 50000. Make dynamic.
    size_t position_string_size = 0;

    // Process arguments
    while (--argc)
    {
        if (**++argv == '-')
        {
            switch (toupper(*++*argv))
            {
                case 'G':  // Game selection
                    chosen_game = atoi(*argv + 1);
                    if (chosen_game < 1 || chosen_game > g_num_games)
                    {
                        printf("Bad game number %d (valid games are 1 to %d).\n", chosen_game, g_num_games);
                        chosen_game = 0;
                    }
                    break;

                case 'H':  // Playing order
                    if (g_profiling)
                    {
                        printf("Ignoring 'h' option in profiling mode.\n");
                    }
                    else
                    {
                        human_player = atoi(*argv + 1);
                        if (human_player != 1 && human_player != 2)
                        {
                            printf("Invalid 'h' option; must specify 1 or 2.\n");
                            human_player = 1;
                        }
                    }
                    break;

                case 'D':  // Maximum search depth
                    if (atoi(*argv + 1) < 0)
                    {
                        printf("Ignoring invalid depth %s.\n", *argv+1);
                    }
                    else
                    {
                        maximum_depth = atoi(*argv + 1);
                    }
                    break;

                case 'M':  // Maximum analysis time per move
                    maximum_analysis_time = atoi(*argv + 1);
                    if (maximum_analysis_time < 1) maximum_analysis_time = 1;
                    break;

                case 'V':  // Position evaluation functions
                    if ((*argv)[1] == '1' && (*argv)[2] == '=')
                    {
                        value_functions[0] = atoi(*argv + 3);
                    }
                    else if ((*argv)[1] == '2' && (*argv)[2] == '=')
                    {
                        value_functions[1] = atoi(*argv + 3);
                    }
                    else
                    {
                        printf("Invalid 'v' option.\n");
                    }
                    break;

                case 'S':  // Random number generator seed
                    rng_seed = atoi(*argv + 1);
                    break;

                case 'C':  // Make computer play itself
                    human_player = 0;
                    break;

                case 'P':  // Profiling mode
                    g_profiling = true;
                    human_player = 0;
                    break;

                case 'F':  // Load initial position from the specified file
                {
                    position_file_name = *argv + 1;
                    FILE* position_file = NULL;
                    errno_t error = fopen_s(&position_file, position_file_name, "r");
                    if (error)
                    {
                        printf("Failed to open position file \"%s\"; error %d\n", position_file_name, error);
                        return;
                    }
                    else
                    {
                        // Read as much of the file as will fit in position_string
                        ASSERT(position_file);
                        position_string_size = fread_s(position_string, sizeof position_string, 1, sizeof position_string, position_file);
                        BOOL file_completely_read = feof(position_file);
                        fclose(position_file);

                        if (position_string_size == 0)
                        {
                            printf("Failed to read read data from position file \"%s\"; error %d\n", position_file_name, error);
                            return;
                        }
                        if (!file_completely_read)
                        {
                            printf("Position file \"%s\" is longer than the maximum size of %d bytes\n", position_file_name, sizeof position_string);
                            return;
                        }
                    }
                    break;
                }
                default:
                    printf("Bad option '%c'.\n\n", **argv);
                    printf("Valid options:\n"
                           "\t-g<N>\tPlay game N (see list below)\n"
                           "\t-h<N>\tHuman plays in Nth position\n"
                           "\t-d<N>\tSet maximum search depth to N\n"
                           "\t-m<N>\tSet maximum time per computer move to N\n"
                           "\t-v<P>=<N>\tUse position evaluator N for computer player P\n"
                           "\t-s<N>\tUse random number generator seed N\n"
                           "\t-c\tComputer plays itself\n"
                           "\t-p\tRun silently (for performance testing)\n"
                           "\t-fFILE\tLoad initial position from FILE\n");
                    return;
            }
        }
    }

    if (!chosen_game)
    {
        printf("Choose a game:\n");
        for (int i = 0; i < g_num_games; ++i)
        {
            printf("  %d: %s.\n", i+1, g_game_list[i]->m_name);
        }
        do
        {
            char game_name[20];
            gets_s(game_name);
            sscanf_s(game_name, "%d", &chosen_game);
        }
        while (chosen_game < 1 || chosen_game > g_num_games);
    }

    // If no random number generator seed has been specified, select one based
    // on the current time (except when profiling, which must be repeatable)
    if (rng_seed == -1)
    {
        rng_seed = g_profiling ? 0 : int(time(NULL));
    }
    srand(rng_seed);

    const GameDesc* pGame = g_game_list[chosen_game-1];
    Player* pPlayer0 = pGame->create_player(human_player == 1, 0);
    Player* pPlayer1 = pGame->create_player(human_player == 2, 1);

    GameState* pState = pGame->create_game();
    pState->set_player(0, pPlayer0);
    pState->set_player(1, pPlayer1);

    if (FAILED(pState->set_value_function(value_functions[0] - 1)))
    {
        printf("First player strategy %d is invalid.\n", value_functions[0]);
        return;
    }
    if (FAILED(pState->set_value_function(value_functions[1] - 1)))
    {
        printf("Second player strategy %d is invalid.\n", value_functions[1]);
        return;
    }

    if (*position_string != 0)
    {
        if (SUCCEEDED(pState->set_initial_position(position_string_size, position_string)))
        {
            printf("Loaded initial position from \"%s\".\n", position_file_name);
        }
        else
        {
            printf("Position file \"%s\" is invalid.\n", position_file_name);
            return;
        }
    }

    if (g_profiling)
    {
        // printf("%s: depth %u: max time %d: player 1 strategy %d: player 2 strategy %d: ", pGame->m_name,
        //        maximum_depth, maximum_analysis_time, value_functions[0], value_functions[1]);
        // FIXME: use or remove strategy stuff
        printf("%s: depth %u: max time %d: ", pGame->m_name, maximum_depth, maximum_analysis_time);

        DELAY_CHECKPOINT();

        play(pState, maximum_depth, maximum_analysis_time, value_functions);

        printf("Game took %.6f seconds.\n", DELAY_MEASURED() / 1000);
        char transcript[10000];
        //pState->display_score_sheet(false, sizeof transcript, transcript);
        pState->display_score_sheet(true, sizeof transcript, transcript);  // FIXME: temp, or control via cmdline option
        printf("%s", transcript);
    }
    else
    {
        // printf("%s: %s vs. %s: depth %d: max time %d: player 1 strategy %d: player 2 strategy %d: seed %d\n",
        //        pGame->m_name, human_player == 1 ? "human" : "computer", human_player == 2 ? "human" : "computer",
        //        maximum_depth, maximum_analysis_time, value_functions[0], value_functions[1], rng_seed);
        // FIXME: use or remove strategy stuff
        printf("%s: %s vs. %s: depth %d: max time %d: seed %d\n", pGame->m_name,
               human_player == 1 ? "human" : "computer",
               human_player == 2 ? "human" : "computer",
               maximum_depth, maximum_analysis_time, rng_seed);

        if (human_player)
        {
            printf("Type 'P' to pass, '-' to take back your last move, 'T' to display game tree, 'X'..., 'M'..., 'Z'..., or 'Q' to quit.\n");
        }

        bool play_again = human_player ? true : false;
        do
        {
            play(pState, maximum_depth, maximum_analysis_time, value_functions);

            const Player* winner = pState->player_ahead();
            printf("Game over; %s was victorious.\n", winner ? winner->get_side_name() : "neither player");

            if (human_player) for (;;)
            {
                printf("Type 'P' to play again, 'T' for a transcript of the game, or any other key to quit.\n");
                char action[20]; gets_s(action); *action = char(toupper(*action));
                if (*action == 'T')
                {
                    char transcript[10000];
                    pState->display_score_sheet(true, sizeof transcript, transcript);
                    printf("%s", transcript);
                }
                if (*action == 'P')
                {
                    pState->reset();
                }
                else
                {
                    play_again = false;
                }
                break;
            }
            else
            {
                char transcript[10000];
                pState->display_score_sheet(true, sizeof transcript, transcript);  // FIXME: temp, or control via cmdline option
                printf("%s", transcript);
            }
        }
        while (play_again);
    }

    delete pState;
    delete pPlayer0;
    delete pPlayer1;

    #if USE_GAMENODE_HEAP
        ASSERT(g_allocations == 0);
        VERIFY_WIN32(HeapDestroy(g_gamenode_heap));
    #endif

    TRACE(INFO, "Polygamy exiting");
    ComponentTraceEnd();
}
