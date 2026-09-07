using System;
using System.Collections.Generic;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace MnpControl
{
    public partial class MimotTestDialog : Window
    {
        public MimotTestConfig Config { get; private set; } = new();

        private static readonly string SettingsFilePath = Path.Combine(
            AppDomain.CurrentDomain.BaseDirectory,
            "mimot_last_session.txt");

        public MimotTestDialog(int initialCycles = 10)
        {
            InitializeComponent();
            TxtCycles.Text = initialCycles > 0 ? initialCycles.ToString() : "10";
            LoadLastSettings();
        }

        private void LoadLastSettings()
        {
            try
            {
                if (File.Exists(SettingsFilePath))
                {
                    string[] lines = File.ReadAllLines(SettingsFilePath);
                    if (lines.Length > 0 && !string.IsNullOrWhiteSpace(lines[0]))
                    {
                        TxtOperator.Text = lines[0].Trim();
                    }
                    if (lines.Length > 1 && !string.IsNullOrWhiteSpace(lines[1]))
                    {
                        TxtSerial.Text = lines[1].Trim();
                    }
                }
            }
            catch
            {
                // Unkritisch
            }
        }

        private void SaveLastSettings()
        {
            try
            {
                File.WriteAllLines(SettingsFilePath, new[]
                {
                    TxtOperator.Text.Trim(),
                    TxtSerial.Text.Trim()
                });
            }
            catch
            {
                // Unkritisch
            }
        }

        private void BtnSelectAll_Click(object sender, RoutedEventArgs e)
        {
            Chk1.IsChecked = true;
            Chk2.IsChecked = true;
            Chk3.IsChecked = true;
            Chk4.IsChecked = true;
            Chk5.IsChecked = true;
            Chk6.IsChecked = true;
            Chk7.IsChecked = true;
            Chk8.IsChecked = true;
            Chk9.IsChecked = true;
            Chk10.IsChecked = true;
            Chk11.IsChecked = true;
        }

        private void BtnStart_Click(object sender, RoutedEventArgs e)
        {
            string op = TxtOperator.Text.Trim();
            if (string.IsNullOrWhiteSpace(op))
            {
                MessageBox.Show("Bitte ein Prüfer-Kürzel oder eine Personalnummer eingeben (max. 4 Zeichen).", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                TxtOperator.Focus();
                return;
            }
            if (op.Length > 4)
            {
                op = op.Substring(0, 4);
            }

            string serial = TxtSerial.Text.Trim();
            if (string.IsNullOrWhiteSpace(serial))
            {
                MessageBox.Show("Bitte eine Seriennummer eingeben (max. 20 Zeichen).", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                TxtSerial.Focus();
                return;
            }
            if (serial.Length > 20)
            {
                serial = serial.Substring(0, 20);
            }

            if (!int.TryParse(TxtCycles.Text.Trim(), out int cycles) || cycles <= 0)
            {
                MessageBox.Show("Bitte eine gültige Zyklenzahl eingeben (z.B. 10, 50, 100, 1000).", "Eingabefehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                TxtCycles.Focus();
                return;
            }

            Config.OperatorId = op;
            Config.SerialNumber = serial;
            Config.Cycles = cycles;
            Config.Remarks = string.IsNullOrWhiteSpace(TxtRemarks.Text) ? "keine" : TxtRemarks.Text.Trim();
            Config.TestType = (RbModeB.IsChecked == true) ? MimotTestType.BestueckenTestB : MimotTestType.DauertestHubTestA;

            Config.Checklist.Clear();
            Config.Checklist.Add(new MimotChecklistItem("Zahnstange gefettet, Kugelführung geölt", Chk1.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Silikon auf Stecker Druckmotor", Chk2.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Hebel des Druckmotors freigängig", Chk3.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Kabel der Lichtschranke fest fixiert", Chk4.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Sensor oben richtig positioniert und fest", Chk5.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Z-Motor axial Spiel ok", Chk6.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Zahnstange hat wenig Spiel und ist leichtgängig", Chk7.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Oberer Endanschlag bei der Kugelführung", Chk8.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Kabel von Spannung und Encoder getrennt verlegt", Chk9.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Schlauchführung befestigt", Chk10.IsChecked == true));
            Config.Checklist.Add(new MimotChecklistItem("Allgemeinzustand", Chk11.IsChecked == true));

            SaveLastSettings();
            DialogResult = true;
            Close();
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }
    }
}
