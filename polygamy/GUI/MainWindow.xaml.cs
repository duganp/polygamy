using System;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Threading;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Imaging;

using NetWrapper;

namespace PolyGUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        const int m_default_game = 4;
        const int m_default_depth = 5;
        const int m_default_search_time = 3;

        ManagedGameList m_game_list;
        int m_current_game;
        ManagedGameState m_game_state;
        int m_rows;
        int m_columns;
        BitmapImage[] m_cell_state_bitmaps;
        Image[][] m_cell_images;
        AutoResetEvent m_wake_game_thread;
        ManualResetEvent m_kill_game_thread;
        int m_search_depth = m_default_depth;
        int m_search_time = m_default_search_time;
        bool m_waiting_for_computer_move = false;
        DispatcherTimer m_output_dispatcher;
        int m_last_row_clicked = -1;
        int m_last_column_clicked = -1;

        private void LogString(String str)
        {
            if (str != "")
            {
                OutputBox.Text += str;
                if (str.Last() != '\n') OutputBox.Text += "\n";
                OutputBox.PageDown();
            }
        }

        /// <summary>
        /// Summary documentation for MainWindow.
        /// </summary>
        /// <remarks>
        /// Remarks about MainWindow.
        /// </remarks>
        /// <param>
        /// Parameter 1.
        /// </param>
        /// <return>
        /// Nothing (top-level entry point).</return>
        public MainWindow()
        {
            InitializeComponent();

            m_game_list = new ManagedGameList();
            for (int n = 0; n < m_game_list.GetGameCount(); ++n)
            {
                GameSelection.Items.Add(m_game_list.GetGameName(n));
            }

            m_current_game = -1;
            GameSelection.SelectedIndex = m_default_game;  // This triggers "SwitchGame"

            // FIXME: Make sure everything is initialized, and freed if necessary
            DepthBox.Text = m_search_depth.ToString();

            // Set up a timer to display messages from the unmanaged engine
            m_output_dispatcher = new DispatcherTimer();
            m_output_dispatcher.Tick += new EventHandler(delegate(object s, EventArgs e) {LogString(m_game_state.GetOutputText());});
            m_output_dispatcher.Interval = TimeSpan.FromMilliseconds(100);
            m_output_dispatcher.Start();
            // FIXME: Need m_output_dispatcher.Stop() in a destructor/Dispose?

            // Kick off the thinking thread
            m_wake_game_thread = new AutoResetEvent(false);
            m_kill_game_thread = new ManualResetEvent(false);
            Thread thread = new Thread(new ThreadStart(GameThread));
            thread.Start();
        }

        private void AddRow(int height)
        {
            RowDefinition rowdef = new RowDefinition();
            rowdef.Height = new GridLength(height);
            GameBoardData.RowDefinitions.Add(rowdef);
        }

        private void AddColumn(int width)
        {
            ColumnDefinition coldef = new ColumnDefinition();
            coldef.Width = new GridLength(width);
            GameBoardData.ColumnDefinitions.Add(coldef);
        }

        private void SwitchGame(int game_identifier)
        {
            if (game_identifier != m_current_game)
            {
                m_current_game = game_identifier;
                m_game_state = m_game_list.CreateGame(m_current_game);
                m_game_state.ResetGame();  // FIXME: needed?

                // Destroy previous cell table, if any
                if (GameBoardData.Children.Count != 0)  // Shouldn't need these guards, but we do
                    GameBoardData.Children.RemoveRange(0, GameBoardData.Children.Count);
                if (GameBoardData.RowDefinitions.Count != 0)
                    GameBoardData.RowDefinitions.RemoveRange(0, GameBoardData.RowDefinitions.Count);
                if (GameBoardData.ColumnDefinitions.Count != 0)
                    GameBoardData.ColumnDefinitions.RemoveRange(0, GameBoardData.ColumnDefinitions.Count);

                // Load new game-specific board cell bitmaps
                LoadCellStateBitmaps();

                // Set up the game state display
                m_rows = m_game_state.GetRows();
                m_columns = m_game_state.GetColumns();

                int label_strips_width = 16;
                if (m_current_game == 4)  // FIXME: Need something like 'supress_row_numbers' (perhaps included in a game details struct along with m_rows, etc)
                {
                    label_strips_width = 4;
                }

                AddRow(label_strips_width);
                for (int row = 0; row < m_rows; ++row) AddRow(40);  // FIXME: AddRow(image.Height) doesn't work
                AddRow(label_strips_width);

                AddColumn(label_strips_width);
                for (int col = 0; col < m_columns; ++col) AddColumn(40);
                AddColumn(label_strips_width);

                if (m_current_game != 4)  // FIXME: Need something like 'supress_row_numbers' (perhaps included in a game details struct along with m_rows, etc)
                {
                    for (int row = 1; row <= m_rows; ++row)
                    {
                        TextBlock label1 = new TextBlock(), label2 = new TextBlock();
                        label1.Text = label2.Text = new String(new char[] {(char)('1' + m_rows - row)});
                        label1.HorizontalAlignment = label2.HorizontalAlignment = HorizontalAlignment.Center;
                        label1.VerticalAlignment = label2.VerticalAlignment = VerticalAlignment.Center;
                        label1.TextAlignment = label2.TextAlignment = TextAlignment.Center;
                        label1.Height = label2.Height = 15;
                        Grid.SetRow(label1, row); Grid.SetRow(label2, row);
                        Grid.SetColumn(label1, 0); Grid.SetColumn(label2, m_columns + 1);
                        GameBoardData.Children.Add(label1); GameBoardData.Children.Add(label2);
                    }
                    for (int col = 1; col <= m_columns; ++col)
                    {
                        TextBlock label1 = new TextBlock(), label2 = new TextBlock();
                        label1.Text = label2.Text = new String(new char[] {(char)('A' + col - 1)});
                        label1.HorizontalAlignment = label2.HorizontalAlignment = HorizontalAlignment.Center;
                        label1.VerticalAlignment = label2.VerticalAlignment = VerticalAlignment.Center;
                        label1.TextAlignment = label2.TextAlignment = TextAlignment.Center;
                        label1.Height = label2.Height = 14;
                        Grid.SetRow(label1, 0); Grid.SetRow(label2, m_rows + 1);
                        Grid.SetColumn(label1, col); Grid.SetColumn(label2, col);
                        GameBoardData.Children.Add(label1); GameBoardData.Children.Add(label2);
                    }
                }

                m_cell_images = new Image[m_rows][];
                for (int row = 0; row < m_rows; ++row)
                {
                    m_cell_images[row] = new Image[m_columns];
                    for (int col = 0; col < m_columns; ++col)
                    {
                        m_cell_images[row][col] = new Image();
                        m_cell_images[row][col].MouseDown += new MouseButtonEventHandler(BoardCellClicked);
                        m_cell_images[row][col].MouseUp += new MouseButtonEventHandler(BoardCellUnclicked);
                        Grid.SetRow(m_cell_images[row][col], row + 1);
                        Grid.SetColumn(m_cell_images[row][col], col + 1);
                        GameBoardData.Children.Add(m_cell_images[row][col]);
                    }
                }

                LogString("New " + m_game_list.GetGameName(m_current_game) + " game.");
                Title = "Polygamy: " + m_game_list.GetGameName(m_current_game);

                RefreshBoard();
            }
        }

        private void RefreshBoard()
        {
            int count = 0;
            foreach (UIElement element in GameBoardData.Children)
            {
                if (element is Image)
                {
                    int state = m_game_state.GetCellState(count / m_columns, count % m_columns);
                    (element as Image).Source = m_cell_state_bitmaps[state];
                    ++count;
                }
            }
            if (m_game_state.IsGameOver())
            {
                InputBoxLabel.Content = "Game over.";
            }
            else
            {
                InputBoxLabel.Content = m_game_state.GetPlayerToMove() + " move?";
                InputBox.Focus();
            }
        }

        private void InputTextChanged(object sender, TextChangedEventArgs e)
        {
            if (InputBox.Text.Contains('\n'))
            {
                String move_string = InputBox.Text;
                InputBox.Text = "";
                if (!m_waiting_for_computer_move && !m_game_state.IsGameOver())
                {
                    PerformHumanMove(move_string);
                }
            }
        }

        private void BoardCellClicked(object sender, MouseEventArgs e)
        {
            m_last_row_clicked = -1;
            m_last_column_clicked = -1;

            Image cell_image = sender as Image;
            for (int row = 0; row < m_rows; ++row)
            {
                for (int column = 0; column < m_columns; ++column)
                {
                    if (m_cell_images[row][column] == cell_image)
                    {
                        m_last_row_clicked = row;
                        m_last_column_clicked = column;
                        return;
                    }
                }
            }
        }

        private void BoardCellUnclicked(object sender, MouseEventArgs e)
        {
            if (m_last_row_clicked != -1)
            {
                int source_row = m_last_row_clicked;
                int source_column = m_last_column_clicked;
                m_last_row_clicked = m_last_column_clicked = -1;

                if (m_waiting_for_computer_move)
                {
                    LogString("Computer is still thinking.");
                }
                else if (m_game_state.IsGameOver())
                {
                    LogString("This game is over.");
                }
                else
                {
                    Image cell_image = sender as Image;
                    for (int row = 0; row < m_rows; ++row)
                    {
                        for (int column = 0; column < m_columns; ++column)
                        {
                            if (m_cell_images[row][column] == cell_image)
                            {
                                char[] move_chars = new char[] {(char)('A' + source_column), (char)('0' + m_rows - source_row),
                                                                (char)('A' + column), (char)('0' + m_rows - row)};
                                PerformHumanMove(new String(move_chars));
                                return;
                            }
                        }
                    }
                }
            }
        }

        private void PerformHumanMove(String move_string)
        {
            String side_to_move = m_game_state.GetPlayerToMove();
            // This method must be called before the move is actually performed
            // in order to get the correct player name.

            // Conver the move to internal representation
            int move = m_game_state.MoveFromString(move_string);

            // Convert the move back into a canonical textual representation (e.g. 'C' rather than 'D1D1' for Kalah)
            move_string = m_game_state.MoveToString(move);

            if (!m_game_state.IsMoveValid(move))
            {
                LogString("Invalid move '" + move_string + "'.");
            }
            else if (!m_game_state.PerformMove(move))
            {
                LogString("Illegal move '" + move_string + "'.");
            }
            else
            {
                LogString(side_to_move + " move: " + move_string);
                RefreshBoard();
                if (m_game_state.IsGameOver())
                {
                    LogString("Game over. " + m_game_state.GetPlayerAhead() + " is victorious.");
                }
                else
                {
                    InputBoxLabel.Content = "Thinking...";
                    m_waiting_for_computer_move = true;
                    m_wake_game_thread.Set();
                }
            }
        }

        private void MainWindowClosed(object sender, EventArgs e)
        {
            m_kill_game_thread.Set();
        }

        private void SearchDepthChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            m_search_depth = (int)SearchDepth.Value;

            // FIXME: DepthBox may still be null the first time this is called.  Clumsy, unsafe.
            if (DepthBox != null)
            {
                DepthBox.Text = m_search_depth.ToString();
            }
        }

        private void LoadCellStateBitmaps()
        {
            int count = m_game_state.GetCellStatesCount();
            m_cell_state_bitmaps = new BitmapImage[count];

            for (int n = 0; n < count; ++n)
            {
                m_cell_state_bitmaps[n] = new BitmapImage();
                m_cell_state_bitmaps[n].BeginInit();
                m_cell_state_bitmaps[n].UriSource = new Uri(@"/PolyGUI;component/Images/" +
                                                            m_game_state.GetCellStateImageName(n) +
                                                            ".bmp", UriKind.RelativeOrAbsolute);
                m_cell_state_bitmaps[n].EndInit();
            }
        }

        private void GameRestarted(object sender, RoutedEventArgs e)
        {
            if (m_waiting_for_computer_move)
            {
                LogString("Computer is still thinking.");
            }
            else
            {
                m_game_state.ResetGame();
                RefreshBoard();
            }
        }

        private void GameSelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (m_waiting_for_computer_move)
            {
                LogString("Computer is still thinking.");
            }
            else
            {
                SwitchGame(GameSelection.SelectedIndex);
            }
        }

        private void SuggestMove(object sender, RoutedEventArgs e)
        {
            if (m_waiting_for_computer_move)
            {
                LogString("Computer is still thinking.");
            }
            else if (m_game_state.IsGameOver())
            {
                LogString("This game is over.");
            }
            else
            {
                LogString("Getting a computer suggestion for " + m_game_state.GetPlayerToMove() + "...");
                InputBoxLabel.Content = "Thinking...";
                m_waiting_for_computer_move = true;
                m_wake_game_thread.Set();
            }
        }

        private void GameThread()
        {
            WaitHandle[] waitHandles = new WaitHandle[] {m_wake_game_thread, m_kill_game_thread};
            for (;;)
            {
                int index = WaitHandle.WaitAny(waitHandles);
                if (index == 1) return;

                int move = 0;
                int value = m_game_state.AnalyzePosition(m_search_depth, m_search_time, ref move);
                String message = m_game_state.GetPlayerToMove() + " move: " + m_game_state.MoveToString(move) + " (estimated value " + value + ")";
                m_game_state.PerformMove(move);
                if (m_game_state.IsGameOver())
                {
                    message += "\nGame over. " + m_game_state.GetPlayerAhead() + " is victorious.";
                }
                Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(delegate()
                {
                    LogString(message);
                    RefreshBoard();
                }));
                m_waiting_for_computer_move = false;
            }
        }

        private void CopyLogWindow(object sender, RoutedEventArgs e)
        {
            // To do
        }

        private void ClearLogWindow(object sender, RoutedEventArgs e)
        {
            OutputBox.Text = "";
        }
    }
}
