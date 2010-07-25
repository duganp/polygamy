// NetWrapper.cpp

#include "NetWrapper.h"

using namespace System::Runtime::InteropServices;

using namespace NetWrapper;


ManagedGameState::ManagedGameState(GameState* pGameState, Player* player0, Player* player1)
{
    m_pGameState = pGameState;
    m_player0 = player0;
    m_player1 = player1;

    m_unmanaged_output_buffer = new char[1024];
    m_unmanaged_output_buffer[0] = '\0';
    m_protect_output_buffer = new CRITICAL_SECTION;
    InitializeCriticalSection(m_protect_output_buffer);
    m_pGameState->set_output_buffer(m_unmanaged_output_buffer, m_protect_output_buffer);
}

ManagedGameState::~ManagedGameState()
{
    delete m_pGameState;
    delete m_player0;
    delete m_player1;
}

String^ ManagedGameState::GetPlayerAhead()
{
    const Player* winner = m_pGameState->player_ahead();
    return gcnew String(winner ? winner->get_side_name() : "Neither player");
}

GameMove ManagedGameState::MoveFromString(String^ move_string)
{
    IntPtr move_chars = Marshal::StringToHGlobalAnsi(move_string);
    GameMove result = m_pGameState->read_move((char*)move_chars.ToPointer());
    Marshal::FreeHGlobal(move_chars);
    return result;
}

String^ ManagedGameState::MoveToString(GameMove move)
{
    char move_string[1000];
    m_pGameState->write_move(move, sizeof move_string, move_string);
    return gcnew String(move_string);
}

String^ ManagedGameState::GetOutputText()
{
    EnterCriticalSection(m_protect_output_buffer);
    String^ output_string = gcnew String(m_unmanaged_output_buffer);
    m_unmanaged_output_buffer[0] = '\0';
    LeaveCriticalSection(m_protect_output_buffer);

    return output_string;
}

Value ManagedGameState::AnalyzePosition(int target_depth, int max_analysis_time, GameMove% ret_move)
{
    GameMove move;
    Value val = m_pGameState->analyze(target_depth, max_analysis_time, &move);
    ret_move = move;
    return val;
}

String^ ManagedGameState::GetScoreSheet(bool include_moves)
{
    char output[1000];
    m_pGameState->display_score_sheet(include_moves, sizeof output, output);
    return gcnew String(output);
}

ManagedGameState^ ManagedGameList::CreateGame(int index)
{
    GameState* pGameState = g_game_list[index]->create_game();
    Player* player0 = g_game_list[index]->create_player(false, 0);
    Player* player1 = g_game_list[index]->create_player(false, 1);
    pGameState->set_player(0, player0);
    pGameState->set_player(1, player1);
    return gcnew ManagedGameState(pGameState, player0, player1);
}