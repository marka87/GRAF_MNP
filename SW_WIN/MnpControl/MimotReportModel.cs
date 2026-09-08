using System;
using System.Collections.Generic;

namespace MnpControl
{
    public enum MimotTestType
    {
        BestueckenTestB,
        DauertestHubTestA
    }

    public class MimotChecklistItem
    {
        public string Description { get; set; } = "";
        public bool IsPassed { get; set; } = true;

        public MimotChecklistItem() { }

        public MimotChecklistItem(string description, bool isPassed = true)
        {
            Description = description;
            IsPassed = isPassed;
        }
    }

    public class MimotTestConfig
    {
        public string OperatorId { get; set; } = "";
        public string SerialNumber { get; set; } = "";
        public string Remarks { get; set; } = "keine";
        public MimotTestType TestType { get; set; } = MimotTestType.BestueckenTestB;
        public int Cycles { get; set; } = 10;
        public List<MimotChecklistItem> Checklist { get; set; } = new();
    }

    public class MimotTestResult
    {
        public MimotTestConfig Config { get; set; } = new();
        public DateTime StartTime { get; set; } = DateTime.Now;
        public DateTime EndTime { get; set; } = DateTime.Now;
        public bool OverallSuccess { get; set; } = true;
        public string ErrorMessage { get; set; } = "";

        // Sensor-Werte
        public float BaselineVoltage { get; set; } = 0.05f;
        public float TriggerVoltage { get; set; } = 4.69f;
        public int NoSensorPos { get; set; } = 0;
        public int ContactTravelInc { get; set; } = 5;

        // Zyklus- und Positionsdaten
        public int CompletedCycles { get; set; } = 0;
        public int LostSteps { get; set; } = 0;
        public int Overshoot { get; set; } = 0;
        public int ScatterRange { get; set; } = 0;
        public float MeanPosition { get; set; } = 0;
        public int IstMin { get; set; } = 0;
        public int IstMax { get; set; } = 0;
        public int SollMin { get; set; } = 0;
        public int SollMax { get; set; } = 0;

        // Kinematik
        public float MaxVelocityMmS { get; set; } = 0;
        public float MaxAccelG { get; set; } = 0;
        public float MaxDecelG { get; set; } = 0;
    }
}

