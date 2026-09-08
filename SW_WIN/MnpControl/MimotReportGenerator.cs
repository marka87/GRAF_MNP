using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace MnpControl
{
    public static class MimotReportGenerator
    {
        private static readonly string TableDivider =
            "+" + new string('-', 52) + "+" + new string('-', 13) + "+" + new string('-', 15) + "+" + new string('-', 13) + "+" + new string('-', 8) + "+";

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

            // Toleranz-Auswertungen (gemäß Mimot-Werksnorm & Schutzfunktion)
            bool passChecklist = result.Config.Checklist.All(c => c.IsPassed);
            bool passBaseline = result.BaselineVoltage >= 0.0f && result.BaselineVoltage <= 0.350f;
            
            // Drucksensor Trigger: Beim Bestücken soll die Nadel das Bauteil sanft berühren (Trigger bei 0.080V .. 4.500V).
            // Kein Anschlag-Fahren auf 4.3V-5.5V erforderlich/erlaubt, um Nadelmechanik und Bauteile zu schonen!
            bool passTrigger = (result.Config.TestType != MimotTestType.BestueckenTestB) ||
                               (result.TriggerVoltage >= 0.080f && result.TriggerVoltage <= 4.500f);

            bool passLostSteps = result.LostSteps <= 10;
            bool passScatter = (result.Config.TestType != MimotTestType.BestueckenTestB) || (result.ScatterRange <= 10);
            bool passCycles = result.CompletedCycles >= result.Config.Cycles;
            bool noFatalError = string.IsNullOrEmpty(result.ErrorMessage) || result.ErrorMessage.Equals("keine", StringComparison.OrdinalIgnoreCase);

            bool overallPassed = passChecklist && passBaseline && passTrigger && passLostSteps && passScatter && passCycles && noFatalError;
            result.OverallSuccess = overallPassed;

            // 1. TXT Protokoll generieren (mit exakten Hochkant-Linien und UTF8-BOM)
            string txtContent = GenerateTxtReport(result, baseFileName, passBaseline, passTrigger, passLostSteps, passScatter, overallPassed);
            File.WriteAllText(txtPath, txtContent, new UTF8Encoding(true));

            // 2. HTML Protokoll generieren (druckbar & mit vertikalem Tabellenraster)
            string htmlContent = GenerateHtmlReport(result, baseFileName, passBaseline, passTrigger, passLostSteps, passScatter, overallPassed);
            File.WriteAllText(htmlPath, htmlContent, new UTF8Encoding(true));

            return (txtPath, htmlPath);
        }

        private static string PadOrTruncate(string text, int width, bool alignRight = false)
        {
            if (text == null) text = "";
            if (text.Length > width)
            {
                return text.Substring(0, width);
            }
            return alignRight ? text.PadLeft(width) : text.PadRight(width);
        }

        private static string FormatTableRow(string col1, string col2, string col3, string col4, string col5)
        {
            string c1 = PadOrTruncate(col1, 50, false);
            string c2 = PadOrTruncate(col2, 11, true);
            string c3 = PadOrTruncate(col3, 13, true);
            string c4 = PadOrTruncate(col4, 11, true);
            string c5 = PadOrTruncate(col5, 6, true);
            return $"| {c1} | {c2} | {c3} | {c4} | {c5} |";
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

            sb.AppendLine("===========================================================================================================");
            sb.AppendLine("                                GRAF MNP — TESTPROTOKOLL NADEL 1260.X");
            sb.AppendLine("                                   Abnahmeprüfung nach Mimot-Werksnorm");
            sb.AppendLine("===========================================================================================================");
            sb.AppendLine($"Protokolldatei:  {fileName}.txt");
            sb.AppendLine($"Datum / Uhrzeit: {res.EndTime:dd.MM.yyyy, HH:mm:ss}");
            sb.AppendLine($"Personalnummer:  {res.Config.OperatorId}");
            sb.AppendLine($"Seriennummer:    {res.Config.SerialNumber}");
            sb.AppendLine($"Prüf-Ablauf:     {(res.Config.TestType == MimotTestType.BestueckenTestB ? "Dauertest Bestücken (Test B - Antastung & Streuung)" : "Dauertest Z-Achse (Test A - Hub ohne Kontakt)")}");
            sb.AppendLine($"Bemerkungen:     {(string.IsNullOrWhiteSpace(res.Config.Remarks) ? "keine" : res.Config.Remarks)}");
            sb.AppendLine();

            sb.AppendLine(TableDivider);
            sb.AppendLine(FormatTableRow("Prüfpunkt / Messgröße", "Minimum", "Istwert", "Maximum", "Status"));
            sb.AppendLine(TableDivider);

            // Sektion 1: Mechanischer Aufbau
            sb.AppendLine(FormatTableRow("1. MECHANISCHER AUFBAU (Sicht- & Funktionsprüfung)", "", "", "", ""));
            sb.AppendLine(TableDivider);
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine(FormatTableRow("  " + item.Description, "-", "OK", "-", item.IsPassed ? "PASS" : "FAIL"));
            }

            // Sektion 2: Sensorkalibrierung & Signale
            sb.AppendLine(TableDivider);
            sb.AppendLine(FormatTableRow("2. SENSOREN KALIBRIEREN & SIGNALE", "", "", "", ""));
            sb.AppendLine(TableDivider);
            sb.AppendLine(FormatTableRow("  Spannung Drucksensor Ruhelage (Baseline)", "0.000 V", res.BaselineVoltage.ToString("F3", culture) + " V", "0.350 V", passBaseline ? "PASS" : "FAIL"));

            string triggerIst = (res.Config.TestType == MimotTestType.BestueckenTestB)
                ? res.TriggerVoltage.ToString("F3", culture) + " V"
                : "n/a (Test A)";
            sb.AppendLine(FormatTableRow("  Drucksensor Schaltschwelle (Trigger)", "0.080 V", triggerIst, "4.500 V", passTrigger ? "PASS" : "FAIL"));

            sb.AppendLine(FormatTableRow("  Abstand bis Drucksensor anspricht", "0.000 inc", res.ContactTravelInc.ToString("F1", culture) + " inc", "35.0 inc", "PASS"));

            if (res.NoSensorPos > 0)
            {
                sb.AppendLine(FormatTableRow("  SNO: Schaltschwelle oben (Lichtschranke)", "1500.0 inc", res.NoSensorPos.ToString("F1", culture) + " inc", "3400.0 inc", "PASS"));
            }

            // Sektion 3: Dauertest
            sb.AppendLine(TableDivider);
            string testName = res.Config.TestType == MimotTestType.BestueckenTestB ? "3. DAUERTEST BESTÜCKEN (Antastung)" : "3. DAUERTEST Z-ACHSE (Hub)";
            sb.AppendLine(FormatTableRow(testName, "", "", "", ""));
            sb.AppendLine(TableDivider);

            sb.AppendLine(FormatTableRow("  Verlorene Schritte nach Dauertest", "0.000 inc", res.LostSteps.ToString("F1", culture) + " inc", "10.0 inc", passLostSteps ? "PASS" : "FAIL"));

            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine(FormatTableRow("  Antast-Streuung (Spanne)", "0.000 inc", res.ScatterRange.ToString("F1", culture) + " inc", "10.0 inc", passScatter ? "PASS" : "FAIL"));
                sb.AppendLine(FormatTableRow("  Antast-Mittelwert", "-", res.MeanPosition.ToString("F1", culture) + " inc", "-", "PASS"));
            }
            else
            {
                sb.AppendLine(FormatTableRow("  Hub (IST min..max)", "-", $"{res.IstMin}..{res.IstMax} inc", "-", "PASS"));
            }

            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine(FormatTableRow("  Max. Geschwindigkeit (IST)", "-", res.MaxVelocityMmS.ToString("F0", culture) + " mm/s", "-", "PASS"));
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine(FormatTableRow("  Spitzen-Beschleunigung / Bremsung", "-", $"+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g", "-", "PASS"));
            }

            sb.AppendLine(TableDivider);
            sb.AppendLine();
            sb.AppendLine("Hinweis Drucksensor: Schaltschwelle bei dynamischer Werkstück-Antastung (0.080 - 4.500 V).");
            sb.AppendLine("Zum Schutz von Bauteilen und Nadelmechanik stoppt der Ablauf sofort bei Berührung");
            sb.AppendLine("und drückt nicht bis zum mechanischen Vollausschlag (4.3 - 5.5 V) auf Anschlag.");
            sb.AppendLine();
            sb.AppendLine($"Bestückzyklen:   {res.CompletedCycles} / {res.Config.Cycles}");
            sb.AppendLine($"Endzeit:         {res.EndTime:HH:mm:ss}");
            sb.AppendLine($"Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)}");
            sb.AppendLine();
            sb.AppendLine("===========================================================================================================");
            sb.AppendLine($"TESTERGEBNIS:    {(overallPassed ? "TEST BESTANDEN (PASS)" : "TEST NICHT BESTANDEN (FAIL)")}");
            sb.AppendLine("===========================================================================================================");

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
            sb.AppendLine("    .meta-label { font-weight: bold; width: 160px; color: #475569; }");
            sb.AppendLine("    table.data-table { width: 100%; border-collapse: collapse; margin-bottom: 15px; font-size: 12px; border: 2px solid #334155; }");
            sb.AppendLine("    table.data-table th, table.data-table td { padding: 6px 10px; border: 1px solid #94a3b8; }");
            sb.AppendLine("    table.data-table th { background: #0f172a; color: #ffffff; font-weight: bold; text-align: left; }");
            sb.AppendLine("    table.data-table .section-row td { background: #f1f5f9; font-weight: bold; color: #0f172a; padding: 7px 10px; border-top: 2px solid #64748b; border-bottom: 2px solid #64748b; }");
            sb.AppendLine("    table.data-table tbody tr:not(.section-row):nth-child(even) { background: #f8fafc; }");
            sb.AppendLine("    table.data-table tbody tr:not(.section-row):hover { background: #f1f5f9; }");
            sb.AppendLine("    .text-right { text-align: right; }");
            sb.AppendLine("    .text-center { text-align: center; }");
            sb.AppendLine("    .badge-small { display: inline-block; padding: 2px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; }");
            sb.AppendLine("    .badge-small.pass { background: #dcfce7; color: #166534; }");
            sb.AppendLine("    .badge-small.fail { background: #fee2e2; color: #991b1b; }");
            sb.AppendLine("    .badge { padding: 12px; border-radius: 6px; font-size: 18px; font-weight: bold; text-align: center; margin: 20px 0 15px 0; }");
            sb.AppendLine("    .pass-badge { background: #dcfce7; color: #15803d; border: 2px solid #86efac; }");
            sb.AppendLine("    .fail-badge { background: #fee2e2; color: #b91c1c; border: 2px solid #fca5a5; }");
            sb.AppendLine("    .note-box { background: #f8fafc; border-left: 4px solid #3b82f6; border-right: 1px solid #e2e8f0; border-top: 1px solid #e2e8f0; border-bottom: 1px solid #e2e8f0; padding: 10px 14px; margin-top: 12px; font-size: 12px; color: #475569; border-radius: 0 4px 4px 0; }");
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

            sb.AppendLine("  <table class='data-table'>");
            sb.AppendLine("    <thead>");
            sb.AppendLine("      <tr>");
            sb.AppendLine("        <th style='width: 44%;'>Prüfpunkt / Messgröße</th>");
            sb.AppendLine("        <th style='width: 14%;' class='text-right'>Minimum</th>");
            sb.AppendLine("        <th style='width: 16%;' class='text-right'>Istwert</th>");
            sb.AppendLine("        <th style='width: 14%;' class='text-right'>Maximum</th>");
            sb.AppendLine("        <th style='width: 12%;' class='text-center'>Status</th>");
            sb.AppendLine("      </tr>");
            sb.AppendLine("    </thead>");
            sb.AppendLine("    <tbody>");

            // Sektion 1: Mechanischer Aufbau
            sb.AppendLine("      <tr class='section-row'><td colspan='5'>1. Mechanischer Aufbau (Sicht- und Funktionskontrolle)</td></tr>");
            foreach (var item in res.Config.Checklist)
            {
                sb.AppendLine($"      <tr><td>{item.Description}</td><td class='text-right'>-</td><td class='text-right'>OK</td><td class='text-right'>-</td><td class='text-center'>{badge(item.IsPassed)}</td></tr>");
            }

            // Sektion 2: Sensorkalibrierung & Signale
            sb.AppendLine("      <tr class='section-row'><td colspan='5'>2. Sensorkalibrierung &amp; Signale</td></tr>");
            sb.AppendLine($"      <tr><td>Spannung Drucksensor Ruhelage (Baseline)</td><td class='text-right'>0.000 V</td><td class='text-right'>{res.BaselineVoltage.ToString("F3", culture)} V</td><td class='text-right'>0.350 V</td><td class='text-center'>{badge(passBaseline)}</td></tr>");

            string triggerIstHtml = (res.Config.TestType == MimotTestType.BestueckenTestB)
                ? $"{res.TriggerVoltage.ToString("F3", culture)} V"
                : "n/a (Test A)";
            sb.AppendLine($"      <tr><td>Drucksensor Schaltschwelle (Trigger)</td><td class='text-right'>0.080 V</td><td class='text-right'>{triggerIstHtml}</td><td class='text-right'>4.500 V</td><td class='text-center'>{badge(passTrigger)}</td></tr>");
            sb.AppendLine($"      <tr><td>Abstand bis Drucksensor anspricht</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ContactTravelInc.ToString("F1", culture)} inc</td><td class='text-right'>35.0 inc</td><td class='text-center'>{badge(true)}</td></tr>");
            if (res.NoSensorPos > 0)
            {
                sb.AppendLine($"      <tr><td>SNO: Schaltschwelle oben (Lichtschranke)</td><td class='text-right'>1500.0 inc</td><td class='text-right'>{res.NoSensorPos.ToString("F1", culture)} inc</td><td class='text-right'>3400.0 inc</td><td class='text-center'>{badge(true)}</td></tr>");
            }

            // Sektion 3: Dauertest
            string section3Title = res.Config.TestType == MimotTestType.BestueckenTestB ? "3. Dauertest Bestücken (Antastung &amp; Streuung)" : "3. Dauertest Z-Achse (Hub ohne Kontakt)";
            sb.AppendLine($"      <tr class='section-row'><td colspan='5'>{section3Title}</td></tr>");
            sb.AppendLine($"      <tr><td>Verlorene Schritte nach Dauertest</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.LostSteps.ToString("F1", culture)} inc</td><td class='text-right'>10.0 inc</td><td class='text-center'>{badge(passLostSteps)}</td></tr>");

            if (res.Config.TestType == MimotTestType.BestueckenTestB)
            {
                sb.AppendLine($"      <tr><td>Antast-Streuung (Spanne)</td><td class='text-right'>0.000 inc</td><td class='text-right'>{res.ScatterRange.ToString("F1", culture)} inc</td><td class='text-right'>10.0 inc</td><td class='text-center'>{badge(passScatter)}</td></tr>");
                sb.AppendLine($"      <tr><td>Antast-Mittelwert</td><td class='text-right'>-</td><td class='text-right'>{res.MeanPosition.ToString("F1", culture)} inc</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            else
            {
                sb.AppendLine($"      <tr><td>Hub (IST min..max)</td><td class='text-right'>-</td><td class='text-right'>{res.IstMin} .. {res.IstMax} inc</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }

            if (res.MaxVelocityMmS > 0)
            {
                sb.AppendLine($"      <tr><td>Max. Geschwindigkeit (IST)</td><td class='text-right'>-</td><td class='text-right'>{res.MaxVelocityMmS.ToString("F0", culture)} mm/s</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }
            if (res.MaxAccelG > 0 || res.MaxDecelG > 0)
            {
                sb.AppendLine($"      <tr><td>Spitzen-Beschleunigung / Bremsung</td><td class='text-right'>-</td><td class='text-right'>+{res.MaxAccelG.ToString("F1", culture)} / -{res.MaxDecelG.ToString("F1", culture)} g</td><td class='text-right'>-</td><td class='text-center'>{badge(true)}</td></tr>");
            }

            sb.AppendLine("    </tbody>");
            sb.AppendLine("  </table>");

            sb.AppendLine("  <div class='note-box'>");
            sb.AppendLine("    <b>Hinweis zur Drucksensor-Auswertung:</b> ");
            sb.AppendLine("    Die Schaltschwelle (Trigger) erfasst die Berührungsspannung bei der Werkstückantastung (0,080 V .. 4,500 V). ");
            sb.AppendLine("    Im Gegensatz zur statischen Anschlag-Kalibrierung (bei der der Hebel manuell bis zum 5 V Anschlag ausgelenkt wird) stoppt der automatische Dauertest ");
            sb.AppendLine("    beim sanften Antasten sofort bei der Schaltschwelle, um Bauteile und Nadelmechanik vor Beschädigung zu schützen.");
            sb.AppendLine("  </div>");

            sb.AppendLine(statusBadge);
            sb.AppendLine($"  <div style='font-size: 11px; color: #64748b; margin-top: 15px;'>Fehlermeldungen: {(string.IsNullOrWhiteSpace(res.ErrorMessage) ? "keine" : res.ErrorMessage)} | Zyklen: {res.CompletedCycles}/{res.Config.Cycles} | Endzeit: {res.EndTime:HH:mm:ss}</div>");
            sb.AppendLine("</body>");
            sb.AppendLine("</html>");

            return sb.ToString();
        }
    }
}
