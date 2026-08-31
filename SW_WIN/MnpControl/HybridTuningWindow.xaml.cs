using System.Globalization;
using System.Windows;

namespace MnpControl
{
    public sealed class HybridTuningConfig
    {
        public float PosKp { get; set; }
        public float PosKi { get; set; }
        public float PosKd { get; set; }
        public float VelKp { get; set; }
        public float VelKi { get; set; }
        public float VelKd { get; set; }
    }

    public partial class HybridTuningWindow : Window
    {
        public HybridTuningConfig Config { get; }

        public HybridTuningWindow(HybridTuningConfig config)
        {
            InitializeComponent();
            Config = config;
            TxtPosKp.Text = config.PosKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtPosKi.Text = config.PosKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtPosKd.Text = config.PosKd.ToString("0.######", CultureInfo.InvariantCulture);
            TxtVelKp.Text = config.VelKp.ToString("0.######", CultureInfo.InvariantCulture);
            TxtVelKi.Text = config.VelKi.ToString("0.#########", CultureInfo.InvariantCulture);
            TxtVelKd.Text = config.VelKd.ToString("0.######", CultureInfo.InvariantCulture);
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            if (!float.TryParse(TxtPosKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float posKp)
                || !float.TryParse(TxtPosKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float posKi)
                || !float.TryParse(TxtPosKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float posKd)
                || !float.TryParse(TxtVelKp.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float velKp)
                || !float.TryParse(TxtVelKi.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float velKi)
                || !float.TryParse(TxtVelKd.Text.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float velKd))
            {
                MessageBox.Show("Ungültige Eingabe.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (posKp < 0 || posKi < 0 || posKd < 0 || velKp < 0 || velKi < 0 || velKd < 0)
            {
                MessageBox.Show("PID-Werte dürfen nicht negativ sein.", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            Config.PosKp = posKp;
            Config.PosKi = posKi;
            Config.PosKd = posKd;
            Config.VelKp = velKp;
            Config.VelKi = velKi;
            Config.VelKd = velKd;
            DialogResult = true;
        }
    }
}
