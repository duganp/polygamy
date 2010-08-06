// NetWrapper.h

#ifndef NETWRAPPER_H
#define NETWRAPPER_H

#include "windows.h"
#include "game.h"


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
    (KalahGameState::creator != NULL);


using namespace System;

namespace NetWrapper
{
    public ref class ManagedGameState
    {
        public:
            ManagedGameState(GameState* pGameState);
            ~ManagedGameState();

            GameMove MoveFromString(String^ move_string);
            String^ MoveToString(GameMove move);
            bool IsMoveValid(GameMove move) {return m_pGameState->valid_move(move);}
            String^ GetScoreSheet(bool include_moves);
            int GetRows() {return m_pGameState->get_rows();}
            int GetColumns() {return m_pGameState->get_columns();}
            int GetCellStatesCount() {return m_pGameState->get_cell_states_count();};
            String^ GetCellStateImageName(int state) {return gcnew String(m_pGameState->get_cell_state_image_name(state));}
            int GetCellState(int row, int column) {return m_pGameState->get_cell_state(row, column);}
            bool IsGameOver() {return m_pGameState->game_over();}
            void ResetGame() {m_pGameState->reset();}
            String^ GetPlayerToMove() {return gcnew String(m_pGameState->get_player_name(m_pGameState->player_up()));}
            String^ GetPlayerAhead() {return gcnew String(m_pGameState->get_player_name(m_pGameState->player_ahead()));}
            Value AnalyzePosition(int target_depth, int max_analysis_time, GameMove% ret_move);
            bool PerformMove(GameMove move) {return m_pGameState->perform_move(move).ok();}
            void RevertMove() {m_pGameState->revert_move();}
            String^ GetOutputText();

        private:
            GameState* m_pGameState;
            char* m_unmanaged_output_buffer;
            CRITICAL_SECTION* m_protect_output_buffer;
    };

    public ref class ManagedGameList
    {
        public:
            int GetGameCount() {return g_num_games;}
            String^ GetGameName(int index) {return gcnew String(g_game_list[index]->m_name);}
            ManagedGameState^ CreateGame(int index);
    };
}

#endif // NETWRAPPER_H
