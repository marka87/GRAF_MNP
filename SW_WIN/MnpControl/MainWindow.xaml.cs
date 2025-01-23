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
        private static SerialPort _devConnection = new SerialPort("COM4", 115200, Parity.None, 8, StopBits.One);

        private static bool _continue = true;
        private StreamWriter _currentLogFile; // Logfile für den aktuellen Ablauf
        private StreamWriter _filteredLogFile; // Gefilterte Log-Datei

        private Thread readThread;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            _devConnection.ReadTimeout = 500;
            _devConnection.WriteTimeout = 500;

            _devConnection.Open();

            readThread = new Thread(Read);
            readThread.Start();
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

            // Nachricht in die Datei schreiben, wenn sie relevant ist
            if (_currentLogFile != null && msg.Contains("Zeit") && msg.Contains("Position"))
            {
                _currentLogFile.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} - {msg}");
            }

            string[] sSplit = msg.Split(";", StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (sSplit.Length == 5)
            {
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
                msg = msg.Replace("_STATUS_", "");
                TxtStatus.Text = msg.Trim();
                return;
            }
        }

        private void Read()
        {
            while (_continue)
            {
                try
                {
                    string message = _devConnection.ReadLine();
                    Application.Current.Dispatcher.Invoke(new Action(() => { UpdateStatus(message); }));
                }
                catch (TimeoutException) { }
            }
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            _continue = false;

            readThread.Join();
            _devConnection.Close();
            CloseLogFile(); // Sicherstellen, dass das aktuelle Logfile geschlossen wird
        }

        private void BtnStartReferenceRun_Click(object sender, RoutedEventArgs e)
        {
            CloseLogFile(); // Eventuell vorheriges Logfile schließen
            StartNewLogFile(); // Neues Logfile erstellen
            _devConnection.Write("s");
        }

        private void BtnStartDemoRun_Click(object sender, RoutedEventArgs e)
        {
            CloseLogFile();
            StartNewLogFile();
            _devConnection.Write("1");
        }

        private void BtnStartShortRun_Click(object sender, RoutedEventArgs e)
        {
            CloseLogFile();
            StartNewLogFile();
            _devConnection.Write("2");
        }

        private void BtnStartLongRun_Click(object sender, RoutedEventArgs e)
        {
            CloseLogFile();
            StartNewLogFile();
            _devConnection.Write("3");
        }

        private void BtnReset_Click(object sender, RoutedEventArgs e)
        {
            _devConnection.Write("r");
        }

        private void BtnUp_Click(object sender, RoutedEventArgs e)
        {
            _devConnection.Write("+");
        }

        private void BtnDown_Click(object sender, RoutedEventArgs e)
        {
            _devConnection.Write("-");
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            _devConnection.Write("q");
        }

        private void BtnTestStart_click(object sender, RoutedEventArgs e)
        {
            CloseLogFile();
            StartNewLogFile();
            _devConnection.Write("e");
        }
    }
}
