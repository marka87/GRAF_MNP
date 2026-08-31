using System.Globalization;
using System.Windows;

namespace MnpControl
{
    public sealed class HybridTuningConfig
    {
        public float FastKp { get; set; }
        public float FastKi { get; set; }
        public float FastKd { get; set; }
        public float MediumKp { get; set; }
        public float MediumKi { get; set; }
        public float MediumKd { get; set; }
        public float SlowKp { get; set; }
        public float SlowKi { get; set; }
        public float SlowKd { get; set; }
        public uint SlowEnter { get; set; }
        public uint FastExit { get; set; }
        public uint MediumEnter { get; set; }
        public uint MediumExit { get; set; }
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
            TxtMediumKp.Text = config.MediumKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtMediumKi.Text = config.MediumKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtMediumKd.Text = config.MediumKd.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowKp.Text = config.SlowKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowKi.Text = config.SlowKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtSlowKd.Text = config.SlowKd.ToString("0.######", CultureInfo.InvariantCulture);
            TxtSlowEnter.Text = config.SlowEnter.ToString(CultureInfo.InvariantCulture);
            TxtFastExit.Text = config.FastExit.ToString(CultureInfo.InvariantCulture);
            TxtMediumEnter.Text = config.MediumEnter.ToString(CultureInfo.InvariantCulture);
            TxtMediumExit.Text = config.MediumExit.ToString(CultureInfo.InvariantCulture);
            TxtHoldDelta.Text = config.HoldDelta.ToString(CultureInfo.InvariantCulture);
            TxtHoldCycles.Text = config.HoldCycles.ToString(CultureInfo.InvariantCulture);
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (!float.TryParse(TxtFastKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKp)
                || !float.TryParse(TxtFastKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKi)
                || !float.TryParse(TxtFastKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float fastKd)
                || !float.TryParse(TxtMediumKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float mediumKp)
                || !float.TryParse(TxtMediumKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float mediumKi)
                || !float.TryParse(TxtMediumKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float mediumKd)
                || !float.TryParse(TxtSlowKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKp)
                || !float.TryParse(TxtSlowKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKi)
                || !float.TryParse(TxtSlowKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float slowKd)
                || !uint.TryParse(TxtSlowEnter.Text.Trim(), out uint slowEnter)
                || !uint.TryParse(TxtFastExit.Text.Trim(), out uint fastExit)
                || !uint.TryParse(TxtMediumEnter.Text.Trim(), out uint mediumEnter)
                || !uint.TryParse(TxtMediumExit.Text.Trim(), out uint mediumExit)
                || !uint.TryParse(TxtHoldDelta.Text.Trim(), out uint holdDelta)
                || !uint.TryParse(TxtHoldCycles.Text.Trim(), out uint holdCycles))
            {
                MessageBox.Show("Ungültige Eingabe.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (fastKp <= 0 || fastKi <= 0 || fastKd < 0
                || mediumKp <= 0 || mediumKi <= 0 || mediumKd < 0
                || slowKp <= 0 || slowKi <= 0 || slowKd < 0)
            {
                MessageBox.Show("PID-Werte müssen positiv sein (KD darf 0 sein).", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            if (slowEnter == 0 || fastExit <= slowEnter || holdDelta == 0)
            {
                MessageBox.Show("Schwellen ungültig: FastExit muss größer als SlowEnter sein, Delta > 0.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            if (mediumEnter == 0 || mediumExit <= mediumEnter)
            {
                MessageBox.Show("Medium-Schwellen ungültig: Medium Exit muss größer als Medium Enter sein.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            Config.FastKp = fastKp;
            Config.FastKi = fastKi;
            Config.FastKd = fastKd;
            Config.MediumKp = mediumKp;
            Config.MediumKi = mediumKi;
            Config.MediumKd = mediumKd;
            Config.SlowKp = slowKp;
            Config.SlowKi = slowKi;
            Config.SlowKd = slowKd;
            Config.SlowEnter = slowEnter;
            Config.FastExit = fastExit;
            Config.MediumEnter = mediumEnter;
            Config.MediumExit = mediumExit;
            Config.HoldDelta = holdDelta;
            Config.HoldCycles = holdCycles;
            DialogResult = true;
        }
    }
}
