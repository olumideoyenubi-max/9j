using System;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Core.World;
using NUnit.Framework;

namespace NaijaHustle.Tests
{
    public class WantedTests
    {
        [Test]
        public void UnwitnessedCrime_DoesNotStartHeat()
        {
            var s = TestContent.NewSession();
            s.Wanted.Report(CrimeType.VehicleTheft, witnessed: false);
            Assert.That(s.Wanted.Stars, Is.EqualTo(0));
            s.Wanted.Report(CrimeType.VehicleTheft, witnessed: true);
            Assert.That(s.Wanted.Stars, Is.EqualTo(1));
        }

        [Test]
        public void Stars_DecayOneAtATimeOutOfSight()
        {
            var s = TestContent.NewSession();
            s.Wanted.AddHeat(3f);
            Assert.That(s.Wanted.Stars, Is.EqualTo(3));

            float perStar = s.Wanted.Profile.SearchSecondsPerStar;
            s.Wanted.Tick(perStar - 1f, seen: false);
            Assert.That(s.Wanted.Stars, Is.EqualTo(3));
            s.Wanted.Tick(2f, seen: false);
            Assert.That(s.Wanted.Stars, Is.EqualTo(2));

            s.Wanted.Tick(perStar - 1f, seen: true); // spotted resets the search
            s.Wanted.Tick(perStar - 1f, seen: false);
            Assert.That(s.Wanted.Stars, Is.EqualTo(2));
        }

        [Test]
        public void Abuja_IsHarsherThanLagos()
        {
            var s = TestContent.NewSession();
            s.Wanted.Report(CrimeType.Assault, true);
            float lagosHeat = s.Wanted.Heat;

            s.Story.UnlockCity(CityId.Abuja);
            s.EnterCity(CityId.Abuja);
            s.Wanted.Report(CrimeType.Assault, true);
            Assert.That(s.Wanted.Heat, Is.GreaterThan(lagosHeat));
            Assert.That(s.Wanted.CurrentUnit(onWater: false), Does.Contain("Capital Guard").Or.Contain("Estate"));
        }

        [Test]
        public void PortHarcourt_SendsMarinePatrolOnWater()
        {
            var s = TestContent.NewSession();
            s.Story.UnlockCity(CityId.PortHarcourt);
            s.EnterCity(CityId.PortHarcourt);
            s.Wanted.AddHeat(2f);
            Assert.That(s.Wanted.CurrentUnit(onWater: true), Does.Contain("Marine Patrol"));
        }

        [Test]
        public void Bust_CostsCashAndClearsHeat()
        {
            var s = TestContent.NewSession();
            s.Wallet.Earn(95_000, "test"); // 100,000 total
            s.Wanted.AddHeat(2f);
            long lost = s.Bust();
            Assert.That(lost, Is.EqualTo(8000)); // Lagos 8%
            Assert.That(s.Wanted.IsWanted, Is.False);
        }

        [Test]
        public void Checkpoint_SettleCostsIntegrity()
        {
            var s = TestContent.NewSession();
            var outcome = s.ResolveCheckpoint(CheckpointChoice.Settle, carryingCargo: false);
            Assert.That(outcome.CashSpent, Is.EqualTo(1000));
            Assert.That(s.Profile.Integrity, Is.LessThan(0));
        }

        [Test]
        public void Checkpoint_ComplyWithCargo_RaisesHeat()
        {
            var s = TestContent.NewSession();
            var outcome = s.ResolveCheckpoint(CheckpointChoice.Comply, carryingCargo: true);
            Assert.That(outcome.Kind, Is.EqualTo(CheckpointOutcomeKind.CargoFound));
            Assert.That(s.Wanted.Stars, Is.EqualTo(2));
        }

        [Test]
        public void Checkpoint_SettleOutcomes_CoverAllBranchesOverManySeeds()
        {
            var rules = new CheckpointRules { City = CityId.Abuja, BaseSettleAmount = 10, RecordChance = 0.3f, StingChance = 0.3f };
            bool sting = false, recorded = false, through = false;
            for (int seed = 0; seed < 200; seed++)
            {
                var r = new CheckpointResolver(new Random(seed));
                var o = r.Resolve(CheckpointChoice.Settle, rules, false, new Core.Economy.Wallet(100), new Core.Progression.PlayerProfile(), 0);
                sting |= o.Kind == CheckpointOutcomeKind.IntegrityUnitSting;
                recorded |= o.Kind == CheckpointOutcomeKind.Recorded;
                through |= o.Kind == CheckpointOutcomeKind.WavedThrough;
            }
            Assert.That(sting && recorded && through);
        }
    }
}
