using System;

namespace NaijaHustle.Core.World
{
    /// <summary>
    /// In-game time. One real second = <see cref="GameMinutesPerRealSecond"/> game minutes.
    /// Kept engine-free so systems such as business income and weather can be unit tested.
    /// </summary>
    public sealed class WorldClock
    {
        public const int MinutesPerDay = 24 * 60;

        public float GameMinutesPerRealSecond { get; set; } = 2f;

        /// <summary>Total game minutes elapsed since the start of the save.</summary>
        public double TotalMinutes { get; private set; }

        public int Day => (int)(TotalMinutes / MinutesPerDay);
        public int Hour => (int)(TotalMinutes % MinutesPerDay) / 60;
        public int Minute => (int)(TotalMinutes % 60);

        /// <summary>0..1 through the current day, handy for sun rotation.</summary>
        public float DayFraction => (float)(TotalMinutes % MinutesPerDay / MinutesPerDay);

        public bool IsNight => Hour >= 19 || Hour < 6;

        /// <summary>Fires with the number of game hours that passed, for systems that accrue over time.</summary>
        public event Action<double> HoursElapsed;

        public WorldClock(double startMinutes = 8 * 60)
        {
            TotalMinutes = startMinutes;
        }

        public void Tick(float realDeltaSeconds)
        {
            if (realDeltaSeconds <= 0f) return;
            Advance(realDeltaSeconds * GameMinutesPerRealSecond);
        }

        /// <summary>Skip time, e.g. sleeping at a safehouse or an interstate bus ride.</summary>
        public void Advance(double gameMinutes)
        {
            if (gameMinutes <= 0) return;
            TotalMinutes += gameMinutes;
            HoursElapsed?.Invoke(gameMinutes / 60.0);
        }

        public void Restore(double totalMinutes) => TotalMinutes = Math.Max(0, totalMinutes);

        public override string ToString() => $"Day {Day + 1}, {Hour:00}:{Minute:00}";
    }
}
