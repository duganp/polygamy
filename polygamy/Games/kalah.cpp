// kalah.cpp

#include "shared.h"  // Precompiled header; obligatory
#include "kalah.h"   // Our public interface


// Game registration stuff

GameState* KalahGameState::creator() {return new KalahGameState;}

static int kalah_registered =
    register_game
    (
        "Kalah",
        KalahGameState::creator
    );


void KalahGameState::reset()
{
    GameState::reset();

    memset(m_states, 0, sizeof m_states);
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        m_states[0][n] = KALAH_SEEDS;
        m_states[0][KALAH_PITS + 1 + n] = KALAH_SEEDS;
    }

    memset(m_move_history, 0, sizeof m_move_history);
    m_forced_pass = false;
}


GameMove* KalahGameState::get_possible_moves() const
{
    GameMove* possible_moves = new GameMove[KALAH_PITS+1];
    GameMove* current_move = possible_moves;

    if (!m_forced_pass)
    {
        int first_pit = (player_up() == 0) ? 0 : KALAH_PITS+1;
        for (int n = 0; n < KALAH_PITS; ++n)
        {
            if (m_states[move_counter()][first_pit + n] != 0)
            {
                *current_move++ = n+1;
            }
        }
    }

    *current_move = INVALID_MOVE;  // Terminate move list for caller convenience

    return possible_moves;
}


GameMove KalahGameState::read_move(const char* move_string) const
{
    if (m_forced_pass)
    {
        output("Can only pass...\n");
        return PASSING_MOVE;
    }

    // Hacky check to see if this move came from the GUI frontend.
    // The user could conceivably type a move in this exact form and
    // have it misinterpreted as a GUI frontend move, but never mind.
    if (move_string[0] >= 'A' && move_string[0] < 'A' + KALAH_PITS + 2 &&
        move_string[1] >= '1' && move_string[1] <= '3' &&
        move_string[2] >= 'A' && move_string[2] < 'A' + KALAH_PITS + 2 &&
        move_string[3] >= '1' && move_string[3] <= '3' &&
        move_string[4] == '\0')
    {
        char c = move_string[0] - 1;  // The intended column is one step to the left
        return (c >= 'A' && c < 'A' + KALAH_PITS) ? GameMove(c - 'A' + 1) : INVALID_MOVE;
    }

    // Otherwise, this must be a human-entered move
    while (int c = toupper(*move_string++))
    {
        if (c >= 'A' && c < 'A' + KALAH_PITS) return GameMove(c - 'A' + 1);
    }

    return INVALID_MOVE;
}


void KalahGameState::write_move(GameMove move, int move_string_size,
                                __in_ecount(move_string_size) char* move_string) const
{
    if (move == PASSING_MOVE)
    {
        sprintf_s(move_string, move_string_size, "Pass");
    }
    else
    {
        sprintf_s(move_string, move_string_size, "%c", 'A' + move - 1);
    }
}


bool KalahGameState::valid_move(GameMove move)
{
    if (move == PASSING_MOVE)
    {
        return m_forced_pass;  // Passing is only valid if forced
    }
    else
    {
        return (move > 0 && move <= KALAH_PITS) &&
               (player_up() == 0 ? (m_states[move_counter()][move-1] != 0)
                                 : (m_states[move_counter()][move+KALAH_PITS] != 0));
    }
}


Result KalahGameState::apply_move(GameMove move)
{
    ASSERT(valid_move(move));
    ASSERT(!m_forced_pass);

    m_move_history[move_counter()] = move;
    advance_move_counter();
    int (&state)[2 * KALAH_PITS + 2] = m_states[move_counter()];
    memcpy(state, m_states[move_counter()-1], sizeof state);

    int player_store = (player_up() == 0) ? KALAH_PITS : 2 * KALAH_PITS + 1;
    int opponent_store = (player_up() == 1) ? KALAH_PITS : 2 * KALAH_PITS + 1;
    int pit_being_emptied = (player_up() == 0) ? (move - 1) : (move + KALAH_PITS);
    int seeds_remaining = state[pit_being_emptied];
    state[pit_being_emptied] = 0;
    int current_pit = pit_being_emptied;

    while (seeds_remaining--)
    {
        current_pit = (current_pit + 1) % (2 * KALAH_PITS + 2);
        if (current_pit == opponent_store)
        {
            current_pit = (current_pit + 1) % (2 * KALAH_PITS + 2);
        }
        ++state[current_pit];
    }

    if (current_pit == player_store)
    {
        m_forced_pass = true;
    }
    else if ((player_up() == 0) == (current_pit < KALAH_PITS))
    {
        // Landed on own side; see if there is a capture
        if (state[current_pit] == 1)
        {
            int opposing_pit = 2 * KALAH_PITS - current_pit;
            if (state[opposing_pit] != 0)
            {
                state[player_store] += state[opposing_pit] + 1;
                state[opposing_pit] = 0;
                state[current_pit] = 0;
            }
        }
    }

    // See if the game is over.  FIXME: ugly and slow.  Keep running totals?
    // Note opponent can only be wiped out after a capture move, so that
    // check doesn't need to be here.
    int player_0_total = 0, player_1_total = 0;
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        player_0_total += state[n];
        player_1_total += state[n+KALAH_PITS+1];
    }
    if (player_0_total == 0 || player_1_total == 0)
    {
        for (int n = 0; n < KALAH_PITS; ++n)
        {
            state[n] = state[n+KALAH_PITS+1] = 0;
        }
        state[KALAH_PITS] += player_0_total;
        state[2*KALAH_PITS+1] += player_1_total;
    }

    switch_player_up();

    return Result::OK;
}


