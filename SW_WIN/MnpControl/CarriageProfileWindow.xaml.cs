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

        private void BtnProfileA_Click(object sender, RoutedEventArgs e)
        {
            TxtDialogCarriageId.Text = "SCHLITTEN-A";
            TxtDialogLandingMargin.Text = "100";
            CmbDialogLandingSpeed.SelectedIndex = 3; // Stufe 4 (8 Inc/ms)
            TxtDialogFlashStatus.Text = "SCHLITTEN-A gewählt";

            _main.TxtCfgCarriageId.Text = "SCHLITTEN-A";
            _main.TxtCfgLandingMargin.Text = "100";
            _main.CmbCfgLandingSpeed.SelectedIndex = 3;
            _main.SendCommand("CMD:SET_CONFIG:ID=SCHLITTEN-A;LZ=100;LS=4;BF=120");
            _main.TxtStatus.Text = "Profil SCHLITTEN-A im RAM aktiviert";
            _main.TxtCfgFlashStatus.Text = "Im RAM angewendet (ungespeichert)";
            TxtDialogFlashStatus.Text = _main.TxtCfgFlashStatus.Text;
            _main.AppendLiveLog("[PROFIL] SCHLITTEN-A aktiviert (LZ=100 inc, Stufe 4, BF=120)");
        }

        private void BtnProfileB_Click(object sender, RoutedEventArgs e)
        {
            TxtDialogCarriageId.Text = "SCHLITTEN-B";
            TxtDialogLandingMargin.Text = "150";
            CmbDialogLandingSpeed.SelectedIndex = 2; // Stufe 3 (4 Inc/ms)
            TxtDialogFlashStatus.Text = "SCHLITTEN-B gewählt";

            _main.TxtCfgCarriageId.Text = "SCHLITTEN-B";
            _main.TxtCfgLandingMargin.Text = "150";
            _main.CmbCfgLandingSpeed.SelectedIndex = 2;
            _main.SendCommand("CMD:SET_CONFIG:ID=SCHLITTEN-B;LZ=150;LS=3;BF=120");
            _main.TxtStatus.Text = "Profil SCHLITTEN-B im RAM aktiviert";
            _main.TxtCfgFlashStatus.Text = "Im RAM angewendet (ungespeichert)";
            TxtDialogFlashStatus.Text = _main.TxtCfgFlashStatus.Text;
            _main.AppendLiveLog("[PROFIL] SCHLITTEN-B aktiviert (LZ=150 inc, Stufe 3, BF=120)");
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

