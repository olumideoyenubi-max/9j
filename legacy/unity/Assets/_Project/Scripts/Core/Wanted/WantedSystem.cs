using System;
using System.Collections.Generic;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Wanted
{
    public enum CrimeType
    {
        TrafficOffence,
        Assault,
        VehicleTheft,
        PropertyDamage,
        Trespass,
        CheckpointRunning,
        AttackOnResponder,
        Sabotage,
        WeaponDischarge,
    }

    /// <summary>Heat added per crime. Data-driven so designers can tune without code.</summary>
    [Serializable]
    public sealed class CrimeWeight
    {
        public CrimeType Crime { get; set; }
        public float Heat { get; set; }
    }

    /// <summary>
    /// City-specific heat response: Lagos sends the (fictional) Eko Task Force, Port Harcourt
    /// adds Marine Patrol boats on water, Abuja escalates fast to armoured Capital Guard units.
    /// </summary>
    [Serializable]
    public sealed class ResponseProfile
    {
        public CityId City { get; set; }
        /// <summary>Unit names per star (index 0 = 1 star ... 4 = 5 stars).</summary>
        public string[] UnitsByStar { get; set; } = new string[5];
        /// <summary>Unit spawned instead when the player is on water.</summary>
        public string[] WaterUnitsByStar { get; set; } = new string[5];
        /// <summary>Multiplier on all heat gained. Abuja &gt; 1: heavy security.</summary>
        public float HeatMultiplier { get; set; } = 1f;
        /// <summary>Seconds out of sight before losing one star.</summary>
        public float SearchSecondsPerStar { get; set; } = 20f;
        /// <summary>Radius (m) of the search area responders sweep around the last-seen point.</summary>
        public float SearchRadius { get; set; } = 150f;
        /// <summary>Fraction of cash lost when busted.</summary>
        public float BustedCashPenalty { get; set; } = 0.1f;
    }

    /// <summary>
    /// Five-star heat. Heat accumulates from crimes; each star is one unit of heat.
    /// Out of the responders' sight, a search timer runs down and drops a star at a time.
    /// </summary>
    public sealed class WantedSystem
    {
        public const int MaxStars = 5;

        private readonly Dictionary<CrimeType, float> _weights = new Dictionary<CrimeType, float>();
        private ResponseProfile _profile;
        private float _searchTimer;

        public float Heat { get; private set; }
        public int Stars => (int)Math.Ceiling(Math.Min(Heat, MaxStars) - 0.0001f);
        public bool IsWanted => Stars > 0;
        public bool PlayerSeen { get; private set; }
        public ResponseProfile Profile => _profile;
        /// <summary>0..1 progress of the current search (1 = about to lose a star). For HUD flashing.</summary>
        public float SearchProgress => _profile == null || !IsWanted || PlayerSeen ? 0f : _searchTimer / _profile.SearchSecondsPerStar;

        public event Action<int> StarsChanged;
        public event Action<CrimeType> CrimeReported;

        public WantedSystem(IEnumerable<CrimeWeight> weights, ResponseProfile profile)
        {
            foreach (var w in weights) _weights[w.Crime] = w.Heat;
            SetProfile(profile);
        }

        public void SetProfile(ResponseProfile profile)
        {
            _profile = profile ?? throw new ArgumentNullException(nameof(profile));
        }

        /// <summary>Report a crime. Unwitnessed crimes only add heat if the player is already wanted.</summary>
        public void Report(CrimeType crime, bool witnessed)
        {
            if (!witnessed && !IsWanted) return;
            float heat = _weights.TryGetValue(crime, out var h) ? h : 0.5f;
            AddHeat(heat * _profile.HeatMultiplier);
            CrimeReported?.Invoke(crime);
        }

        public void AddHeat(float amount)
        {
            if (amount <= 0f) return;
            int before = Stars;
            Heat = Math.Min(MaxStars, Heat + amount);
            _searchTimer = 0f;
            if (Stars != before) StarsChanged?.Invoke(Stars);
        }

        /// <summary>Advance the search. <paramref name="seen"/> = any responder has line of sight.</summary>
        public void Tick(float deltaSeconds, bool seen, bool inSafeZone = false)
        {
            PlayerSeen = seen;
            if (!IsWanted) return;
            if (seen)
            {
                _searchTimer = 0f;
                return;
            }

            _searchTimer += deltaSeconds * (inSafeZone ? 3f : 1f);
            if (_searchTimer < _profile.SearchSecondsPerStar) return;

            _searchTimer = 0f;
            Heat = Math.Max(0f, (float)Math.Ceiling(Heat - 0.0001f) - 1f);
            StarsChanged?.Invoke(Stars);
        }

        public string CurrentUnit(bool onWater)
        {
            if (!IsWanted) return null;
            var table = onWater ? _profile.WaterUnitsByStar : _profile.UnitsByStar;
            if (table == null || table.Length == 0) return null;
            return table[Math.Min(Stars, table.Length) - 1];
        }

        public void Clear()
        {
            if (Heat <= 0f) return;
            Heat = 0f;
            _searchTimer = 0f;
            StarsChanged?.Invoke(0);
        }
    }
}
