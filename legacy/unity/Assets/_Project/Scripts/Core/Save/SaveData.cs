using System;
using System.Collections.Generic;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Phone;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Save
{
    /// <summary>
    /// Plain serializable snapshot of everything that persists. Kept flat and versioned so
    /// old saves can be migrated; the runtime layer serializes it to JSON (Newtonsoft).
    /// </summary>
    [Serializable]
    public sealed class SaveData
    {
        public const int CurrentVersion = 1;

        public int Version { get; set; } = CurrentVersion;
        public string SavedAtUtc { get; set; }

        public long Cash { get; set; }
        public double ClockMinutes { get; set; }

        public CityId CurrentCity { get; set; }
        public float[] Position { get; set; } = new float[3];
        public string LastSafehouse { get; set; }

        public List<string> CompletedMissions { get; set; } = new List<string>();
        public List<string> Flags { get; set; } = new List<string>();
        public List<CityId> UnlockedCities { get; set; } = new List<CityId>();

        public Dictionary<CityId, int> Cred { get; set; } = new Dictionary<CityId, int>();
        public int Integrity { get; set; }
        public int Clout { get; set; }

        public List<string> OwnedOutfits { get; set; } = new List<string>();
        public string EquippedOutfit { get; set; }
        public List<string> OwnedSafehouses { get; set; } = new List<string>();
        public List<OwnedVehicle> Vehicles { get; set; } = new List<OwnedVehicle>();
        public List<OwnedBusiness> Businesses { get; set; } = new List<OwnedBusiness>();
        public Dictionary<string, int> HustleJobs { get; set; } = new Dictionary<string, int>();

        public List<ChatMessage> Chats { get; set; } = new List<ChatMessage>();

        public static SaveData Migrate(SaveData data)
        {
            if (data == null) return null;
            // v1 is the first format. Add "if (data.Version < 2) { ... }" steps here.
            data.Version = CurrentVersion;
            return data;
        }
    }

    /// <summary>Platform storage (PlayerPrefs, persistent file, cloud). Implemented in the runtime layer.</summary>
    public interface ISaveStore
    {
        bool Exists(int slot);
        void Write(int slot, SaveData data);
        SaveData Read(int slot);
        void Delete(int slot);
    }
}
