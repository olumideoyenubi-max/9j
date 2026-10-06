using System;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Missions
{
    public enum HustleKind
    {
        DanfoConductor,
        OkadaDelivery,
        CraneJob,
        ForkliftJob,
        BoatTaxi,
        VipChauffeur,
        EstateSecurity,
    }

    [Serializable]
    public sealed class HustleDefinition
    {
        public string Id { get; set; }
        public HustleKind Kind { get; set; }
        public CityId City { get; set; }
        public string Name { get; set; }
        public string Description { get; set; }
        /// <summary>Required vehicle class id to start (e.g. "danfo").</summary>
        public string RequiredVehicle { get; set; }
        public long PayPerJob { get; set; }
        /// <summary>Extra pay per consecutive job in one shift.</summary>
        public long StreakBonus { get; set; }
        public int JobsPerLevel { get; set; } = 10;
        public int MaxLevel { get; set; } = 5;
        /// <summary>Mission that unlocks the hustle.</summary>
        public string UnlockedBy { get; set; }
    }

    /// <summary>
    /// A shift of repeatable work: fares on a danfo route, containers on the crane,
    /// passengers on the boat taxi. Pays per job with a streak bonus that resets on a botched job.
    /// </summary>
    public sealed class HustleShift
    {
        public HustleDefinition Definition { get; }
        public int Streak { get; private set; }
        public int JobsDone { get; private set; }
        public long Earned { get; private set; }
        public int Level { get; }

        public HustleShift(HustleDefinition def, int lifetimeJobs)
        {
            Definition = def ?? throw new ArgumentNullException(nameof(def));
            Level = LevelFor(def, lifetimeJobs);
        }

        public static int LevelFor(HustleDefinition def, int lifetimeJobs) =>
            Math.Min(def.MaxLevel, 1 + lifetimeJobs / Math.Max(1, def.JobsPerLevel));

        /// <param name="quality">0..1 — passenger comfort, container placement accuracy, on-time arrival.</param>
        /// <returns>Pay for this job.</returns>
        public long CompleteJob(float quality)
        {
            quality = Math.Max(0f, Math.Min(1f, quality));
            if (quality < 0.25f)
            {
                Streak = 0;
                JobsDone++;
                return 0;
            }

            Streak++;
            JobsDone++;
            double levelMult = 1.0 + 0.15 * (Level - 1);
            long pay = (long)Math.Round(Definition.PayPerJob * quality * levelMult) + Definition.StreakBonus * (Streak - 1);
            Earned += pay;
            return pay;
        }
    }
}
