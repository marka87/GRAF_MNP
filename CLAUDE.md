# CLAUDE.md — Strikt geschützte Kernfunktionen & Entwicklungsregeln

> **ACHTUNG — VERBINDLICHE ENTWICKLUNGSREGEL:**
> Bei JEDER künftigen Änderung muss vorab geprüft werden, ob die unten aufgeführten Kernfunktionen, Grenzwerte und Schutzlogiken unversehrt bleiben.
> Änderungen dürfen ausschließlich **minimal, gezielt und chirurgisch** vorgenommen werden. Es dürfen keine bestehenden Schutzmechanismen, Messroutinen, SNO-Felder oder Prüfbericht-Layouts gekürzt oder gelöscht werden!

---

## 1. Strikt geschützte Kernfunktionen (TABU für Kürzungen)

### A. Sanfte Antastung (`PHASE_B_SETUP_SLOW_PROBE` in `test_run.c`)
- **Aufgabe:** Schont die Mechanik bei der ersten Annäherung an das Messobjekt.
- **Verhalten:** Fährt mit sanfter Geschwindigkeit (`Z_PID_SetSpeedLevel(3)`) abwärts, bis `ds_value >= s_ds_trigger_threshold` erreicht und mit 1 Tick entprellt ist. Fixiert sofort `touch_pos` (`s_z_ref_pos`).
- **Schutzgrenzen:**
  - Timeout: 15 Sekunden (`HAL_GetTick() - s_setup_start_tick > 15000u`)
  - Endlagenüberwachung: Notstopp, falls `z_pos <= z_encoder_start + 15` ("Z-Ende erreicht ohne Kontakt").
- **Regel:** Die State-Machine von `PHASE_B_SETUP_SLOW_PROBE` darf **nicht** durch verschachtelte Zwischenstufen ersetzt oder modifiziert werden.

### B. Drucksensor-Grenzwerte & Spannungsbegrenzung (`test_run.c`)
- **Aufgabe:** Schutz vor fehlerhaften Sollwerten / Telegrammen und ADC-Übersteuerung.
- **Werte & Grenzen:**
  - `TestRun_SetTriggerDeltaMv(uint32_t mv)`: Minimal 20 mV, **Maximal 4700 mV** (erlaubt realen Sensorbereich bis 4,78 V).
  - Minimaler Schwellwert in ADC-Counts: >= 75 Counts (Rauschunterdrückung).
  - Headroom-Prüfung in `TestRun_InitEx`: Baseline + Delta <= 4050 Counts (~4,94 V), verhindert ADC-Clipping.
- **Regel:** Der 4700 mV Deckel und die Headroom-Begrenzung dürfen niemals ersatzlos entfernt werden.

### C. Drucksensor-Messbereichsaufnahme (`PHASE_B_CALIB_DEFLECT` in `test_run.c`)
- **Aufgabe:** Ermittelt vor Beginn der schnellen Dauertest-Zyklen die individuelle Kennlinie der Nadel (realer Federweg und Spitzenspannung).
- **Verhalten:** Startet sanft von `touch_pos` mit Geschwindigkeitsstufe 2 und drückt bis zu maximal 30 Inkremente ein.
- **4-fache Sicherheitsabschaltung:**
  1. Wegbegrenzung: `travel >= 30` Inkremente (Überlastungsschutz Nadelmechanik).
  2. Sättigung: `ds_value >= 3890` Counts (~4,75 V).
  3. Plateau: `ds_value >= 3500` Counts (~4,27 V) und 20 Ticks unverändert.
  4. Endlage: `z_pos <= z_encoder_start + 15`.
- **Telegramm:** Sendet `TEST_B_CALIB:travel=...,peak_v=...,base_v=...` und übergibt in `TEST_B_SUMMARY` `contact_travel` und `peak_v`.
- **Regel:** Diese Routine ist Voraussetzung für die normgerechte Protokollierung im Mimot-Prüfbericht.

### D. Prüfbericht-Generierung & Normtoleranzen (`MimotReportGenerator.cs`)
- **Aufgabe:** Vollständige, abnahmekonforme Dokumentation gemäß Mimot-Werksvorschrift (TXT und HTML).
- **Geschützte Prüfkriterien:**
  - Mechanische Checkliste: Alle Punkte müssen mit Pass bewertet sein.
  - Standby-Spannung Drucksensor: 0.000 V .. 0.350 V.
  - Angesprochene Sensorspannung: 4.250 V .. 5.500 V.
  - Federweg / Ansprechabstand: 0.000 inc .. 35.000 inc.
  - SNO Schaltschwelle oben: 1500 .. 3450 inc.
  - SNO Schaltschwelle unten: 1500 .. 3450 inc.
  - SNO Schalthysterese: 0 .. 30 inc.
  - SNO Nadel-oben-Position: 1500 .. 3450 inc.
  - Verlorene Schritte: <= 10 inc.
  - Streuung Dauertest (Test B): <= 10 inc.
- **Regel:** Keine dieser Zeilen, Felder oder Toleranzprüfungen darf gekürzt, ausgeblendet oder entfernt werden.

### E. Kinematik-, Stall- & Positions-Überwachung
- **`s_ds_accel_fault_debounce`:** Blockierschutz und Notabschaltung bei Schleppfehlern >= 15 Inkrementen.
- **Positionshaltung bei STOP:** Hält die Achse nach Abbruch aktiv auf Position (kein Absacken).
- **Lichtschranken-Abschaltung (SNO):** Schutz vor oberem mechanischen Crash.

---

## 2. Arbeits- und Vorgehensregeln für KI-Assistenten

1. **Vor jeder Änderung:**
   - Den Diff vorab prüfen: Berührt die Änderung eine der geschützten 5 Kernfunktionen?
   - Wenn ja: Ist die Änderung 100% kompatibel und erhält alle Grenzwerte und Sicherheitschecks?
2. **Minimaler Eingriff ("Ponytail-Prinzip"):**
   - Kürzester funktionierender Diff.
   - Keine unnötigen Refactorings oder Umbenennungen funktionierender Codeblöcke.
3. **Synchronisations-Pflicht:**
   - Änderungen müssen immer synchron in beiden Repositories gepflegt werden:
     - Worktree: `c:\GIT\GRAF_MNP.worktrees\stm32-vscode-build-flash`
     - Haupt-Repo: `C:\GIT\GRAF_MNP`
4. **Build-Verifikation:**
   - Firmware Build (`make -C Software/GRAF_MNPA/Debug -j8 all`) muss 0 Fehler und 0 Warnungen liefern.
   - GUI Build (`dotnet build SW_WIN/MnpControl/MnpControl.csproj -c Release`) muss 0 Fehler liefern.