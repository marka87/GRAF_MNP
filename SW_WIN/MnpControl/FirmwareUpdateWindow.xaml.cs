using System.IO;
using System.Text;
using System.Windows;
using Microsoft.Win32;

namespace MnpControl
{
    public partial class FirmwareUpdateWindow : Window
    {
        private readonly MainWindow _main;
        private byte[]? _firmwareBytes;
        private uint _firmwareCrc32;
        private bool _isUpdating = false;

        public FirmwareUpdateWindow(MainWindow main)
        {
            InitializeComponent();
            _main = main;

            // Auto-detect newly built binary if available
            string defaultBin = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"..\..\..\..\..\Software\GRAF_MNPA\Debug\GRAF_MNPA.bin");
            if (File.Exists(defaultBin))
            {
                LoadFirmwareFile(Path.GetFullPath(defaultBin));
            }
        }

        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog dlg = new OpenFileDialog
            {
                Title = "Firmware-Binärdatei auswählen",
                Filter = "STM32 Binary (*.bin)|*.bin|Alle Dateien (*.*)|*.*",
                InitialDirectory = AppDomain.CurrentDomain.BaseDirectory
            };

            if (dlg.ShowDialog(this) == true)
            {
                LoadFirmwareFile(dlg.FileName);
            }
        }

        private void LoadFirmwareFile(string path)
        {
            try
            {
                byte[] bytes = File.ReadAllBytes(path);
                if (bytes.Length == 0 || bytes.Length > (224 * 1024))
                {
                    MessageBox.Show("Die Datei ist ungültig oder größer als 224 KB.", "Dateifehler", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                _firmwareBytes = bytes;
                _firmwareCrc32 = CalculateCrc32(bytes);

                TxtFilePath.Text = path;
                TxtFileInfo.Text = $"Größe: {bytes.Length:N0} Bytes ({bytes.Length / 1024.0:F1} KB) | CRC32: 0x{_firmwareCrc32:X8}";
                BtnStartUpdate.IsEnabled = true;
                TxtStatus.Text = "Bereit zum Flashen";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Fehler beim Laden der Datei:\n{ex.Message}", "Fehler", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private async void BtnStartUpdate_Click(object sender, RoutedEventArgs e)
        {
            if (_firmwareBytes == null || _firmwareBytes.Length == 0)
            {
                MessageBox.Show("Bitte wählen Sie zuerst eine Firmware-Datei aus.", "Hinweis", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            if (_isUpdating) return;

            var confirm = MessageBox.Show(
                $"Möchten Sie das Firmware-Update ({_firmwareBytes.Length:N0} Bytes) jetzt starten?\n\n" +
                "Der STM32 wechselt in den Bootloader-Modus und flasht die neue Version.",
                "Firmware-Update bestätigen",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);

            if (confirm != MessageBoxResult.Yes) return;

            _isUpdating = true;
            BtnStartUpdate.IsEnabled = false;
            BtnBrowse.IsEnabled = false;
            BtnClose.IsEnabled = false;

            try
            {
                await FlashFirmwareAsync(_firmwareBytes, _firmwareCrc32);
            }
            catch (Exception ex)
            {
                TxtStatus.Text = "Fehler beim Update: " + ex.Message;
                MessageBox.Show($"Firmware-Update fehlgeschlagen:\n{ex.Message}", "Update-Fehler", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                _isUpdating = false;
                BtnStartUpdate.IsEnabled = true;
                BtnBrowse.IsEnabled = true;
                BtnClose.IsEnabled = true;
            }
        }

        private async Task FlashFirmwareAsync(byte[] binData, uint crc32)
        {
            // Message receiver buffer
            TaskCompletionSource<string>? pendingResponse = null;
            string? filterPrefix = null;

            Action<string> onLineReceived = line =>
            {
                if (pendingResponse != null && (filterPrefix == null || line.StartsWith(filterPrefix, StringComparison.Ordinal)))
                {
                    pendingResponse.TrySetResult(line);
                }
            };

            _main.OnSerialLineReceived += onLineReceived;

            try
            {
                async Task<string> SendAndWaitAsync(string cmd, string expectedPrefix, int timeoutMs)
                {
                    filterPrefix = expectedPrefix;
                    pendingResponse = new TaskCompletionSource<string>();

                    _main.SendCommand(cmd);

                    using CancellationTokenSource cts = new CancellationTokenSource(timeoutMs);
                    cts.Token.Register(() => pendingResponse.TrySetCanceled());

                    try
                    {
                        return await pendingResponse.Task;
                    }
                    catch (TaskCanceledException)
                    {
                        throw new TimeoutException($"Zeitüberschreitung beim Warten auf '{expectedPrefix}' (nach {timeoutMs} ms)");
                    }
                }

                // 1. Enter Bootloader
                UpdateProgress(5, "Verbindung mit Bootloader herstellen...");
                string readyResp = "";

                // Send ENTER_BOOTLOADER and wait for READY
                try
                {
                    readyResp = await SendAndWaitAsync("CMD:ENTER_BOOTLOADER", "BOOTLOADER:READY", 2500);
                }
                catch (TimeoutException)
                {
                    // If already in bootloader, try PING
                    UpdateProgress(10, "Bootloader PING...");
                    readyResp = await SendAndWaitAsync("CMD:BOOT_PING", "BOOTLOADER:PONG", 2000);
                }

                await Task.Delay(100);

                // 2. Erase Application Flash (Sectors 1..4)
                UpdateProgress(15, $"Lösche Flash-Sektoren ({binData.Length:N0} Bytes)...");
                string eraseResp = await SendAndWaitAsync($"CMD:BOOT_ERASE:size={binData.Length}", "BOOTLOADER:ERASE", 8000);
                if (!eraseResp.Contains("ERASE_OK"))
                {
                    throw new InvalidOperationException("Flash-Löschen fehlgeschlagen: " + eraseResp);
                }

                // 3. Send Chunks à 512 Bytes
                int chunkSize = 512;
                int totalChunks = (binData.Length + chunkSize - 1) / chunkSize;
                uint offset = 0;

                for (int seq = 0; seq < totalChunks; seq++)
                {
                    int currentLen = Math.Min(chunkSize, binData.Length - (int)offset);
                    byte[] chunk = new byte[currentLen];
                    Buffer.BlockCopy(binData, (int)offset, chunk, 0, currentLen);

                    string hexData = Convert.ToHexString(chunk);
                    ushort chunkCrc16 = CalculateCrc16(chunk);

                    int progress = 20 + (int)(((float)(seq + 1) / totalChunks) * 65.0f);
                    UpdateProgress(progress, $"Übertrage Block {seq + 1} / {totalChunks} ({offset + (uint)currentLen:N0} B)...");

                    string ackResp = "";
                    int retries = 0;
                    while (retries < 3)
                    {
                        try
                        {
                            ackResp = await SendAndWaitAsync(
                                $"CMD:BOOT_CHUNK:{seq}:{offset}:{currentLen}:{hexData}:{chunkCrc16:X4}",
                                $"BOOTLOADER:ACK:{seq}",
                                3000);
                            break;
                        }
                        catch (TimeoutException)
                        {
                            retries++;
                            if (retries >= 3) throw;
                        }
                    }

                    offset += (uint)currentLen;
                }

                // 4. Verify full binary CRC32
                UpdateProgress(90, "Verifiziere Flash-Prüfsumme (CRC32)...");
                string verifyResp = await SendAndWaitAsync($"CMD:BOOT_VERIFY:size={binData.Length}:crc32={crc32:X8}", "BOOTLOADER:VERIFY", 4000);
                if (!verifyResp.Contains("VERIFY_OK"))
                {
                    throw new InvalidOperationException("Flash-Verifikation fehlgeschlagen: " + verifyResp);
                }

                // 5. Launch Application
                UpdateProgress(98, "Starte neue Firmware...");
                await SendAndWaitAsync("CMD:BOOT_RUN", "BOOTLOADER:RUNNING", 2000);

                UpdateProgress(100, "Firmware erfolgreich geflasht! STM32 gestartet.");
                MessageBox.Show(
                    "Das Firmware-Update wurde erfolgreich durchgeführt!\nDie neue Applikation läuft jetzt.",
                    "Erfolg",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information);
            }
            finally
            {
                _main.OnSerialLineReceived -= onLineReceived;
            }
        }

        private void UpdateProgress(int percent, string statusText)
        {
            Dispatcher.Invoke(() =>
            {
                PrgUpdate.Value = percent;
                TxtProgressPercent.Text = $"{percent} %";
                TxtStatus.Text = statusText;
            });
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        public static uint CalculateCrc32(byte[] data)
        {
            uint crc = 0xFFFFFFFFU;
            for (int i = 0; i < data.Length; i++)
            {
                crc ^= data[i];
                for (int bit = 0; bit < 8; bit++)
                {
                    if ((crc & 1U) != 0)
                        crc = (crc >> 1) ^ 0xEDB88320U;
                    else
                        crc >>= 1;
                }
            }
            return ~crc;
        }

        public static ushort CalculateCrc16(byte[] data)
        {
            ushort crc = 0xFFFF;
            for (int i = 0; i < data.Length; i++)
            {
                crc ^= data[i];
                for (int bit = 0; bit < 8; bit++)
                {
                    if ((crc & 1) != 0)
                        crc = (ushort)((crc >> 1) ^ 0xA001);
                    else
                        crc >>= 1;
                }
            }
            return crc;
        }
    }
}
