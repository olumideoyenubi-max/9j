using System;
using System.Collections.Generic;

namespace NaijaHustle.Core.World
{
    /// <summary>A city flavour event: go-slow, owambe, area-boy toll, convoy road block, flash flood...</summary>
    [Serializable]
    public sealed class AmbientEventDefinition
    {
        public string Id { get; set; }
        public CityId City { get; set; }
        public string Name { get; set; }
        public string Description { get; set; }
        /// <summary>Chance per game hour of starting while inside the hour window.</summary>
        public float ChancePerHour { get; set; } = 0.1f;
        public int StartHour { get; set; } = 0;
        public int EndHour { get; set; } = 24;
        public float DurationMinutes { get; set; } = 30f;
        /// <summary>Multiplier applied to traffic density while active (go-slow = 3.0).</summary>
        public float TrafficMultiplier { get; set; } = 1f;
        public string DistrictId { get; set; }
    }

    public sealed class ActiveAmbientEvent
    {
        public AmbientEventDefinition Definition { get; }
        public double EndsAtMinute { get; }

        public ActiveAmbientEvent(AmbientEventDefinition def, double endsAt)
        {
            Definition = def;
            EndsAtMinute = endsAt;
        }
    }

    /// <summary>Rolls ambient events every game hour for the city the player is in.</summary>
    public sealed class AmbientEventScheduler
    {
        private readonly Random _rng;
        private readonly List<ActiveAmbientEvent> _active = new List<ActiveAmbientEvent>();

        public IReadOnlyList<ActiveAmbientEvent> Active => _active;

        public event Action<ActiveAmbientEvent> Started;
        public event Action<AmbientEventDefinition> Ended;

        public AmbientEventScheduler(Random rng)
        {
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        public float TrafficMultiplier
        {
            get
            {
                float m = 1f;
                foreach (var e in _active) m = Math.Max(m, e.Definition.TrafficMultiplier);
                return m;
            }
        }

        public void RollHour(IEnumerable<AmbientEventDefinition> candidates, CityId city, WorldClock clock)
        {
            Expire(clock.TotalMinutes);
            foreach (var def in candidates)
            {
                if (def.City != city || IsActive(def.Id)) continue;
                if (!InWindow(clock.Hour, def.StartHour, def.EndHour)) continue;
                if (_rng.NextDouble() >= def.ChancePerHour) continue;

                var evt = new ActiveAmbientEvent(def, clock.TotalMinutes + def.DurationMinutes);
                _active.Add(evt);
                Started?.Invoke(evt);
            }
        }

        public void Expire(double nowMinutes)
        {
            for (int i = _active.Count - 1; i >= 0; i--)
            {
                if (_active[i].EndsAtMinute > nowMinutes) continue;
                var def = _active[i].Definition;
                _active.RemoveAt(i);
                Ended?.Invoke(def);
            }
        }

        /// <summary>Called when the player changes city: nothing carries over.</summary>
        public void Clear()
        {
            foreach (var e in _active) Ended?.Invoke(e.Definition);
            _active.Clear();
        }

        public bool IsActive(string id) => _active.Exists(e => e.Definition.Id == id);

        private static bool InWindow(int hour, int start, int end)
        {
            // Supports windows that wrap past midnight, e.g. owambe 20 → 3.
            return start <= end ? hour >= start && hour < end : hour >= start || hour < end;
        }
    }
}
