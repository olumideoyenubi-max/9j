using System.Linq;
using NaijaHustle.Core;
using NaijaHustle.Core.Missions;
using NaijaHustle.Core.World;
using NUnit.Framework;

namespace NaijaHustle.Tests
{
    public class MissionTests
    {
        /// <summary>Feeds each objective exactly what it needs. Used to "speedrun" the story in tests.</summary>
        private static void Complete(GameSession s, string id)
        {
            Assert.That(s.Missions.TryStart(id), Is.True, "could not start " + id);
            var run = s.Missions.Active;
            int guard = 0;
            while (s.Missions.Active == run && guard++ < 100)
            {
                var obj = run.CurrentObjective;
                if (obj.Type == ObjectiveType.Wait || obj.Type == ObjectiveType.Defend) s.Missions.Tick(obj.TimeLimit + 0.1f);
                else s.Missions.Report(new ObjectiveEvent(obj.Type, obj.TargetId, obj.Count));
            }
            Assert.That(run.State, Is.EqualTo(MissionState.Passed), id);
        }

        [Test]
        public void FirstMission_IsTheOnlyLagosStoryMissionAvailable()
        {
            var s = TestContent.NewSession();
            var story = s.Missions.Available(CityId.Lagos).Where(m => m.IsStory).Select(m => m.Id).ToList();
            Assert.That(story, Is.EqualTo(new[] { "lag_01" }));
        }

        [Test]
        public void Objectives_AdvanceInOrderAndPay()
        {
            var s = TestContent.NewSession();
            long before = s.Wallet.Balance;
            Assert.That(s.Missions.TryStart("lag_01"), Is.True);
            var run = s.Missions.Active;

            Assert.That(s.Missions.Report(new ObjectiveEvent(ObjectiveType.Collect, "fare")), Is.False, "wrong objective");
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.GoTo, "mk_oshoja_park"));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.EnterVehicle, "danfo"));
            for (int i = 0; i < 5; i++) s.Missions.Report(new ObjectiveEvent(ObjectiveType.Collect, "fare"));
            Assert.That(run.ObjectiveIndex, Is.EqualTo(2));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.Collect, "fare"));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.GoTo, "mk_eko_crest_stop"));

            Assert.That(run.State, Is.EqualTo(MissionState.Passed));
            Assert.That(s.Wallet.Balance - before, Is.EqualTo(8000));
            Assert.That(s.Story.IsComplete("lag_01"));
        }

        [Test]
        public void TimedObjective_FailsMission()
        {
            var s = TestContent.NewSession();
            s.Missions.TryStart("lag_01");
            var run = s.Missions.Active;
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.GoTo, "mk_oshoja_park"));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.EnterVehicle, "danfo"));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.Collect, "fare", 6));
            s.Missions.Tick(241f);
            Assert.That(run.State, Is.EqualTo(MissionState.Failed));
            Assert.That(s.Missions.Active, Is.Null);
            Assert.That(s.Story.IsComplete("lag_01"), Is.False);
        }

        [Test]
        public void OptionalObjectiveFailure_ReducesPayButPasses()
        {
            var s = TestContent.NewSession();
            Complete(s, "lag_01");
            Assert.That(s.Missions.TryStart("lag_03"), Is.True);
            var run = s.Missions.Active;
            long before = s.Wallet.Balance;
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.GoTo, "mk_balo_warehouse"));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.Collect, "crate_screens", 3));
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.Deliver, "mk_amaka_stall"));
            run.Fail("cracked a screen"); // optional bonus objective
            Assert.That(run.State, Is.EqualTo(MissionState.Passed));
            Assert.That(s.Wallet.Balance - before, Is.EqualTo(13500));
        }

        [Test]
        public void CannotStartMissionWhileWanted()
        {
            var s = TestContent.NewSession();
            s.Wanted.AddHeat(1f);
            Assert.That(s.Missions.TryStart("lag_01"), Is.False);
        }

        [Test]
        public void FullStory_IsCompletableAcrossAllThreeCities()
        {
            var s = TestContent.NewSession();
            foreach (var city in new[] { CityId.Lagos, CityId.PortHarcourt, CityId.Abuja })
            {
                if (city != CityId.Lagos) s.EnterCity(city);
                MissionDefinition next;
                int guard = 0;
                while ((next = s.Missions.NextStory(city)) != null && guard++ < 20) Complete(s, next.Id);
            }

            Assert.That(s.Story.HasFlag("story_complete"));
            Assert.That(s.Story.HasFlag("flight_unlocked"));
            Assert.That(s.Story.IsUnlocked(CityId.Abuja));
            var storyCount = TestContent.Load().Data.Missions.Count(m => m.IsStory);
            Assert.That(s.Story.CompletedMissions.Count(), Is.EqualTo(storyCount));
        }

        [Test]
        public void Hustle_StreakPaysMoreAndBotchedJobResetsIt()
        {
            var s = TestContent.NewSession();
            Assert.That(s.StartShift("hus_lag_conductor"), Is.Null, "locked before lag_01");
            Complete(s, "lag_01");
            var shift = s.StartShift("hus_lag_conductor");
            Assert.That(shift, Is.Not.Null);

            long first = s.CompleteShiftJob(1f);
            long second = s.CompleteShiftJob(1f);
            Assert.That(second, Is.GreaterThan(first));
            Assert.That(s.CompleteShiftJob(0.1f), Is.EqualTo(0));
            Assert.That(shift.Streak, Is.EqualTo(0));
            s.EndShift();
            Assert.That(s.HustleJobs["hus_lag_conductor"], Is.EqualTo(3));
        }
    }
}