Result KalahGameState::apply_passing_move()
{
    if (!m_forced_pass)
    {
        return Result::Fail;
    }

    m_move_history[move_counter()] = PASSING_MOVE;
    advance_move_counter();
    memcpy(m_states[move_counter()], m_states[move_counter()-1], sizeof *m_states);
    m_forced_pass = false;
    switch_player_up();

    return Result::OK;
}


void KalahGameState::undo_last_move()
{
    ASSERT(move_counter() > 0);
    retreat_move_counter();
    switch_player_up();
    m_forced_pass = (m_move_history[move_counter()] == PASSING_MOVE);
}


void KalahGameState::display(size_t output_size, __out_ecount(output_size) char* output) const
{
    #define WRITE(fmt, arg) StringCchPrintfExA(output, output_size, &output, &output_size, 0, fmt, arg);

    // First row
    WRITE(" É", 0);
    for (int n = 0; n < KALAH_PITS+2; ++n)
    {
        WRITE("ÍÍÍÍ%c", n == KALAH_PITS+1 ? '»' : 'Ñ');
    }
    WRITE("\n º   ", 0);
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        WRITE(" ³ %2d", m_states[move_counter()][2*KALAH_PITS-n]);
    }

    // Second row
    WRITE(" ³    º\n Ç", 0);
    for (int n = 0; n < KALAH_PITS+2; ++n)
    {
        WRITE("ÄÄÄÄ%c", n == KALAH_PITS+1 ? '¶' : 'Á');
    }
    WRITE("\n º %2d", m_states[move_counter()][2*KALAH_PITS+1]);
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        WRITE("     ", 0);
    }

    // Third row
    WRITE("   %2d º\n Ç", m_states[move_counter()][KALAH_PITS]);
    for (int n = 0; n < KALAH_PITS+2; ++n)
    {
        WRITE("ÄÄÄÄ%c", n == KALAH_PITS+1 ? '¶' : 'Â');
    }
    WRITE("\n º   ", 0);
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        WRITE(" ³ %2d", m_states[move_counter()][n]);
    }

    // Wrap up
    WRITE(" ³    º\n È", 0);
    for (int n = 0; n < KALAH_PITS+2; ++n)
    {
        WRITE("ÍÍÍÍ%c", n == KALAH_PITS+1 ? '¼' : 'Ï');
    }
    WRITE(" (move %u)\n     ", move_counter());
    for (int n = 0; n < KALAH_PITS; ++n)
    {
        WRITE("    %c", 'A' + n);
    }
    WRITE("\n\n", 0);
}


void KalahGameState::display_score_sheet(bool include_moves, size_t output_size, __out_ecount(output_size) char* output) const
{
    Value value = position_val();
    if (value == 0)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "Tie");
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                           "%s won by %d", get_player_name(value < 0), abs(value));
    }

    if (include_moves)
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, " in %d moves:\n", move_counter());
        for (int n = 0; n < move_counter(); ++n)
        {
            char move_string[MAX_MOVE_STRING_SIZE];
            write_move(m_move_history[n], sizeof move_string, move_string);
            StringCchPrintfExA(output, output_size, &output, &output_size, 0,
                               "\t%d. %s %s\n", n + 1, get_player_name(n%2), move_string);
        }

        StringCchPrintfExA(output, output_size, &output, &output_size, 0, "\nFinal board state:\n");
        display(output_size, output);
    }
    else
    {
        StringCchPrintfExA(output, output_size, &output, &output_size, 0, ".\n");
    }
}


const char* KalahGameState::get_cell_state_image_name(int state) const
{
    C_ASSERT(KALAH_CELL_TYPE_IMAGES == 23);

    if (state == 0)
    {
        return "KalahBlank";
    }
    else if (state == 1)
    {
        return "KalahPitEmpty";
    }
    else if (state <= 21)
    {
        static char name[20];
        sprintf_s(name, sizeof name, "KalahPit%dSeeds", state - 1);
        return name;
    }
    else
    {
        return "KalahPitManySeeds";
    }
}


int KalahGameState::get_cell_state(int row, int column) const
{
    if (row == 1)
    {
        if (column == 0)
        {
            int seeds = m_states[move_counter()][2 * KALAH_PITS + 1];
            return seeds <= 20 ? seeds + 1 : 22;  // FIXME: Magic numbers everywhere
        }
        else if (column == KALAH_PITS+1)
        {
            int seeds = m_states[move_counter()][KALAH_PITS];
            return seeds <= 20 ? seeds + 1 : 22;
        }
        else return 0;
    }
    else if (column == 0 || column == KALAH_PITS+1)
    {
        return 0;
    }
    else if (row == 0)
    {
        int seeds = m_states[move_counter()][2*KALAH_PITS+1 - column];
        return seeds <= 20 ? seeds + 1 : 22;
    }
    else // row == 2
    {
        int seeds = m_states[move_counter()][column - 1];
        return seeds <= 20 ? seeds + 1 : 22;
    }
}