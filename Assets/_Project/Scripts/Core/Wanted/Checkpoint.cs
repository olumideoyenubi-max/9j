using System;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Progression;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Wanted
{
    public enum CheckpointChoice
    {
        /// <summary>Show papers, open the boot. Slow but clean, unless you're carrying mission cargo.</summary>
        Comply,
        /// <summary>The satirical "settle" option. Quick, costs naira and Integrity, and someone might be filming.</summary>
        Settle,
        /// <summary>Talk your way through. Outfit respect and street cred help.</summary>
        Talk,
        /// <summary>Floor it. Instant heat.</summary>
        Floor,
    }

    public enum CheckpointOutcomeKind
    {
        WavedThrough,
        Delayed,
        CargoFound,
        Recorded,
        IntegrityUnitSting,
        Escalated,
    }

    public sealed class CheckpointOutcome
    {
        public CheckpointOutcomeKind Kind { get; set; }
        public long CashSpent { get; set; }
        public float DelayMinutes { get; set; }
        public float HeatAdded { get; set; }
        public int IntegrityDelta { get; set; }
        public int CloutDelta { get; set; }
        /// <summary>Line of satirical dialogue/caption for UI and the social feed.</summary>
        public string Flavour { get; set; }
    }

    [Serializable]
    public sealed class CheckpointRules
    {
        public CityId City { get; set; }
        public long BaseSettleAmount { get; set; } = 2000;
        /// <summary>Chance a bystander records the "settle" and it trends on the social app.</summary>
        public float RecordChance { get; set; } = 0.15f;
        /// <summary>Chance the officer is actually an undercover integrity unit (Abuja is highest).</summary>
        public float StingChance { get; set; } = 0.05f;
        public float ComplyDelayMinutes { get; set; } = 20f;
        public int TalkDifficulty { get; set; } = 50;
    }

    /// <summary>
    /// Resolves a highway or city checkpoint. This is satire: settling is never free —
    /// it chips away at Integrity, can go viral, and in the capital can trigger a sting.
    /// </summary>
    public sealed class CheckpointResolver
    {
        private readonly Random _rng;

        public CheckpointResolver(Random rng)
        {
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        public CheckpointOutcome Resolve(
            CheckpointChoice choice,
            CheckpointRules rules,
            bool carryingCargo,
            Wallet wallet,
            PlayerProfile profile,
            int outfitRespect)
        {
            CheckpointOutcome outcome;
            switch (choice)
            {
                case CheckpointChoice.Comply:
                    outcome = carryingCargo
                        ? new CheckpointOutcome { Kind = CheckpointOutcomeKind.CargoFound, HeatAdded = 2f, Flavour = "\"Oga, wetin dey this box?\" Time to run." }
                        : new CheckpointOutcome { Kind = CheckpointOutcomeKind.Delayed, DelayMinutes = rules.ComplyDelayMinutes, IntegrityDelta = 1, Flavour = "Papers checked, boot checked, patience checked. You're clear." };
                    break;

                case CheckpointChoice.Settle:
                    outcome = ResolveSettle(rules, wallet);
                    break;

                case CheckpointChoice.Talk:
                    int score = outfitRespect + profile.Cred(rules.City) / 20 + _rng.Next(0, 60);
                    outcome = score >= rules.TalkDifficulty
                        ? new CheckpointOutcome { Kind = CheckpointOutcomeKind.WavedThrough, Flavour = "\"Ah, big boy! Correct outfit. Go well.\"" }
                        : new CheckpointOutcome { Kind = CheckpointOutcomeKind.Delayed, DelayMinutes = rules.ComplyDelayMinutes * 1.5f, Flavour = "Your story did not land. Park well." };
                    if (outcome.Kind == CheckpointOutcomeKind.Delayed && carryingCargo)
                        outcome = new CheckpointOutcome { Kind = CheckpointOutcomeKind.CargoFound, HeatAdded = 2f, Flavour = "They searched the vehicle anyway." };
                    break;

                case CheckpointChoice.Floor:
                default:
                    outcome = new CheckpointOutcome { Kind = CheckpointOutcomeKind.Escalated, HeatAdded = 2f, Flavour = "You floored it. Spikes, sirens, wahala." };
                    break;
            }

            profile.AddIntegrity(outcome.IntegrityDelta);
            profile.AddClout(outcome.CloutDelta);
            return outcome;
        }

        private CheckpointOutcome ResolveSettle(CheckpointRules rules, Wallet wallet)
        {
            long amount = rules.BaseSettleAmount;
            if (!wallet.TrySpend(amount, "checkpoint"))
                return new CheckpointOutcome { Kind = CheckpointOutcomeKind.Delayed, DelayMinutes = rules.ComplyDelayMinutes * 2f, Flavour = "\"You no get money? Park there.\"" };

            if (_rng.NextDouble() < rules.StingChance)
                return new CheckpointOutcome { Kind = CheckpointOutcomeKind.IntegrityUnitSting, CashSpent = amount, HeatAdded = 3f, IntegrityDelta = -10, Flavour = "Surprise: Integrity Unit sting operation. Smile for the camera, then run." };

            if (_rng.NextDouble() < rules.RecordChance)
                return new CheckpointOutcome { Kind = CheckpointOutcomeKind.Recorded, CashSpent = amount, HeatAdded = 1f, IntegrityDelta = -5, CloutDelta = -50, Flavour = "Someone on the bus filmed it. #CheckpointChronicles is trending." };

            return new CheckpointOutcome { Kind = CheckpointOutcomeKind.WavedThrough, CashSpent = amount, IntegrityDelta = -3, Flavour = "\"Happy weekend!\" ...it's Tuesday." };
        }
    }
}
