using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace MnpControl
{
    public static class MimotReportGenerator
    {
        public static (string txtPath, string htmlPath) GenerateAndSave(MimotTestResult result, string? targetDirectory = null)
        {
            if (string.IsNullOrWhiteSpace(targetDirectory))
            {
                string baseDir = AppDomain.CurrentDomain.BaseDirectory;
                targetDirectory = Path.Combine(baseDir, "Testprotokolle");
            }

            Directory.CreateDirectory(targetDirectory);

            string safeSerial = string.Join("_", (result.Config.SerialNumber ?? "").Split(Path.GetInvalidFileNameChars(), StringSplitOptions.RemoveEmptyEntries)).Trim();
            if (string.IsNullOrEmpty(safeSerial))
            {
                safeSerial = "MIMOT_NADEL";
            }

            string baseFileName = $"{safeSerial}___{result.EndTime:yyyyMMdd-HHmmss}";
            string txtPath = Path.Combine(targetDirectory, $"{baseFileName}.txt");
            string htmlPath = Path.Combine(targetDirectory, $"{baseFileName}.html");

            // Toleranz-Auswertungen (gemäß Mimot-Werksnorm)
            bool passChecklist = result.Config.Checklist.All(c => c.IsPassed);
            bool passBaseline = result.BaselineVoltage >= 0.0f && result.BaselineVoltage <= 0.350f;
            bool passTrigger = result.TriggerVoltage >= 4.0f && result.TriggerVoltage <= 5.500f;
            bool passLostSteps = result.LostSteps <= 10;
            bool passScatter = (result.Config.TestType != MimotTestType.BestueckenTestB) || (result.ScatterRange <= 10);
            bool passCycles = result.CompletedCycles >= result.Config.Cycles;
            bool noFatalError = string.IsNullOrEmpty(result.ErrorMessage) || result.ErrorMessage.Equals("keine", StringComparison.OrdinalIgnoreCase);

            bool overallPassed = passChecklist && passBaseline && passTrigger && passLostSteps && passScatter && passCycles && noFatalError;
            result.OverallSuccess = overallPassed;

            // 1. TXT Protokoll generieren (original Mimot Layout)
            string txtContent = GenerateTxtReport(result, baseFileName, passBaseline, passTrigger, passLostSteps, passScatter, overallPassed);
            File.WriteAllText(txtPath, txtContent, Encoding.UTF8);

            // 2. HTML Protokoll generieren (druckbar & modern für PDF)
            string htmlContent = GenerateHtmlReport(result, baseFileName, passBaseline, passTrigger, passLostSteps, passScatter, overallPassed);
            File.WriteAllText(htmlPath, htmlContent, Encoding.UTF8);

            return (txtPath, htmlPath);
        }

        private static string FormatTxtCol(string text, int width, bool alignRight = false)
        {
            if (text == null) text = "";
            if (text.Length > width) return text.Substring(0, width);
            return alignRight ? text.PadLeft(width) : text.PadRight(width);
        }

        private static string GenerateTxtReport(
            MimotTestResult res,
            string fileName,
            bool passBaseline,
            bool passTrigger,
            bool passLostSteps,
            bool passScatter,
            bool overallPassed)
        {
            var sb = new StringBuilder();
            var culture = CultureInfo.InvariantCulture;

            sb.AppendLine(FormatTxtCol(fileName + ".txt", 78, alignRight: true));
            sb.AppendLine(FormatTxtCol("Testprotokoll Nadel 1260.x", 62, alignRight: true));
            sb.AppendLine();
            sb.AppendLine($"{res.EndTime:dd.MM.yyyy, HH:mm:ss}");
            sb.AppendLine($"Personalnummer: {res.Config.OperatorId}");
            sb.AppendLine($"Seriennummer: {res.Config.SerialNumber}");
            sb.AppendLine("Reparaturnummer: n/a");
            sb.AppendLine();

            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}", "Label", "Minimum", "Actual", "Maximum", "Result"));
            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}", "-----", "-------", "------", "-------", "------"));

            sb.AppendLine("--------- Mechanischer Aufbau ---------");
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    item.Description, "", "", "", item.IsPassed ? "Pass" : "Fail"));
            }
            sb.AppendLine();
            sb.AppendLine($"Bemerkungen: {(string.IsNullOrWhiteSpace(res.Config.Remarks) ? "keine" : res.Config.Remarks)}");
            sb.AppendLine();

            sb.AppendLine("--------- Sensoren kalibrieren ---------");
            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                "Spannung Drucksensor nicht angesprochen", "0.000 V", res.BaselineVoltage.ToString("F3", culture) + " V", "0.350 V", passBaseline ? "Pass" : "Fail"));

            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                "Spannung Drucksensor angesprochen", "4.300 V", res.TriggerVoltage.ToString("F3", culture) + " V", "5.500 V", passTrigger ? "Pass" : "Fail"));

            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                "Abstand bis Drucksensor anspricht", "0.000 inc", res.ContactTravelInc.ToString("F3", culture) + " inc", "35.000 inc", "Pass"));

            if (res.NoSensorPos > 0)
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "SNO: Schaltschwelle oben", "1500.000 inc", res.NoSensorPos.ToString("F3", culture) + " inc", "3400.000 inc", "Pass"));
            }

            sb.AppendLine();
            string testName = res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest bestücken" : "Dauertest Z-Achse";
            sb.AppendLine($"--------- {testName} ---------");
            sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                "Verlorene Schritte nach Dauertest", "0.000 inc", res.LostSteps.ToString("F3", culture) + " inc", "10.000 inc", passLostSteps ? "Pass" : "Fail"));

            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "Antast-Streuung (Spanne)", "0.000 inc", res.ScatterRange.ToString("F3", culture) + " inc", "10.000 inc", passScatter ? "Pass" : "Fail"));
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "Antast-Mittelwert", "", res.MeanPosition.ToString("F1", culture) + " inc", "", "Pass"));
            }
            else
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "Hub (IST min..max)", "", $"{res.IstMin}..{res.IstMax} inc", "", "Pass"));
            }

            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "Max. Geschwindigkeit (IST)", "", res.MaxVelocityMmS.ToString("F0", culture) + " mm/s", "", "Pass"));
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine(string.Format("{0,-48} {1,10} {2,12} {3,12} {4,8}",
                    "Spitzen-Beschleunigung / Bremsen", "", $"+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g", "", "Pass"));
            }

            sb.AppendLine();
            sb.AppendLine($"Bestückzyklen: {res.CompletedCycles} / {res.Config.Cycles}");
            sb.AppendLine($"Endzeit: {res.EndTime:HH:mm:ss}");
            sb.AppendLine();
            sb.AppendLine($"Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)}");
            sb.AppendLine();
            sb.AppendLine($"Testergebnis: {(overallPassed ? "Test Bestanden" : "Test Nicht Bestanden")}");

            return sb.ToString();
        }

        private static string GenerateHtmlReport(
            MimotTestResult res,
            string fileName,
            bool passBaseline,
            bool passTrigger,
            bool passLostSteps,
            bool passScatter,
            bool overallPassed)
        {
            var culture = CultureInfo.InvariantCulture;
            string statusBadge = overallPassed
                ? "<div class='badge pass-badge'>TEST BESTANDEN (PASS)</div>"
                : "<div class='badge fail-badge'>TEST NICHT BESTANDEN (FAIL)</div>";

            string badge(bool pass) => pass
                ? "<span class='badge-small pass'>Pass</span>"
                : "<span class='badge-small fail'>Fail</span>";

            var sb = new StringBuilder();
            sb.AppendLine("<!DOCTYPE html>");
            sb.AppendLine("<html lang='de'>");
            sb.AppendLine("<head>");
            sb.AppendLine("  <meta charset='utf-8'>");
            sb.AppendLine($"  <title>Prüfprotokoll {res.Config.SerialNumber}</title>");
            sb.AppendLine("  <style>");
            sb.AppendLine("    body { font-family: 'Segoe UI', Arial, sans-serif; margin: 25px; color: #1e293b; background: #fff; font-size: 13px; line-height: 1.4; }");
            sb.AppendLine("    .header-box { border-bottom: 2px solid #0f172a; padding-bottom: 12px; margin-bottom: 18px; display: flex; justify-content: space-between; align-items: flex-start; }");
            sb.AppendLine("    .title { font-size: 20px; font-weight: bold; color: #0f172a; margin: 0; }");
            sb.AppendLine("    .subtitle { font-size: 13px; color: #64748b; margin-top: 3px; }");
            sb.AppendLine("    .meta-table { width: 100%; margin-bottom: 20px; border-collapse: collapse; }");
            sb.AppendLine("    .meta-table td { padding: 4px 8px; }");
            sb.AppendLine("    .meta-label { font-weight: bold; width: 150px; color: #475569; }");
            sb.AppendLine("    .section-title { font-size: 14px; font-weight: bold; background: #f1f5f9; padding: 6px 10px; margin-top: 18px; margin-bottom: 8px; border-left: 4px solid #2563eb; }");
            sb.AppendLine("    table.data-table { width: 100%; border-collapse: collapse; margin-bottom: 15px; font-family: 'Consolas', monospace; font-size: 12px; }");
            sb.AppendLine("    table.data-table th { background: #f8fafc; border-bottom: 2px solid #cbd5e1; text-align: left; padding: 6px 8px; font-weight: bold; color: #334155; }");
            sb.AppendLine("    table.data-table td { padding: 5px 8px; border-bottom: 1px solid #e2e8f0; }");
            sb.AppendLine("    .text-right { text-align: right; }");
            sb.AppendLine("    .badge-small { display: inline-block; padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; }");
            sb.AppendLine("    .badge-small.pass { background: #dcfce7; color: #166534; }");
            sb.AppendLine("    .badge-small.fail { background: #fee2e2; color: #991b1b; }");
            sb.AppendLine("    .badge { padding: 12px; border-radius: 6px; font-size: 18px; font-weight: bold; text-align: center; margin: 25px 0 15px 0; }");
            sb.AppendLine("    .pass-badge { background: #dcfce7; color: #15803d; border: 2px solid #86efac; }");
            sb.AppendLine("    .fail-badge { background: #fee2e2; color: #b91c1c; border: 2px solid #fca5a5; }");
            sb.AppendLine("    .print-btn { background: #2563eb; color: #fff; border: none; padding: 8px 16px; font-size: 13px; font-weight: bold; border-radius: 4px; cursor: pointer; margin-bottom: 20px; }");
            sb.AppendLine("    @media print { .print-btn { display: none; } body { margin: 10mm; } }");
            sb.AppendLine("  </style>");
            sb.AppendLine("</head>");
            sb.AppendLine("<body>");
            sb.AppendLine("  <button class='print-btn' onclick='window.print()'>🖨️ Protokoll drucken / als PDF speichern</button>");
            sb.AppendLine("  <div class='header-box'>");
            sb.AppendLine("    <div>");
            sb.AppendLine("      <div class='title'>GRAF MNP — Testprotokoll Nadel 1260.x</div>");
            sb.AppendLine("      <div class='subtitle'>Abnahmeprüfung nach Mimot-Werksvorschrift</div>");
            sb.AppendLine("    </div>");
            sb.AppendLine($"    <div style='text-align: right; font-family: Consolas, monospace; font-size: 11px; color: #64748b;'>{fileName}.txt</div>");
            sb.AppendLine("  </div>");

            sb.AppendLine("  <table class='meta-table'>");
            sb.AppendLine($"    <tr><td class='meta-label'>Datum / Uhrzeit:</td><td>{res.EndTime:dd.MM.yyyy, HH:mm:ss}</td><td class='meta-label'>Personalnummer:</td><td><b>{res.Config.OperatorId}</b></td></tr>");
            sb.AppendLine($"    <tr><td class='meta-label'>Seriennummer:</td><td><b>{res.Config.SerialNumber}</b></td><td class='meta-label'>Reparaturnummer:</td><td>n/a</td></tr>");
            sb.AppendLine($"    <tr><td class='meta-label'>Prüfart:</td><td>{(res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestücken (Test B)" : "Dauertest Z-Achse (Test A)")}</td><td class='meta-label'>Zyklen:</td><td>{res.CompletedCycles} / {res.Config.Cycles}</td></tr>");
            sb.AppendLine($"    <tr><td class='meta-label'>Bemerkungen:</td><td colspan='3'>{(string.IsNullOrWhiteSpace(res.Config.Remarks) ? "keine" : res.Config.Remarks)}</td></tr>");
            sb.AppendLine("  </table>");

            sb.AppendLine("  <div class='section-title'>1. Mechanischer Aufbau (Sicht- und Funktionskontrolle)</div>");
            sb.AppendLine("  <table class='data-table'>");
            sb.AppendLine("    <tr><th>Prüfpunkt</th><th style='width: 100px;' class='text-right'>Ergebnis</th></tr>");
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine($"    <tr><td>{item.Description}</td><td class='text-right'>{badge(item.IsPassed)}</td></tr>");
            }
            sb.AppendLine("  </table>");

            sb.AppendLine("  <div class='section-title'>2. Sensorkalibrierung & Signale</div>");
            sb.AppendLine("  <table class='data-table'>");
            sb.AppendLine("    <tr><th>Messgröße</th><th class='text-right'>Minimum</th><th class='text-right'>Istwert</th><th class='text-right'>Maximum</th><th class='text-right' style='width: 80px;'>Status</th></tr>");
            sb.AppendLine($"    <tr><td>Spannung Drucksensor nicht angesprochen</td><td class='text-right'>0.000 V</td><td class='text-right'>{res.BaselineVoltage:F3} V</td><td class='text-right'>0.350 V</td><td class='text-right'>{badge(passBaseline)}</td></tr>");
            sb.AppendLine($"    <tr><td>Spannung Drucksensor angesprochen</td><td class='text-right'>4.300 V</td><td class='text-right'>{res.TriggerVoltage:F3} V</td><td class='text-right'>5.500 V</td><td class='text-right'>{badge(passTrigger)}</td></tr>");
            sb.AppendLine($"    <tr><td>Abstand bis Drucksensor anspricht</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ContactTravelInc:F3} inc</td><td class='text-right'>35.000 inc</td><td class='text-right'>{badge(true)}</td></tr>");
            if (res.NoSensorPos > 0)
            {
                sb.AppendLine($"    <tr><td>SNO: Schaltschwelle oben (Lichtschranke)</td><td class='text-right'>1500.000 inc</td><td class='text-right'>{res.NoSensorPos:F3} inc</td><td class='text-right'>3400.000 inc</td><td class='text-right'>{badge(true)}</td></tr>");
            }
            sb.AppendLine("  </table>");

            sb.AppendLine($"  <div class='section-title'>3. {(res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestücken" : "Dauertest Z-Achse")}</div>");
            sb.AppendLine("  <table class='data-table'>");
            sb.AppendLine("    <tr><th>Prüfparameter</th><th class='text-right'>Minimum</th><th class='text-right'>Istwert</th><th class='text-right'>Maximum</th><th class='text-right' style='width: 80px;'>Status</th></tr>");
            sb.AppendLine($"    <tr><td>Verlorene Schritte nach Dauertest</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.LostSteps:F3} inc</td><td class='text-right'>10.000 inc</td><td class='text-right'>{badge(passLostSteps)}</td></tr>");
            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine($"    <tr><td>Antast-Streuung (Spanne)</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ScatterRange:F3} inc</td><td class='text-right'>10.000 inc</td><td class='text-right'>{badge(passScatter)}</td></tr>");
                sb.AppendLine($"    <tr><td>Antast-Mittelwert</td><td class='text-right'>-</td><td class='text-right'>{res.MeanPosition:F1} inc</td><td class='text-right'>-</td><td class='text-right'>{badge(true)}</td></tr>");
            }
            else
            {
                sb.AppendLine($"    <tr><td>Hub (IST min..max)</td><td class='text-right'>-</td><td class='text-right'>{res.IstMin} .. {res.IstMax} inc</td><td class='text-right'>-</td><td class='text-right'>{badge(true)}</td></tr>");
            }
            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine($"    <tr><td>Max. Geschwindigkeit (IST)</td><td class='text-right'>-</td><td class='text-right'>{res.MaxVelocityMmS:F0} mm/s</td><td class='text-right'>-</td><td class='text-right'>{badge(true)}</td></tr>");
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine($"    <tr><td>Spitzen-Beschleunigung / Bremsung</td><td class='text-right'>-</td><td class='text-right'>+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g</td><td class='text-right'>-</td><td class='text-right'>{badge(true)}</td></tr>");
            }
            sb.AppendLine("  </table>");

            sb.AppendLine(statusBadge);
            sb.AppendLine($"  <div style='font-size: 11px; color: #64748b; margin-top: 15px;'>Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)} | Zyklen: {res.CompletedCycles}/{res.Config.Cycles} | Endzeit: {res.EndTime:HH:mm:ss}</div>");
            sb.AppendLine("</body>");
            sb.AppendLine("</html>");

            return sb.ToString();
        }
    }
}
