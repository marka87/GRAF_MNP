using System.Windows;

namespace MnpControl
{
    public partial class CarriageProfileWindow : Window
    {
        private readonly MainWindow _main;

        public CarriageProfileWindow(MainWindow main)
        {
            InitializeComponent();
            _main = main;

            TxtDialogCarriageId.Text = _main.TxtCfgCarriageId.Text;
            TxtDialogLandingMargin.Text = _main.TxtCfgLandingMargin.Text;
            int idx = _main.CmbCfgLandingSpeed.SelectedIndex;
            if (idx >= 0 && idx < CmbDialogLandingSpeed.Items.Count)
            {
                CmbDialogLandingSpeed.SelectedIndex = idx;
            }
            else
            {
                CmbDialogLandingSpeed.SelectedIndex = 2; // Stufe 3
            }
            TxtDialogFlashStatus.Text = _main.TxtCfgFlashStatus.Text;
        }

        public void SyncFromMain(string id, string lz, int speedIndex, string flashStatus)
        {
            TxtDialogCarriageId.Text = id;
            TxtDialogLandingMargin.Text = lz;
            if (speedIndex >= 0 && speedIndex < CmbDialogLandingSpeed.Items.Count)
            {
                CmbDialogLandingSpeed.SelectedIndex = speedIndex;
            }
            TxtDialogFlashStatus.Text = flashStatus;
        }

        private void BtnDialogDefault_Click(object sender, RoutedEventArgs e)
        {
            TxtDialogCarriageId.Text = "DEFAULT";
            TxtDialogLandingMargin.Text = "150";
            CmbDialogLandingSpeed.SelectedIndex = 2;
            TxtDialogFlashStatus.Text = "DEFAULT im RAM aktiv";

            _main.BtnProfileDefault_Click(sender, e);
        }

        private void BtnDialogRead_Click(object sender, RoutedEventArgs e)
        {
            _main.BtnCfgRead_Click(sender, e);
        }

        private void BtnDialogApply_Click(object sender, RoutedEventArgs e)
        {
            _main.TxtCfgCarriageId.Text = TxtDialogCarriageId.Text;
            _main.TxtCfgLandingMargin.Text = TxtDialogLandingMargin.Text;
            _main.CmbCfgLandingSpeed.SelectedIndex = CmbDialogLandingSpeed.SelectedIndex;

            _main.BtnCfgApply_Click(sender, e);
            TxtDialogFlashStatus.Text = _main.TxtCfgFlashStatus.Text;
        }

        private void BtnDialogSave_Click(object sender, RoutedEventArgs e)
        {
            _main.TxtCfgCarriageId.Text = TxtDialogCarriageId.Text;
            _main.TxtCfgLandingMargin.Text = TxtDialogLandingMargin.Text;
            _main.CmbCfgLandingSpeed.SelectedIndex = CmbDialogLandingSpeed.SelectedIndex;

            _main.BtnCfgSave_Click(sender, e);
            TxtDialogFlashStatus.Text = _main.TxtCfgFlashStatus.Text;
        }

        private void BtnDialogClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
