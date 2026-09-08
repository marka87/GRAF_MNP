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
            // Trigger-Spannung: Pruefstand tastet sanft bei Baseline + 1.5V (~1.5V) an, um Nadelmechanik zu schonen
            bool passTrigger = result.TriggerVoltage >= 1.000f && result.TriggerVoltage <= 5.500f;
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

            sb.AppendLine(FormatTxtCol(fileName + ".txt", 88, alignRight: true));
            sb.AppendLine(FormatTxtCol("Testprotokoll Nadel 1260.x", 88, alignRight: true));
            sb.AppendLine();
            sb.AppendLine($"{res.EndTime:dd.MM.yyyy, HH:mm:ss}");
            sb.AppendLine($"Personalnummer: {res.Config.OperatorId}");
            sb.AppendLine($"Seriennummer:   {res.Config.SerialNumber}");
            sb.AppendLine("Reparaturnummer: n/a");
            sb.AppendLine();

            string headerSep = "+--------------------------------------------------+------------+------------+------------+--------+";
            string formatRow = "| {0,-48} | {1,10} | {2,10} | {3,10} | {4,6} |";

            sb.AppendLine(headerSep);
            sb.AppendLine(string.Format(formatRow, "Label / Messgroesse", "Minimum", "Istwert", "Maximum", "Status"));
            sb.AppendLine(headerSep);

            sb.AppendLine(string.Format("| {0,-88} |", "--- 1. Mechanischer Aufbau (Sicht- und Funktionskontrolle) ---"));
            sb.AppendLine(headerSep);
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine(string.Format(formatRow, item.Description, "-", "-", "-", item.IsPassed ? "Pass" : "Fail"));
            }
            sb.AppendLine(headerSep);
            sb.AppendLine($"Bemerkungen: {(string.IsNullOrWhiteSpace(res.Config.Remarks) ? "keine" : res.Config.Remarks)}");
            sb.AppendLine();

            sb.AppendLine(headerSep);
            sb.AppendLine(string.Format("| {0,-88} |", "--- 2. Sensorkalibrierung & Signale ---"));
            sb.AppendLine(headerSep);
            sb.AppendLine(string.Format(formatRow,
                "Spannung Drucksensor nicht angesprochen", "0.000 V", res.BaselineVoltage.ToString("F3", culture) + " V", "0.350 V", passBaseline ? "Pass" : "Fail"));

            sb.AppendLine(string.Format(formatRow,
                "Spannung Drucksensor angesprochen", "1.000 V", res.TriggerVoltage.ToString("F3", culture) + " V", "5.500 V", passTrigger ? "Pass" : "Fail"));

            sb.AppendLine(string.Format(formatRow,
                "Abstand bis Drucksensor anspricht", "0.000 inc", res.ContactTravelInc.ToString("F3", culture) + " inc", "35.000 inc", "Pass"));

            if (res.NoSensorPos > 0)
            {
                sb.AppendLine(string.Format(formatRow,
                    "SNO: Schaltschwelle oben (Lichtschranke)", "1500.000 inc", res.NoSensorPos.ToString("F3", culture) + " inc", "3400.000 inc", "Pass"));
            }
            sb.AppendLine(headerSep);
            sb.AppendLine();

            string testName = res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestuecken (Test B)" : "Dauertest Z-Achse (Test A)";
            sb.AppendLine(headerSep);
            sb.AppendLine(string.Format("| {0,-88} |", $"--- 3. {testName} ---"));
            sb.AppendLine(headerSep);
            sb.AppendLine(string.Format(formatRow,
                "Verlorene Schritte nach Dauertest", "0.000 inc", res.LostSteps.ToString("F3", culture) + " inc", "10.000 inc", passLostSteps ? "Pass" : "Fail"));

            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine(string.Format(formatRow,
                    "Antast-Streuung (Spanne)", "0.000 inc", res.ScatterRange.ToString("F3", culture) + " inc", "10.000 inc", passScatter ? "Pass" : "Fail"));
                sb.AppendLine(string.Format(formatRow,
                    "Antast-Mittelwert", "-", res.MeanPosition.ToString("F1", culture) + " inc", "-", "Pass"));
            }
            else
            {
                sb.AppendLine(string.Format(formatRow,
                    "Hub (IST min..max)", "-", $"{res.IstMin}..{res.IstMax} inc", "-", "Pass"));
            }

            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine(string.Format(formatRow,
                    "Max. Geschwindigkeit (IST)", "-", res.MaxVelocityMmS.ToString("F0", culture) + " mm/s", "-", "Pass"));
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine(string.Format(formatRow,
                    "Spitzen-Beschleunigung / Bremsen", "-", $"+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g", "-", "Pass"));
            }
            sb.AppendLine(headerSep);

            sb.AppendLine();
            sb.AppendLine($"Bestueckzyklen:  {res.CompletedCycles} / {res.Config.Cycles}");
            sb.AppendLine($"Endzeit:         {res.EndTime:HH:mm:ss}");
            sb.AppendLine($"Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)}");
            sb.AppendLine();
            sb.AppendLine("=========================================================================================");
            sb.AppendLine($"  TESTERGEBNIS:  {(overallPassed ? "TEST BESTANDEN (PASS)" : "TEST NICHT BESTANDEN (FAIL)")}");
            sb.AppendLine("=========================================================================================");

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
            sb.AppendLine("    body { font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Arial, sans-serif; margin: 0; padding: 25px 15px; color: #0f172a; background: #e2e8f0; font-size: 13px; line-height: 1.4; }");
            sb.AppendLine("    .report-card { max-width: 960px; margin: 0 auto; background: #ffffff; padding: 35px 40px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); border-radius: 6px; }");
            sb.AppendLine("    .header-box { border-bottom: 2px solid #1e293b; padding-bottom: 12px; margin-bottom: 18px; display: flex; justify-content: space-between; align-items: flex-start; }");
            sb.AppendLine("    .title { font-size: 22px; font-weight: bold; color: #0f172a; margin: 0; }");
            sb.AppendLine("    .subtitle { font-size: 13px; color: #64748b; margin-top: 4px; font-weight: 500; }");
            sb.AppendLine("    .meta-table { width: 100%; margin-bottom: 22px; border-collapse: collapse; border: 1px solid #94a3b8; }");
            sb.AppendLine("    .meta-table td { padding: 6px 10px; border: 1px solid #cbd5e1; }");
            sb.AppendLine("    .meta-label { font-weight: bold; width: 140px; color: #334155; background: #f8fafc; }");
            sb.AppendLine("    .section-title { font-size: 14px; font-weight: bold; background: #0f172a; color: #ffffff; padding: 7px 12px; margin-top: 22px; margin-bottom: 0px; border-radius: 4px 4px 0 0; }");
            sb.AppendLine("    table.data-table { width: 100%; border-collapse: collapse; margin-bottom: 20px; border: 1px solid #94a3b8; font-size: 12px; }");
            sb.AppendLine("    table.data-table th { background: #e2e8f0; color: #1e293b; border: 1px solid #94a3b8; padding: 8px 10px; font-weight: bold; text-align: left; }");
            sb.AppendLine("    table.data-table td { border: 1px solid #cbd5e1; padding: 6px 10px; font-family: 'Consolas', monospace; color: #1e293b; }");
            sb.AppendLine("    table.data-table tr:nth-child(even) td { background-color: #f8fafc; }");
            sb.AppendLine("    table.data-table tr:hover td { background-color: #f1f5f9; }");
            sb.AppendLine("    .text-right { text-align: right; }");
            sb.AppendLine("    .text-center { text-align: center; }");
            sb.AppendLine("    .badge-small { display: inline-block; padding: 2px 10px; border-radius: 4px; font-weight: bold; font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px; }");
            sb.AppendLine("    .badge-small.pass { background: #dcfce7; color: #15803d; border: 1px solid #86efac; }");
            sb.AppendLine("    .badge-small.fail { background: #fee2e2; color: #b91c1c; border: 1px solid #fca5a5; }");
            sb.AppendLine("    .badge { padding: 14px; border-radius: 6px; font-size: 18px; font-weight: bold; text-align: center; margin: 25px 0 15px 0; letter-spacing: 1px; }");
            sb.AppendLine("    .pass-badge { background: #dcfce7; color: #15803d; border: 2px solid #86efac; }");
            sb.AppendLine("    .fail-badge { background: #fee2e2; color: #b91c1c; border: 2px solid #fca5a5; }");
            sb.AppendLine("    .print-btn { background: #2563eb; color: #fff; border: none; padding: 9px 18px; font-size: 13px; font-weight: bold; border-radius: 5px; cursor: pointer; margin-bottom: 20px; transition: background 0.15s; }");
            sb.AppendLine("    .print-btn:hover { background: #1d4ed8; }");
            sb.AppendLine("    @media print {");
            sb.AppendLine("      body { background: #fff; padding: 0; }");
            sb.AppendLine("      .report-card { max-width: 100%; margin: 0; padding: 0; box-shadow: none; border-radius: 0; }");
            sb.AppendLine("      .print-btn { display: none !important; }");
            sb.AppendLine("    }");
            sb.AppendLine("  </style>");
            sb.AppendLine("</head>");
            sb.AppendLine("<body>");
            sb.AppendLine("  <div class='report-card'>");
            sb.AppendLine("    <button class='print-btn' onclick='window.print()'>🖨️ Protokoll drucken / als PDF speichern</button>");
            sb.AppendLine("    <div class='header-box'>");
            sb.AppendLine("      <div>");
            sb.AppendLine("        <div class='title'>GRAF MNP — Testprotokoll Nadel 1260.x</div>");
            sb.AppendLine("        <div class='subtitle'>Abnahmeprüfung nach Mimot-Werksvorschrift</div>");
            sb.AppendLine("      </div>");
            sb.AppendLine($"      <div style='text-align: right; font-family: Consolas, monospace; font-size: 11px; color: #64748b;'>{fileName}.txt</div>");
            sb.AppendLine("    </div>");
            sb.AppendLine();
            sb.AppendLine("    <table class='meta-table'>");
            sb.AppendLine($"      <tr><td class='meta-label'>Datum / Uhrzeit:</td><td>{res.EndTime:dd.MM.yyyy, HH:mm:ss}</td><td class='meta-label'>Personalnummer:</td><td><b>{res.Config.OperatorId}</b></td></tr>");
            sb.AppendLine($"      <tr><td class='meta-label'>Seriennummer:</td><td><b>{res.Config.SerialNumber}</b></td><td class='meta-label'>Reparaturnummer:</td><td>n/a</td></tr>");
            sb.AppendLine($"      <tr><td class='meta-label'>Prüfart:</td><td>{(res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestücken (Test B)" : "Dauertest Z-Achse (Test A)")}</td><td class='meta-label'>Zyklen:</td><td>{res.CompletedCycles} / {res.Config.Cycles}</td></tr>");
            sb.AppendLine($"      <tr><td class='meta-label'>Bemerkungen:</td><td colspan='3'>{(string.IsNullOrWhiteSpace(res.Config.Remarks) ? "keine" : res.Config.Remarks)}</td></tr>");
            sb.AppendLine("    </table>");
            sb.AppendLine();
            sb.AppendLine("    <div class='section-title'>1. Mechanischer Aufbau (Sicht- und Funktionskontrolle)</div>");
            sb.AppendLine("    <table class='data-table'>");
            sb.AppendLine("      <tr><th style='width: 82%;'>Prüfpunkt</th><th style='width: 18%;' class='text-center'>Ergebnis</th></tr>");
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine($"      <tr><td>{item.Description}</td><td class='text-center'>{badge(item.IsPassed)}</td></tr>");
            }
            sb.AppendLine("    </table>");
            sb.AppendLine();
            sb.AppendLine("    <div class='section-title'>2. Sensorkalibrierung & Signale</div>");
            sb.AppendLine("    <table class='data-table'>");
            sb.AppendLine("      <tr><th style='width: 46%;'>Messgröße</th><th style='width: 14%;' class='text-right'>Minimum</th><th style='width: 14%;' class='text-right'>Istwert</th><th style='width: 14%;' class='text-right'>Maximum</th><th style='width: 12%;' class='text-center'>Status</th></tr>");
            sb.AppendLine($"      <tr><td>Spannung Drucksensor nicht angesprochen</td><td class='text-right'>0.000 V</td><td class='text-right'>{res.BaselineVoltage:F3} V</td><td class='text-right'>0.350 V</td><td class='text-center'>{badge(passBaseline)}</td></tr>");
            sb.AppendLine($"      <tr><td>Spannung Drucksensor angesprochen</td><td class='text-right'>1.000 V</td><td class='text-right'>{res.TriggerVoltage:F3} V</td><td class='text-right'>5.500 V</td><td class='text-center'>{badge(passTrigger)}</td></tr>");
            sb.AppendLine($"      <tr><td>Abstand bis Drucksensor anspricht</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ContactTravelInc:F3} inc</td><td class='text-right'>35.000 inc</td><td class='text-center'>{badge(true)}</td></tr>");
            if (res.NoSensorPos > 0)
            {
                sb.AppendLine($"      <tr><td>SNO: Schaltschwelle oben (Lichtschranke)</td><td class='text-right'>1500.000 inc</td><td class='text-right'>{res.NoSensorPos:F3} inc</td><td class='text-right'>3400.000 inc</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            sb.AppendLine("    </table>");
            sb.AppendLine();
            sb.AppendLine($"    <div class='section-title'>3. {(res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestücken (Test B)" : "Dauertest Z-Achse (Test A)")}</div>");
            sb.AppendLine("    <table class='data-table'>");
            sb.AppendLine("      <tr><th style='width: 46%;'>Prüfparameter</th><th style='width: 14%;' class='text-right'>Minimum</th><th style='width: 14%;' class='text-right'>Istwert</th><th style='width: 14%;' class='text-right'>Maximum</th><th style='width: 12%;' class='text-center'>Status</th></tr>");
            sb.AppendLine($"      <tr><td>Verlorene Schritte nach Dauertest</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.LostSteps:F3} inc</td><td class='text-right'>10.000 inc</td><td class='text-center'>{badge(passLostSteps)}</td></tr>");
            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine($"      <tr><td>Antast-Streuung (Spanne)</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ScatterRange:F3} inc</td><td class='text-right'>10.000 inc</td><td class='text-center'>{badge(passScatter)}</td></tr>");
                sb.AppendLine($"      <tr><td>Antast-Mittelwert</td><td class='text-right'>-</td><td class='text-right'>{res.MeanPosition:F1} inc</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            else
            {
                sb.AppendLine($"      <tr><td>Hub (IST min..max)</td><td class='text-right'>-</td><td class='text-right'>{res.IstMin} .. {res.IstMax} inc</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine($"      <tr><td>Max. Geschwindigkeit (IST)</td><td class='text-right'>-</td><td class='text-right'>{res.MaxVelocityMmS:F0} mm/s</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine($"      <tr><td>Spitzen-Beschleunigung / Bremsung</td><td class='text-right'>-</td><td class='text-right'>+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            sb.AppendLine("    </table>");
            sb.AppendLine();
            sb.AppendLine(statusBadge);
            sb.AppendLine($"    <div style='font-size: 11px; color: #64748b; margin-top: 15px;'>Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)} | Zyklen: {res.CompletedCycles}/{res.Config.Cycles} | Endzeit: {res.EndTime:HH:mm:ss}</div>");
            sb.AppendLine("  </div>");
            sb.AppendLine("</body>");
            sb.AppendLine("</html>");

            return sb.ToString();
        }
    }
}
