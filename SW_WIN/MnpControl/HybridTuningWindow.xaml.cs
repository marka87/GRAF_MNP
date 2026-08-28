using System.Globalization;
using System.Windows;

namespace MnpControl
{
    public sealed class HybridTuningConfig
    {
        public float FastKp { get; set; }
        public float FastKi { get; set; }
        public float FastKd { get; set; }
        public float SlowKp { get; set; }
        public float SlowKi { get; set; }
        public float SlowKd { get; set; }
        public uint SlowEnter { get; set; }
        public uint FastExit { get; set; }
        public uint HoldDelta { get; set; }
        public uint HoldCycles { get; set; }
    }

    public partial class HybridTuningWindow : Window
    {
        public HybridTuningConfig Config { get; }

        public HybridTuningWindow(HybridTuningConfig config)
        {
            InitializeComponent();
            Config = config;
            TxtFastKp.Text = config.FastKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtFastKi.Text = config.FastKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtFastKd.Text = config.FastKd.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowKp.Text = config.SlowKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowKi.Text = config.SlowKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtSlowKd.Text = config.SlowKd.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowEnter.Text = config.SlowEnter.ToString(CultureInfo.InvariantCulture);
            TxtFastExit.Text = config.FastExit.ToString(CultureInfo.InvariantCulture);
            TxtHoldDelta.Text = config.HoldDelta.ToString(CultureInfo.InvariantCulture);
            TxtHoldCycles.Text = config.HoldCycles.ToString(CultureInfo.InvariantCulture);
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (!float.TryParse(TxtFastKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKp)
                || !float.TryParse(TxtFastKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKi)
                || !float.TryParse(TxtFastKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKd)
                || !float.TryParse(TxtSlowKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKp)
                || !float.TryParse(TxtSlowKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKi)
                || !float.TryParse(TxtSlowKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKd)
                || !uint.TryParse(TxtSlowEnter.Text.Trim(), out uint slowEnter)
                || !uint.TryParse(TxtFastExit.Text.Trim(), out uint fastExit)
                || !uint.TryParse(TxtHoldDelta.Text.Trim(), out uint holdDelta)
                || !uint.TryParse(TxtHoldCycles.Text.Trim(), out uint holdCycles))
            {
                MessageBox.Show("Ungültige Eingabe.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (fastKp <= 0 || fastKi <= 0 || fastKd < 0 || slowKp <= 0 || slowKi <= 0 || slowKd < 0)
            {
                MessageBox.Show("PID-Werte müssen positiv sein (KD darf 0 sein).", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            if (slowEnter == 0 || fastExit <= slowEnter || holdDelta == 0)
            {
                MessageBox.Show("Schwellen ungültig: FastExit muss größer als SlowEnter sein, Delta > 0.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            Config.FastKp = fastKp;
            Config.FastKi = fastKi;
            Config.FastKd = fastKd;
            Config.SlowKp = slowKp;
            Config.SlowKi = slowKi;
            Config.SlowKd = slowKd;
            Config.SlowEnter = slowEnter;
            Config.FastExit = fastExit;
            Config.HoldDelta = holdDelta;
            Config.HoldCycles = holdCycles;
            DialogResult = true;
        }
    }
}
