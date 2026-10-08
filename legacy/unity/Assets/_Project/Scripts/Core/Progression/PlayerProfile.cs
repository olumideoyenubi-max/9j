using System;
using System.Collections.Generic;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Progression
{
    /// <summary>
    /// Soft stats that drive dialogue, checkpoint outcomes and the story's rise-and-fall beats.
    /// Street Cred is per city; Integrity is global (-100 corrupt .. +100 clean).
    /// </summary>
    public sealed class PlayerProfile
    {
        public const int MaxIntegrity = 100;
        public const int MaxCred = 1000;

        private readonly Dictionary<CityId, int> _cred = new Dictionary<CityId, int>
        {
            { CityId.Lagos, 0 }, { CityId.PortHarcourt, 0 }, { CityId.Abuja, 0 },
        };

        public string Name { get; set; } = "Lucky";
        public int Integrity { get; private set; }
        /// <summary>Followers on the in-game social app. Viral posts move this.</summary>
        public int Clout { get; private set; }

        public event Action<CityId, int> CredChanged;
        public event Action<int> IntegrityChanged;
        public event Action<int> CloutChanged;

        public int Cred(CityId city) => _cred[city];

        public void AddCred(CityId city, int delta)
        {
            _cred[city] = Clamp(_cred[city] + delta, 0, MaxCred);
            CredChanged?.Invoke(city, _cred[city]);
        }

        public void AddIntegrity(int delta)
        {
            Integrity = Clamp(Integrity + delta, -MaxIntegrity, MaxIntegrity);
            IntegrityChanged?.Invoke(Integrity);
        }

        public void AddClout(int delta)
        {
            Clout = Math.Max(0, Clout + delta);
            CloutChanged?.Invoke(Clout);
        }

        internal void Restore(Dictionary<CityId, int> cred, int integrity, int clout)
        {
            foreach (var kv in cred) _cred[kv.Key] = Clamp(kv.Value, 0, MaxCred);
            Integrity = Clamp(integrity, -MaxIntegrity, MaxIntegrity);
            Clout = Math.Max(0, clout);
        }

        internal Dictionary<CityId, int> SnapshotCred() => new Dictionary<CityId, int>(_cred);

        private static int Clamp(int v, int min, int max) => v < min ? min : v > max ? max : v;
    }
}
