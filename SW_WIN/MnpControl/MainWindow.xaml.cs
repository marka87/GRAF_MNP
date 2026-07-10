using System.Data;
using System.Diagnostics;
using System.IO.Ports;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Threading;
using System.IO;

namespace MnpControl
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
    private const string PreferredPortName = "COM9";
    private static SerialPort? _devConnection;

        private static bool _continue = true;
        private StreamWriter _currentLogFile; // Logfile für den aktuellen Ablauf
        private StreamWriter _filteredLogFile; // Gefilterte Log-Datei

        private StringBuilder _serialBuffer = new StringBuilder(); // Buffer for incoming serial data
        private bool _isLoggingSession = false;
        private DateTime _loggingStartTime = DateTime.MinValue;
        private const int LoggingTimeoutSeconds = 120;
        private string _lastStatusMessage = string.Empty;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
        _continue = true;

        if (!TryOpenDeviceConnection())
        {
            MessageBox.Show("No accessible serial port found.\nPlease close other apps using the port and restart.",
                "Serial connection failed",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            return;
        }

        // Subscribe to DataReceived event for instant data processing
        _devConnection.DataReceived += DevConnection_DataReceived;
        }

    private bool TryOpenDeviceConnection()
    {
        while (true)
        {
            string[] availablePorts = SerialPort.GetPortNames();
            if (availablePorts.Length == 0)
            {
                return false;
            }

            string? selectedPort = ShowPortSelectionDialog(availablePorts);
            if (string.IsNullOrWhiteSpace(selectedPort))
            {
                return false;
            }

            if (TryOpenSelectedPort(selectedPort))
            {
                TxtStatus.Text = $"Connected: {selectedPort}";
                return true;
            }

            MessageBox.Show(
                $"Port {selectedPort} is not accessible.\nChoose another port.",
                "Serial connection failed",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
        }
    }

    private bool TryOpenSelectedPort(string portName)
    {
        SerialPort connection = new SerialPort(portName, 115200, Parity.None, 8, StopBits.One)
        {
            ReadTimeout = 100,
            WriteTimeout = 100
        };

        try
        {
            connection.Open();
            _devConnection = connection;
            return true;
        }
        catch (UnauthorizedAccessException)
        {
            connection.Dispose();
            return false;
        }
        catch (IOException)
        {
            connection.Dispose();
            return false;
        }
    }

    private string? ShowPortSelectionDialog(string[] ports)
    {
        PortSelectionWindow picker = new PortSelectionWindow(ports, PreferredPortName)
        {
            Owner = this
        };

        bool? result = picker.ShowDialog();
        if (result == true)
        {
            return picker.SelectedPort;
        }

        return null;
    }

    private sealed class PortSelectionWindow : Window
    {
        private readonly ComboBox _portComboBox;

        public string? SelectedPort { get; private set; }

        public PortSelectionWindow(string[] ports, string preferredPort)
        {
            Title = "Select Serial Port";
            Width = 320;
            Height = 160;
            ResizeMode = ResizeMode.NoResize;
            WindowStartupLocation = WindowStartupLocation.CenterOwner;

            Grid grid = new Grid { Margin = new Thickness(12) };
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            TextBlock label = new TextBlock
            {
                Text = "Available ports:",
                Margin = new Thickness(0, 0, 0, 8)
            };
            Grid.SetRow(label, 0);

            _portComboBox = new ComboBox
            {
                MinWidth = 260,
                ItemsSource = ports,
                Margin = new Thickness(0, 0, 0, 12)
            };

            int preferredIndex = -1;
            for (int i = 0; i < ports.Length; i++)
            {
                if (string.Equals(ports[i], preferredPort, StringComparison.OrdinalIgnoreCase))
                {
                    preferredIndex = i;
                    break;
                }
            }

            _portComboBox.SelectedIndex = preferredIndex >= 0 ? preferredIndex : 0;
            Grid.SetRow(_portComboBox, 1);

            StackPanel buttons = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Right
            };

            Button okButton = new Button
            {
                Content = "Connect",
                Width = 90,
                Margin = new Thickness(0, 0, 8, 0)
            };

            okButton.Click += (_, _) =>
            {
                SelectedPort = _portComboBox.SelectedItem as string;
                DialogResult = !string.IsNullOrWhiteSpace(SelectedPort);
            };

            Button cancelButton = new Button
            {
                Content = "Cancel",
                Width = 90
            };

            cancelButton.Click += (_, _) => DialogResult = false;

            buttons.Children.Add(okButton);
            buttons.Children.Add(cancelButton);
            Grid.SetRow(buttons, 2);

            grid.Children.Add(label);
            grid.Children.Add(_portComboBox);
            grid.Children.Add(buttons);

            Content = grid;
        }
    }

        private void StartNewLogFile()
        {
            string logDirectory = @"C:\LOG\";
            if (!Directory.Exists(logDirectory))
            {
                Directory.CreateDirectory(logDirectory); // Stelle sicher, dass der Ordner existiert
            }

            string logFilePath = System.IO.Path.Combine(logDirectory, $"Log_{DateTime.Now:yyyyMMdd_HHmmss}.txt");
            _currentLogFile = new StreamWriter(logFilePath, true, Encoding.UTF8);
            _currentLogFile.AutoFlush = true; // Automatisches Schreiben auf die Festplatte
        }

        private void CloseLogFile()
        {
            _currentLogFile?.Close();
            _currentLogFile = null; // Logfile-Objekt zurücksetzen
        }

        private void UpdateStatus(string msg)
        {
            Debug.WriteLine(msg);

            // Check if logging session has timed out
            if (_isLoggingSession && (DateTime.Now - _loggingStartTime).TotalSeconds > LoggingTimeoutSeconds)
            {
                _isLoggingSession = false;
                _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - [SESSION ENDED - TIMEOUT]");
                CloseLogFile();
            }

            string[] sSplit = msg.Split(";", StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (sSplit.Length == 5)
            {
                // Log sensor data if logging is active
                if (_isLoggingSession)
                {
                    _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");
                }

                // Always update UI for visual feedback
                if (sSplit[0] == "1")
                {
                    TxtNadelOben.Text = "Unten";
                }
                else
                {
                    TxtNadelOben.Text = "Oben";
                }

                TxtDrucksensor.Text = sSplit[1];
                TxtAaxisPos.Text = sSplit[2];
                TxtZaxisPosIst.Text = sSplit[3];
                TxtZaxisPosSoll.Text = sSplit[4];

                return;
            }

            if (msg.StartsWith("_STATUS_"))
            {
                // Log status only if logging is active and different from last
                if (_isLoggingSession && msg != _lastStatusMessage)
                {
                    _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");
                }

                // Always update status display
                _lastStatusMessage = msg;
                msg = msg.Replace("_STATUS_", "");
                TxtStatus.Text = msg.Trim();
                return;
            }
        }

        private void DevConnection_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                // Read all available data from the serial port
                while (_devConnection != null && _devConnection.BytesToRead > 0)
                {
                    string data = _devConnection.ReadExisting();
                    _serialBuffer.Append(data);

                    // Process complete lines (terminated by newline)
                    string bufferContent = _serialBuffer.ToString();
                    int newlineIndex = bufferContent.IndexOf('\n');

                    while (newlineIndex >= 0)
                    {
                        string message = bufferContent.Substring(0, newlineIndex).TrimEnd('\r');
                        if (!string.IsNullOrEmpty(message))
                        {
                            // Update UI on the main thread asynchronously (non-blocking)
                            Application.Current.Dispatcher.BeginInvoke(new Action(() => { UpdateStatus(message); }), System.Windows.Threading.DispatcherPriority.Normal);
                        }

                        // Remove processed message from buffer
                        _serialBuffer.Remove(0, newlineIndex + 1);
                        bufferContent = _serialBuffer.ToString();
                        newlineIndex = bufferContent.IndexOf('\n');
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Serial data error: {ex.Message}");
            }
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            _continue = false;

            if (_devConnection != null)
            {
                // Unsubscribe from event
                _devConnection.DataReceived -= DevConnection_DataReceived;

                if (_devConnection.IsOpen)
                {
                    _devConnection.Close();
                }

                _devConnection.Dispose();
                _devConnection = null;
            }

            CloseLogFile(); // Sicherstellen, dass das aktuelle Logfile geschlossen wird
        }

    private void SendCommand(string command)
    {
        if (_devConnection == null || !_devConnection.IsOpen)
        {
            MessageBox.Show("No open serial connection.", "Serial connection", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        _devConnection.Write(command);
    }

        private void BtnStartReferenceRun_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("s");
        }

        private void BtnStartDemoRun_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("1");
        }

        private void BtnStartShortRun_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("2");
        }

        private void BtnStartLongRun_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("3");
        }

        private void BtnReset_Click(object sender, RoutedEventArgs e)
        {
        SendCommand("r");
        }

        private void BtnUp_Click(object sender, RoutedEventArgs e)
        {
        SendCommand("+");
        }

        private void BtnDown_Click(object sender, RoutedEventArgs e)
        {
        SendCommand("-");
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("q");
        }

        private void BtnTestStart_click(object sender, RoutedEventArgs e)
        {
            SendCommand("e");
        }
    }
}
