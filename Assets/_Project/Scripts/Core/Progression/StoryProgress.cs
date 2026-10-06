using System;
using System.Collections.Generic;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Progression
{
    /// <summary>Completed missions, unlocked cities and named story flags (e.g. "flight_unlocked").</summary>
    public sealed class StoryProgress
    {
        private readonly HashSet<string> _completed = new HashSet<string>();
        private readonly HashSet<string> _flags = new HashSet<string>();
        private readonly HashSet<CityId> _unlockedCities = new HashSet<CityId> { CityId.Lagos };

        public CityId CurrentCity { get; set; } = CityId.Lagos;

        public IEnumerable<string> CompletedMissions => _completed;
        public IEnumerable<string> Flags => _flags;
        public IEnumerable<CityId> UnlockedCities => _unlockedCities;

        public event Action<CityId> CityUnlocked;
        public event Action<string> FlagSet;

        public bool IsComplete(string missionId) => _completed.Contains(missionId);
        public bool HasFlag(string flag) => _flags.Contains(flag);
        public bool IsUnlocked(CityId city) => _unlockedCities.Contains(city);

        public void MarkComplete(string missionId) => _completed.Add(missionId);

        public void SetFlag(string flag)
        {
            if (string.IsNullOrEmpty(flag)) return;
            if (_flags.Add(flag)) FlagSet?.Invoke(flag);
        }

        public void UnlockCity(CityId city)
        {
            if (_unlockedCities.Add(city)) CityUnlocked?.Invoke(city);
        }

        internal void Restore(IEnumerable<string> completed, IEnumerable<string> flags, IEnumerable<CityId> cities, CityId current)
        {
            _completed.Clear(); _completed.UnionWith(completed);
            _flags.Clear(); _flags.UnionWith(flags);
            _unlockedCities.Clear(); _unlockedCities.Add(CityId.Lagos); _unlockedCities.UnionWith(cities);
            CurrentCity = _unlockedCities.Contains(current) ? current : CityId.Lagos;
        }
    }
}
