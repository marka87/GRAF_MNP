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
        private string _lastLiveLogLine = string.Empty;

        private StringBuilder _serialBuffer = new StringBuilder(); // Buffer for incoming serial data
        private bool _isLoggingSession = false;
        private DateTime _loggingStartTime = DateTime.MinValue;
        private const int LoggingTimeoutSeconds = 120;
        private string _lastStatusMessage = string.Empty;
        private string _currentDeviceState = string.Empty;
        private uint? _zStepLowerLimit;
        private uint? _zStepUpperLimit;
        private readonly HybridTuningConfig _hybridConfig = new HybridTuningConfig
        {
            PosKp = 0.12f,
            PosKi = 0.0f,
            PosKd = 0.0f,
            VelKp = 0.035f,
            VelKi = 0.0003f,
            VelKd = 0.0005f
        };
        private const int LiveLogMaxLines = 5;
        private readonly Queue<string> _liveLogLines = new Queue<string>(LiveLogMaxLines);
        private readonly Queue<string> _testSummaryLines = new Queue<string>(8);
        private static readonly string PidPresetPath = System.IO.Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "MnpControl", "pid_z_presets.json");

        private sealed class PidPreset
        {
            public string Name { get; set; } = "";
            public string Kp { get; set; } = "";
            public string Ki { get; set; } = "";
            public string Kd { get; set; } = "";
            public bool IsBuiltIn { get; set; }
        }

        private static readonly PidPreset StablePreset = new PidPreset
        {
            Name = "Stabil (Empfohlen)",
            Kp = "0.003",
            Ki = "0.000001",
            Kd = "0.018",
            IsBuiltIn = true
        };
        private readonly List<PidPreset> _pidPresets = new List<PidPreset>();

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
        _continue = true;
        UpdateButtonStates(string.Empty); // all disabled until connected + state received
        InitializePidPresets();

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
        SendCommand("PP?");
        SendCommand("L?");
        SendCommand("PV?");
        SendCommand("V?");
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

            if (msg == "Modus?")
            {
                return;
            }

            if (msg.StartsWith("Zeit:", StringComparison.Ordinal))
            {
                return;
            }

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

            if (msg.StartsWith("PIDZP:", StringComparison.Ordinal) || msg.StartsWith("PIDZP_SET:", StringComparison.Ordinal))
            {
                string payload = msg[(msg.StartsWith("PIDZP_SET:", StringComparison.Ordinal) ? 10 : 6)..].Trim();
                if (TryParsePidPayload(payload, out string kp, out string ki, out string kd))
                {
                    TxtPidKpCurrent.Text = kp;
                    TxtPidKiCurrent.Text = ki;
                    TxtPidKdCurrent.Text = kd;
                    if (float.TryParse(kp, NumberStyles.Float, CultureInfo.InvariantCulture, out float pkp)
                        && float.TryParse(ki, NumberStyles.Float, CultureInfo.InvariantCulture, out float pki)
                        && float.TryParse(kd, NumberStyles.Float, CultureInfo.InvariantCulture, out float pkd))
                    {
                        _hybridConfig.PosKp = pkp;
                        _hybridConfig.PosKi = pki;
                        _hybridConfig.PosKd = pkd;
                    }
                    TxtStatus.Text = "Position-PID gelesen";
                    AppendLiveLog("RX Position PID");
                }
                else
                {
                    TxtStatus.Text = "PID-Formatfehler";
                    AppendLiveLog("RX PIDZP parse error: " + payload);
                }
                return;
            }

            if (msg.StartsWith("PIDZV:", StringComparison.Ordinal) || msg.StartsWith("PIDZV_SET:", StringComparison.Ordinal))
            {
                string payload = msg[(msg.StartsWith("PIDZV_SET:", StringComparison.Ordinal) ? 10 : 6)..].Trim();
                if (TryParsePidPayload(payload, out string kp, out string ki, out string kd)
                    && float.TryParse(kp, NumberStyles.Float, CultureInfo.InvariantCulture, out float vkp)
                    && float.TryParse(ki, NumberStyles.Float, CultureInfo.InvariantCulture, out float vki)
                    && float.TryParse(kd, NumberStyles.Float, CultureInfo.InvariantCulture, out float vkd))
                {
                    _hybridConfig.VelKp = vkp;
                    _hybridConfig.VelKi = vki;
                    _hybridConfig.VelKd = vkd;
                    AppendLiveLog("RX Geschwindigkeit PID");
                }
                return;
            }

            if (msg.StartsWith("PIDZP_ERR:", StringComparison.Ordinal)
                || msg.StartsWith("PIDZV_ERR:", StringComparison.Ordinal)
                || msg.StartsWith("Z_NEUTRAL:", StringComparison.Ordinal))
            {
                TxtStatus.Text = msg;
                AppendLiveLog("RX " + msg);
                return;
            }

            if (msg.StartsWith("ZV:", StringComparison.Ordinal) || msg.StartsWith("ZV_SET:", StringComparison.Ordinal))
            {
                string payload = msg.Substring(msg.IndexOf(':') + 1).Trim();
                if (int.TryParse(payload, out int level) && level >= 1 && level <= 5)
                {
                    SldSpeedLevel.ValueChanged -= SldSpeedLevel_ValueChanged;
                    SldSpeedLevel.Value = level;
                    TxtSpeedLevel.Text = $"Stufe: {level}/5";
                    SldSpeedLevel.ValueChanged += SldSpeedLevel_ValueChanged;
                }
                return;
            }

            if (msg.StartsWith("ZLIM:", StringComparison.Ordinal))
            {
                string payload = msg.Substring(5).Trim();
                string[] parts = payload.Split(';', StringSplitOptions.TrimEntries);
                if (parts.Length == 2
                    && uint.TryParse(parts[0], out uint lower)
                    && uint.TryParse(parts[1], out uint upper))
                {
                    _zStepLowerLimit = lower;
                    _zStepUpperLimit = upper;
                    AppendLiveLog($"Limits: {lower}-{upper}");
                }
                else
                {
                    AppendLiveLog("RX ZLIM parse error: " + payload);
                }
                return;
            }

            if (msg.StartsWith("TEST_SUMMARY:", StringComparison.Ordinal))
            {
                TxtStatus.Text = "Test-Ergebnis";
                AppendTestSummary(msg);
                return;
            }

            if (msg.StartsWith("_STATUS_"))
            {
                if (_isLoggingSession && msg != _lastStatusMessage)
                    _currentLogFile?.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");

                _lastStatusMessage = msg;
                string state = msg.Replace("_STATUS_", "").Trim();
                if (state == _currentDeviceState)
                {
                    return;
                }

                _currentDeviceState = state;
                TxtStatus.Text = GetStateFriendlyName(state);
                AppendLiveLog("STATE " + state);
                UpdateButtonStates(state);
                if (state == "TEST_START")
                {
                    SendCommand("L?");
                }
                return;
            }

            TxtStatus.Text = msg;
            AppendLiveLog("RX " + msg);
        }

        private void InitializePidPresets()
        {
            _pidPresets.Clear();
            _pidPresets.Add(new PidPreset
            {
                Name = StablePreset.Name,
                Kp = StablePreset.Kp,
                Ki = StablePreset.Ki,
                Kd = StablePreset.Kd,
                IsBuiltIn = true
            });

            foreach (PidPreset preset in LoadUserPidPresetsFromDisk())
            {
                if (string.Equals(preset.Name, StablePreset.Name, StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }
                _pidPresets.Add(preset);
            }

            CmbPidPreset.ItemsSource = null;
            CmbPidPreset.ItemsSource = _pidPresets;
            CmbPidPreset.DisplayMemberPath = nameof(PidPreset.Name);
            CmbPidPreset.SelectedIndex = 0;
            CmbPidPreset.Text = StablePreset.Name;
        }

        private void AppendLiveLog(string line)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;

            if (string.Equals(_lastLiveLogLine, line, StringComparison.Ordinal))
            {
                return;
            }

            _lastLiveLogLine = line;

            if (_liveLogLines.Count >= LiveLogMaxLines)
                _liveLogLines.Dequeue();

            _liveLogLines.Enqueue($"{DateTime.Now:HH:mm:ss} {line}");
            TxtLiveLog.Text = string.Join(Environment.NewLine, _liveLogLines);
            TxtLiveLog.ScrollToEnd();
        }

        private void AppendTestSummary(string line)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;

            string payload = line;
            int prefixIndex = payload.IndexOf("TEST_SUMMARY:", StringComparison.Ordinal);
            if (prefixIndex >= 0)
            {
                payload = payload.Substring(prefixIndex + "TEST_SUMMARY:".Length);
            }

            Dictionary<string, string> values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (string part in payload.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
            {
                int eqIndex = part.IndexOf('=');
                if (eqIndex > 0)
                {
                    values[part.Substring(0, eqIndex)] = part.Substring(eqIndex + 1);
                }
            }

            string status = values.TryGetValue("status", out string? s) ? s : "?";
            string cycles = values.TryGetValue("cycles", out string? c) ? c : "?";
            string done = values.TryGetValue("done", out string? d) ? d : "?";
            string lost = values.TryGetValue("lost", out string? l) ? l : "?";
            string delta = values.TryGetValue("last_delta", out string? dlt) ? dlt : "?";
            string overshoot = values.TryGetValue("overshoot", out string? over) ? over : "?";
            string validSensor = values.TryGetValue("valid_sensor", out string? valid) ? valid : "?";
            string invalidSensor = values.TryGetValue("invalid_sensor", out string? invalid) ? invalid : "?";
            string motorFault = values.TryGetValue("motor_fault", out string? fault) ? fault : "?";
            string phase = values.TryGetValue("phase", out string? p) ? p : "?";
            string runtime = "";
            if (values.TryGetValue("time_m", out string? tm) || values.TryGetValue("time_s", out string? ts))
            {
                string minutes = values.TryGetValue("time_m", out string? minVal) ? minVal : "0";
                string seconds = values.TryGetValue("time_s", out string? secVal) ? secVal : "0";
                runtime = $" | Zeit: {minutes}m {seconds}s";
            }

            string header = $"{DateTime.Now:HH:mm:ss} TEST {status} ({cycles}/{done}) lost={lost} | Δ={delta} | overshoot={overshoot} | phase={phase}{runtime}";
            string details = string.Format(
                CultureInfo.InvariantCulture,
                "IST min/max: {0} / {1} | SOLL min/max: {2} / {3} | NO-Sensor: {4} | Δ={5} | overshoot={6} | valid={7} | invalid={8} | motor_fault={9}{10}",
                values.TryGetValue("z_ist_min", out string? istMin) ? istMin : "-",
                values.TryGetValue("z_ist_max", out string? istMax) ? istMax : "-",
                values.TryGetValue("z_soll_min", out string? sollMin) ? sollMin : "-",
                values.TryGetValue("z_soll_max", out string? sollMax) ? sollMax : "-",
                values.TryGetValue("no_sensor_pos", out string? noPos) ? noPos : "-",
                delta,
                overshoot,
                validSensor,
                invalidSensor,
                motorFault,
                runtime);

            if (values.TryGetValue("last_error", out string? err) && !string.IsNullOrWhiteSpace(err))
            {
                details += " | ERR: " + err;
            }

            while (_testSummaryLines.Count >= 8)
            {
                _testSummaryLines.Dequeue();
            }

            _testSummaryLines.Enqueue(header);
            _testSummaryLines.Enqueue(details);
            TxtTestSummary.Text = string.Join(Environment.NewLine, _testSummaryLines);
            TxtTestSummary.ScrollToEnd();
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
            BtnHybridTuning.IsEnabled      = isConnected;
        }

        private static string NormalizeDecimalText(string value)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                return string.Empty;
            }

            return value.Trim().Replace(',', '.');
        }

        private static string FormatPidDisplayValue(float value)
        {
            return value.ToString("0.###############", CultureInfo.InvariantCulture);
        }

        private static bool TryParsePidPayload(string payload, out string kp, out string ki, out string kd)
        {
            kp = string.Empty;
            ki = string.Empty;
            kd = string.Empty;
            string[] parts;
            if (payload.Contains(';', StringComparison.Ordinal))
            {
                parts = payload.Split(';', StringSplitOptions.TrimEntries);
            }
            else
            {
                parts = payload.Split(',', StringSplitOptions.TrimEntries);
            }
            if (parts.Length != 3)
            {
                return false;
            }

            if (!float.TryParse(parts[0], NumberStyles.Float, CultureInfo.InvariantCulture, out float kpValue)
                || !float.TryParse(parts[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float kiValue)
                || !float.TryParse(parts[2], NumberStyles.Float, CultureInfo.InvariantCulture, out float kdValue))
            {
                return false;
            }

            kp = FormatPidDisplayValue(kpValue);
            ki = FormatPidDisplayValue(kiValue);
            kd = FormatPidDisplayValue(kdValue);
            return true;
        }

        private bool TryReadPidInput(out float kp, out float ki, out float kd)
        {
            kp = 0.0f;
            ki = 0.0f;
            kd = 0.0f;

            string kpText = NormalizeDecimalText(TxtPidKpNew.Text);
            string kiText = NormalizeDecimalText(TxtPidKiNew.Text);
            string kdText = NormalizeDecimalText(TxtPidKdNew.Text);

            bool ok =
                float.TryParse(kpText, NumberStyles.Float, CultureInfo.InvariantCulture, out kp) &&
                float.TryParse(kiText, NumberStyles.Float, CultureInfo.InvariantCulture, out ki) &&
                float.TryParse(kdText, NumberStyles.Float, CultureInfo.InvariantCulture, out kd);
            return ok && kp >= 0.0f && ki >= 0.0f && kd >= 0.0f;
        }

        private List<PidPreset> LoadUserPidPresetsFromDisk()
        {
            List<PidPreset> presets = new List<PidPreset>();
            try
            {
                if (!File.Exists(PidPresetPath))
                {
                    return presets;
                }

                string json = File.ReadAllText(PidPresetPath);
                List<PidPreset>? loaded = JsonSerializer.Deserialize<List<PidPreset>>(json);
                if (loaded == null)
                {
                    return presets;
                }

                foreach (PidPreset preset in loaded)
                {
                    if (string.IsNullOrWhiteSpace(preset.Name))
                    {
                        continue;
                    }
                    presets.Add(new PidPreset
                    {
                        Name = preset.Name.Trim(),
                        Kp = preset.Kp,
                        Ki = preset.Ki,
                        Kd = preset.Kd,
                        IsBuiltIn = false
                    });
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"PID preset load error: {ex.Message}");
            }

            return presets;
        }

        private void SaveUserPidPresetsToDisk()
        {
            string? dir = System.IO.Path.GetDirectoryName(PidPresetPath);
            if (!string.IsNullOrWhiteSpace(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }

            List<PidPreset> userPresets = new List<PidPreset>();
            foreach (PidPreset preset in _pidPresets)
            {
                if (preset.IsBuiltIn)
                {
                    continue;
                }
                userPresets.Add(new PidPreset
                {
                    Name = preset.Name,
                    Kp = preset.Kp,
                    Ki = preset.Ki,
                    Kd = preset.Kd,
                    IsBuiltIn = false
                });
            }

            string json = JsonSerializer.Serialize(userPresets, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(PidPresetPath, json);
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
            if (TryReadStepCount(out uint steps))
            {
                SendBoundedStep(steps, true);
            }
        }

        private void BtnDown_Click(object sender, RoutedEventArgs e)
        {
            if (TryReadStepCount(out uint steps))
            {
                SendBoundedStep(steps, false);
            }
        }

        private void SldSpeedLevel_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!IsLoaded)
            {
                // Fires while InitializeComponent still builds the tree (TxtSpeedLevel not ready yet).
                return;
            }
            int level = (int)Math.Round(e.NewValue);
            TxtSpeedLevel.Text = $"Stufe: {level}/5";
            SendCommand($"V={level}");
        }

        private void SendBoundedStep(uint steps, bool isUp)
        {
            if (_zStepLowerLimit.HasValue && _zStepUpperLimit.HasValue
                && uint.TryParse(TxtZaxisPosSoll.Text.Trim(), out uint current))
            {
                uint lower = _zStepLowerLimit.Value;
                uint upper = _zStepUpperLimit.Value;
                long delta = isUp ? (long)steps : -(long)steps;
                long wanted = (long)current + delta;
                long clamped = Math.Min(upper, Math.Max(lower, wanted));
                SendCommand($"Z{clamped}");
                return;
            }

            SendCommand(isUp ? $"+{steps}" : $"-{steps}");
        }

        private bool TryReadStepCount(out uint steps)
        {
            steps = 0;
            string text = TxtStepCount.Text.Trim();
            if (!uint.TryParse(text, out steps) || steps == 0)
            {
                TxtStatus.Text = "Ungültige Schrittzahl";
                AppendLiveLog("Step count invalid: " + text);
                return false;
            }

            return true;
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("q");
        }

        private void BtnClearSummary_Click(object sender, RoutedEventArgs e)
        {
            TxtTestSummary.Clear();
            _testSummaryLines.Clear();
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
            SendCommand("PP?");
        }

        private void BtnPidApply_Click(object sender, RoutedEventArgs e)
        {
            if (!TryReadPidInput(out float kp, out float ki, out float kd))
            {
                MessageBox.Show("Ungültige PID-Werte. Format z.B. KP 0.003, KI 0.000001, KD 0.018",
                    "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string cmd = string.Format(
                CultureInfo.InvariantCulture,
                "PP={0},{1},{2}",
                kp.ToString("0.######", CultureInfo.InvariantCulture),
                ki.ToString("0.#########", CultureInfo.InvariantCulture),
                kd.ToString("0.######", CultureInfo.InvariantCulture));
            SendCommand(cmd);
        }

        private void BtnPidSave_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (!TryReadPidInput(out float kp, out float ki, out float kd))
                {
                    MessageBox.Show("Ungültige PID-Werte. Format z.B. KP 0.003, KI 0.000001, KD 0.018",
                        "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                string presetName = CmbPidPreset.Text.Trim();
                if (string.IsNullOrWhiteSpace(presetName))
                {
                    MessageBox.Show("Bitte Preset-Namen eingeben.", "Eingabefehler",
                        MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                if (string.Equals(presetName, StablePreset.Name, StringComparison.OrdinalIgnoreCase))
                {
                    MessageBox.Show("Das feste Preset 'Stabil (Empfohlen)' kann nicht überschrieben werden.",
                        "Preset geschützt", MessageBoxButton.OK, MessageBoxImage.Information);
                    return;
                }

                PidPreset? existing = null;
                foreach (PidPreset preset in _pidPresets)
                {
                    if (string.Equals(preset.Name, presetName, StringComparison.OrdinalIgnoreCase))
                    {
                        existing = preset;
                        break;
                    }
                }

                string kpText = FormatPidDisplayValue(kp);
                string kiText = FormatPidDisplayValue(ki);
                string kdText = FormatPidDisplayValue(kd);
                if (existing != null)
                {
                    existing.Kp = kpText;
                    existing.Ki = kiText;
                    existing.Kd = kdText;
                }
                else
                {
                    _pidPresets.Add(new PidPreset
                    {
                        Name = presetName,
                        Kp = kpText,
                        Ki = kiText,
                        Kd = kdText,
                        IsBuiltIn = false
                    });
                }

                SaveUserPidPresetsToDisk();
                CmbPidPreset.Items.Refresh();
                CmbPidPreset.Text = presetName;
                TxtStatus.Text = $"Preset gespeichert: {presetName}";
                AppendLiveLog("Preset gespeichert: " + presetName);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Speichern fehlgeschlagen: {ex.Message}",
                    "Dateifehler", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnPidLoad_Click(object sender, RoutedEventArgs e)
        {
            string presetName = CmbPidPreset.Text.Trim();
            if (string.IsNullOrWhiteSpace(presetName))
            {
                TxtStatus.Text = "Preset-Name fehlt";
                return;
            }

            PidPreset? presetToDelete = null;
            foreach (PidPreset preset in _pidPresets)
            {
                if (string.Equals(preset.Name, presetName, StringComparison.OrdinalIgnoreCase))
                {
                    presetToDelete = preset;
                    break;
                }
            }

            if (presetToDelete == null)
            {
                TxtStatus.Text = $"Preset nicht gefunden: {presetName}";
                return;
            }

            if (presetToDelete.IsBuiltIn)
            {
                TxtStatus.Text = "Stabil-Preset kann nicht gelöscht werden";
                return;
            }

            MessageBoxResult result = MessageBox.Show(
                $"Preset '{presetToDelete.Name}' löschen?",
                "Preset löschen",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);
            if (result != MessageBoxResult.Yes)
            {
                return;
            }

            _pidPresets.Remove(presetToDelete);
            SaveUserPidPresetsToDisk();
            CmbPidPreset.Items.Refresh();
            CmbPidPreset.SelectedIndex = 0;
            CmbPidPreset.Text = StablePreset.Name;
            TxtStatus.Text = $"Preset gelöscht: {presetToDelete.Name}";
            AppendLiveLog("Preset gelöscht: " + presetToDelete.Name);
        }

        private void BtnPidNeutral_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("N");
        }

        private void BtnHybridTuning_Click(object sender, RoutedEventArgs e)
        {
            HybridTuningWindow dialog = new HybridTuningWindow(new HybridTuningConfig
            {
                PosKp = _hybridConfig.PosKp,
                PosKi = _hybridConfig.PosKi,
                PosKd = _hybridConfig.PosKd,
                VelKp = _hybridConfig.VelKp,
                VelKi = _hybridConfig.VelKi,
                VelKd = _hybridConfig.VelKd
            })
            {
                Owner = this
            };

            bool? result = dialog.ShowDialog();
            if (result != true)
            {
                return;
            }

            _hybridConfig.PosKp = dialog.Config.PosKp;
            _hybridConfig.PosKi = dialog.Config.PosKi;
            _hybridConfig.PosKd = dialog.Config.PosKd;
            _hybridConfig.VelKp = dialog.Config.VelKp;
            _hybridConfig.VelKi = dialog.Config.VelKi;
            _hybridConfig.VelKd = dialog.Config.VelKd;

            string posCmd = string.Format(
                CultureInfo.InvariantCulture,
                "PP={0},{1},{2}",
                _hybridConfig.PosKp.ToString("0.######", CultureInfo.InvariantCulture),
                _hybridConfig.PosKi.ToString("0.#########", CultureInfo.InvariantCulture),
                _hybridConfig.PosKd.ToString("0.######", CultureInfo.InvariantCulture));
            string velCmd = string.Format(
                CultureInfo.InvariantCulture,
                "PV={0},{1},{2}",
                _hybridConfig.VelKp.ToString("0.######", CultureInfo.InvariantCulture),
                _hybridConfig.VelKi.ToString("0.#########", CultureInfo.InvariantCulture),
                _hybridConfig.VelKd.ToString("0.######", CultureInfo.InvariantCulture));

            SendCommand(posCmd);
            SendCommand(velCmd);
            TxtStatus.Text = "Kaskaden-Tuning gesendet";
            AppendLiveLog("Kaskaden-Tuning gespeichert");
        }

        private void BtnPidPresetApply_Click(object sender, RoutedEventArgs e)
        {
            PidPreset? preset = CmbPidPreset.SelectedItem as PidPreset;
            if (preset == null)
            {
                string presetName = CmbPidPreset.Text.Trim();
                foreach (PidPreset candidate in _pidPresets)
                {
                    if (string.Equals(candidate.Name, presetName, StringComparison.OrdinalIgnoreCase))
                    {
                        preset = candidate;
                        break;
                    }
                }
            }
            if (preset == null)
            {
                TxtStatus.Text = "Preset nicht gefunden";
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
