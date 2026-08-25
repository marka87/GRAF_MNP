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
using System.Globalization;
using System.Text.Json;
using System.Collections.Generic;

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
        private string _currentDeviceState = string.Empty;
        private const int LiveLogMaxLines = 5;
        private readonly Queue<string> _liveLogLines = new Queue<string>(LiveLogMaxLines);
        private static readonly string PidProfilePath = System.IO.Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "MnpControl", "pid_z_profile.json");

        private sealed class PidProfile
        {
            public string Kp { get; set; } = "";
            public string Ki { get; set; } = "";
            public string Kd { get; set; } = "";
        }

        private sealed class PidPreset
        {
            public string Name { get; init; } = "";
            public string Kp { get; init; } = "";
            public string Ki { get; init; } = "";
            public string Kd { get; init; } = "";
        }

        private static readonly PidPreset[] PidPresets =
        {
            new PidPreset { Name = "Stabil (Empfohlen)", Kp = "0.0030", Ki = "0.000001", Kd = "0.018000" },
            new PidPreset { Name = "Schneller", Kp = "0.0035", Ki = "0.000001", Kd = "0.016000" },
            new PidPreset { Name = "Feiner", Kp = "0.0025", Ki = "0.000001", Kd = "0.020000" }
        };

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
        _continue = true;
        UpdateButtonStates(string.Empty); // all disabled until connected + state received
        InitializePidPresets();
        LoadPidProfileFromDisk();

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
        SendCommand("P?");
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
            if (sSplit.Length >= 5)
            {
                if (_isLoggingSession)
                    _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");

                TxtNadelOben.Text = sSplit[0] == "1" ? "Unten" : "Oben";
                TxtDrucksensor.Text = sSplit[1];
                TxtAaxisPos.Text = sSplit[2];
                TxtZaxisPosIst.Text = sSplit[3];
                TxtZaxisPosSoll.Text = sSplit[4];
                TxtZDac.Text = sSplit.Length >= 6 ? sSplit[5] : string.Empty;
                return;
            }

            if (msg.StartsWith("PIDZ:", StringComparison.Ordinal))
            {
                string payload = msg.Substring(5);
                if (TryParsePidPayload(payload, out string kp, out string ki, out string kd))
                {
                    TxtPidKpCurrent.Text = kp;
                    TxtPidKiCurrent.Text = ki;
                    TxtPidKdCurrent.Text = kd;
                    TxtStatus.Text = "PID gelesen";
                    AppendLiveLog("RX PID gelesen");
                }
                return;
            }

            if (msg.StartsWith("PIDZ_SET:", StringComparison.Ordinal))
            {
                string payload = msg.Substring(9);
                if (TryParsePidPayload(payload, out string kp, out string ki, out string kd))
                {
                    TxtPidKpCurrent.Text = kp;
                    TxtPidKiCurrent.Text = ki;
                    TxtPidKdCurrent.Text = kd;
                    TxtStatus.Text = "PID gesetzt";
                    AppendLiveLog("RX PID gesetzt");
                }
                return;
            }

            if (msg.StartsWith("PIDZ_ERR:", StringComparison.Ordinal) || msg.StartsWith("Z_NEUTRAL:", StringComparison.Ordinal))
            {
                TxtStatus.Text = msg;
                AppendLiveLog("RX " + msg);
                return;
            }

            if (msg.StartsWith("_STATUS_"))
            {
                if (_isLoggingSession && msg != _lastStatusMessage)
                    _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");

                _lastStatusMessage = msg;
                string state = msg.Replace("_STATUS_", "").Trim();
                _currentDeviceState = state;
                TxtStatus.Text = GetStateFriendlyName(state);
                AppendLiveLog("STATE " + state);
                UpdateButtonStates(state);
                return;
            }

            TxtStatus.Text = msg;
            AppendLiveLog("RX " + msg);
        }

        private void InitializePidPresets()
        {
            CmbPidPreset.ItemsSource = PidPresets;
            CmbPidPreset.DisplayMemberPath = nameof(PidPreset.Name);
            CmbPidPreset.SelectedIndex = 0;
        }

        private void AppendLiveLog(string line)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;

            if (_liveLogLines.Count >= LiveLogMaxLines)
                _liveLogLines.Dequeue();

            _liveLogLines.Enqueue($"{DateTime.Now:HH:mm:ss} {line}");
            TxtLiveLog.Text = string.Join(Environment.NewLine, _liveLogLines);
            TxtLiveLog.ScrollToEnd();
        }

        private static string GetStateFriendlyName(string state) => state switch
        {
            "IDLE_START"          => "Warte auf Referenzlauf",
            "EXEC_REFERENCE_RUN"  => "Referenzlauf läuft...",
            "TEST_START"          => "Bereit — Test wählen",
            "TEST_RUN"            => "Test läuft",
            "STOP"                => "Test abgebrochen",
            "COMPLETED"           => "Test abgeschlossen ✓",
            "FEHLER"              => "⚠ FEHLER",
            _                     => state
        };

        private void UpdateButtonStates(string state)
        {
            bool isTestStart  = state == "TEST_START";
            bool isIdle       = state == "IDLE_START";
            bool isRunning    = state == "TEST_RUN";
            bool canRef       = isIdle || isTestStart || state == "COMPLETED" || state == "STOP" || state == "FEHLER";
            bool isConnected  = _devConnection != null && _devConnection.IsOpen;

            BtnStartReferenceRun.IsEnabled = canRef;
            BtnStartDemo.IsEnabled         = isTestStart;
            BtnStartShort.IsEnabled        = isTestStart;
            BtnStartLong.IsEnabled         = isTestStart;
            BtnTestStart.IsEnabled         = isIdle || state == "COMPLETED" || state == "STOP" || state == "FEHLER";
            BtnStop.IsEnabled              = isRunning;
            BtnUp.IsEnabled                = isTestStart;
            BtnDown.IsEnabled              = isTestStart;
            BtnSetZPos.IsEnabled           = isTestStart;
            BtnPidRead.IsEnabled           = isConnected;
            BtnPidApply.IsEnabled          = isConnected;
            BtnPidNeutral.IsEnabled        = isConnected;
            BtnPidSave.IsEnabled           = true;
            BtnPidLoad.IsEnabled           = true;
        }

        private static bool TryParsePidPayload(string payload, out string kp, out string ki, out string kd)
        {
            kp = string.Empty;
            ki = string.Empty;
            kd = string.Empty;
            string[] parts = payload.Split(';', StringSplitOptions.TrimEntries);
            if (parts.Length != 3) return false;
            kp = parts[0];
            ki = parts[1];
            kd = parts[2];
            return true;
        }

        private bool TryReadPidInput(out float kp, out float ki, out float kd)
        {
            kp = 0.0f;
            ki = 0.0f;
            kd = 0.0f;
            bool ok =
                float.TryParse(TxtPidKpNew.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out kp) &&
                float.TryParse(TxtPidKiNew.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out ki) &&
                float.TryParse(TxtPidKdNew.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out kd);
            return ok && kp > 0.0f && ki > 0.0f && kd >= 0.0f;
        }

        private void LoadPidProfileFromDisk()
        {
            try
            {
                if (!File.Exists(PidProfilePath)) return;
                string json = File.ReadAllText(PidProfilePath);
                PidProfile? profile = JsonSerializer.Deserialize<PidProfile>(json);
                if (profile == null) return;
                TxtPidKpNew.Text = profile.Kp;
                TxtPidKiNew.Text = profile.Ki;
                TxtPidKdNew.Text = profile.Kd;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"PID profile load error: {ex.Message}");
            }
        }

        private void SavePidProfileToDisk()
        {
            string? dir = System.IO.Path.GetDirectoryName(PidProfilePath);
            if (!string.IsNullOrWhiteSpace(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }
            PidProfile profile = new PidProfile
            {
                Kp = TxtPidKpNew.Text.Trim(),
                Ki = TxtPidKiNew.Text.Trim(),
                Kd = TxtPidKdNew.Text.Trim()
            };
            string json = JsonSerializer.Serialize(profile, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(PidProfilePath, json);
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

        _devConnection.Write(command + "\n");
        AppendLiveLog("TX " + command);
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

        private void BtnSetZPos_Click(object sender, RoutedEventArgs e)
        {
            if (uint.TryParse(TxtZPos.Text.Trim(), out uint val))
            {
                SendCommand($"Z{val}");
            }
            else
            {
                MessageBox.Show("Ungültiger Wert. Bitte eine positive ganze Zahl eingeben.",
                    "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnPidRead_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("P?");
        }

        private void BtnPidApply_Click(object sender, RoutedEventArgs e)
        {
            if (!TryReadPidInput(out float kp, out float ki, out float kd))
            {
                MessageBox.Show("Ungültige PID-Werte. Format z.B. KP 0.003, KI 0.000001, KD 0.018",
                    "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string cmd = string.Format(CultureInfo.InvariantCulture, "P={0},{1},{2}", kp, ki, kd);
            SendCommand(cmd);
        }

        private void BtnPidSave_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                SavePidProfileToDisk();
                TxtStatus.Text = "PID-Profil gespeichert";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Speichern fehlgeschlagen: {ex.Message}",
                    "Dateifehler", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnPidLoad_Click(object sender, RoutedEventArgs e)
        {
            LoadPidProfileFromDisk();
            TxtStatus.Text = "PID-Profil geladen";
        }

        private void BtnPidNeutral_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("N");
        }

        private void BtnPidPresetApply_Click(object sender, RoutedEventArgs e)
        {
            if (CmbPidPreset.SelectedItem is not PidPreset preset)
            {
                return;
            }

            TxtPidKpNew.Text = preset.Kp;
            TxtPidKiNew.Text = preset.Ki;
            TxtPidKdNew.Text = preset.Kd;
            TxtStatus.Text = $"Preset geladen: {preset.Name}";
            AppendLiveLog("Preset " + preset.Name);
        }
    }
}
