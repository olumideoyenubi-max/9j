using System;
using System.Collections.Generic;

namespace NaijaHustle.Core.World
{
    public enum WeatherKind
    {
        Clear,
        Cloudy,
        HarmattanHaze,
        Rain,
        Thunderstorm,
        SootHaze,
    }

    /// <summary>Weighted chances for each weather kind in a city for one season.</summary>
    [Serializable]
    public sealed class WeatherWeights
    {
        public string Season { get; set; } = "dry";
        public Dictionary<WeatherKind, float> Weights { get; set; } = new Dictionary<WeatherKind, float>();
    }

    /// <summary>
    /// Picks weather per city and season. Harmattan haze dominates the dry season up north,
    /// Port Harcourt adds rain and its signature black-soot haze.
    /// </summary>
    public sealed class WeatherDirector
    {
        private readonly Random _rng;

        public WeatherKind Current { get; private set; } = WeatherKind.Clear;

        public event Action<WeatherKind> Changed;

        public WeatherDirector(Random rng)
        {
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        /// <summary>Game day 0..364 → season. Nov–Feb dry/harmattan, Mar–Oct rainy.</summary>
        public static string SeasonForDay(int day)
        {
            int dayOfYear = ((day % 365) + 365) % 365;
            int month = dayOfYear / 31; // rough, good enough for gameplay
            return (month >= 10 || month <= 1) ? "dry" : "rainy";
        }

        public WeatherKind Roll(IReadOnlyList<WeatherWeights> cityProfile, int day)
        {
            string season = SeasonForDay(day);
            WeatherWeights weights = null;
            foreach (var w in cityProfile)
            {
                if (string.Equals(w.Season, season, StringComparison.OrdinalIgnoreCase))
                {
                    weights = w;
                    break;
                }
            }

            var next = weights == null ? WeatherKind.Clear : Pick(weights.Weights);
            if (next != Current)
            {
                Current = next;
                Changed?.Invoke(next);
            }
            return next;
        }

        private WeatherKind Pick(Dictionary<WeatherKind, float> weights)
        {
            float total = 0f;
            foreach (var kv in weights) total += Math.Max(0f, kv.Value);
            if (total <= 0f) return WeatherKind.Clear;

            double roll = _rng.NextDouble() * total;
            foreach (var kv in weights)
            {
                roll -= Math.Max(0f, kv.Value);
                if (roll <= 0) return kv.Key;
            }
            return WeatherKind.Clear;
        }
    }
}
