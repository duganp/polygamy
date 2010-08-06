// NetWrapper.cpp
//
// REFERENCES ON MANAGED C++ WRAPPERS:
//
// http://stackoverflow.com/questions/425752/managed-c-wrappers-for-legacy-c-libraries
//

#include "NetWrapper.h"

using namespace System::Runtime::InteropServices;
using namespace NetWrapper;


ManagedGameState::ManagedGameState(GameState* pGameState)
{
    m_pGameState = pGameState;

    m_unmanaged_output_buffer = new char[1024];
    m_unmanaged_output_buffer[0] = '\0';
    m_protect_output_buffer = new CRITICAL_SECTION;
    InitializeCriticalSection(m_protect_output_buffer);
    m_pGameState->set_output_buffer(m_unmanaged_output_buffer, m_protect_output_buffer);
}

ManagedGameState::~ManagedGameState()
{
    delete m_pGameState;
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
    return gcnew ManagedGameState(g_game_list[index]->create_game());
}
