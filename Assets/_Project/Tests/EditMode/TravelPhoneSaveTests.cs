using System.Linq;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.Phone;
using NaijaHustle.Core.Travel;
using NaijaHustle.Core.World;
using NUnit.Framework;

namespace NaijaHustle.Tests
{
    public class TravelPhoneSaveTests
    {
        [Test]
        public void Travel_LockedCityBlocked()
        {
            var s = TestContent.NewSession();
            var q = s.Travel.Quote(CityId.Lagos, CityId.PortHarcourt, TravelMode.LuxuryBus, s.Story, s.Wallet, false);
            Assert.That(q.Allowed, Is.False);
        }

        [Test]
        public void Travel_BusChargesFareAndRollsMiniEvents()
        {
            var s = TestContent.NewSession();
            s.Story.UnlockCity(CityId.PortHarcourt);
            s.Wallet.Earn(100_000, "test");
            var q = s.Travel.Quote(CityId.Lagos, CityId.PortHarcourt, TravelMode.LuxuryBus, s.Story, s.Wallet, false);
            Assert.That(q.Allowed, q.BlockedReason);

            long before = s.Wallet.Balance;
            var plan = s.Travel.Book(q, s.Wallet);
            Assert.That(before - s.Wallet.Balance, Is.EqualTo(25000));
            Assert.That(plan.MiniEvents.Count, Is.EqualTo(3));
            Assert.That(plan.MiniEvents.Select(e => e.Id).Distinct().Count(), Is.EqualTo(3));
        }

        [Test]
        public void Travel_FlightNeedsFlagAndRoadAllowedWhileWanted()
        {
            var s = TestContent.NewSession();
            s.Story.UnlockCity(CityId.Abuja);
            s.Wallet.Earn(1_000_000, "test");
            Assert.That(s.Travel.Quote(CityId.Lagos, CityId.Abuja, TravelMode.Flight, s.Story, s.Wallet, false).Allowed, Is.False);
            s.Story.SetFlag(TravelPlanner.FlightFlag);
            Assert.That(s.Travel.Quote(CityId.Abuja, CityId.Lagos, TravelMode.Flight, s.Story, s.Wallet, false).Allowed, Is.True, "routes are symmetric");
            Assert.That(s.Travel.Quote(CityId.Lagos, CityId.Abuja, TravelMode.Flight, s.Story, s.Wallet, true).Allowed, Is.False);
            var road = s.Travel.Quote(CityId.Lagos, CityId.Abuja, TravelMode.Road, s.Story, s.Wallet, true);
            Assert.That(road.Allowed, Is.True);
            Assert.That(road.Checkpoints, Is.EqualTo(5));
        }

        [Test]
        public void Chat_TracksUnread()
        {
            var chat = new ChatApp();
            chat.Receive("c_baba_sule", "My pikin, come to the park.", 10, "lag_01");
            chat.Receive("c_amaka", "Your phone screen is ready", 20);
            Assert.That(chat.UnreadCount, Is.EqualTo(2));
            chat.MarkRead("c_baba_sule");
            Assert.That(chat.UnreadCount, Is.EqualTo(1));
            Assert.That(chat.Threads().First().contactId, Is.EqualTo("c_amaka"));
        }

        [Test]
        public void Transfer_ChargesFee()
        {
            var s = TestContent.NewSession();
            s.Transfers.NetworkWahalaChance = 0f;
            Assert.That(s.Transfers.Send(s.Wallet, "c_amaka", 1000), Is.EqualTo(TransferStatus.Successful));
            Assert.That(s.Wallet.Balance, Is.EqualTo(5000 - 1050));
            Assert.That(s.Transfers.Send(s.Wallet, "c_amaka", 1_000_000), Is.EqualTo(TransferStatus.InsufficientFunds));
        }

        [Test]
        public void Social_ReactsToMissionWithPlayerName()
        {
            var s = TestContent.NewSession();
            var post = s.Social.React("mission_passed:lag_02", 0, "Lucky");
            Assert.That(post.Text, Does.Contain("Lucky"));
            Assert.That(s.Social.React("nothing:matches", 0), Is.Null);
        }

        [Test]
        public void Save_RoundTripsThroughJson()
        {
            var s = TestContent.NewSession();
            s.Wallet.Earn(500_000, "test");
            s.Story.MarkComplete("lag_01");
            s.Story.MarkComplete("lag_03");
            s.Story.UnlockCity(CityId.PortHarcourt);
            s.Profile.AddCred(CityId.Lagos, 42);
            s.Wardrobe.TryBuy("fit_kaftan_white", s.Wallet);
            s.Wardrobe.Equip("fit_kaftan_white");
            s.Businesses.TryBuy("biz_lag_carwash", s.Wallet, s.Story.IsComplete);
            s.Businesses.Accrue(6);
            var car = s.Garage.Store("sedan_kamsi", "sh_lag_oshoja", 1);
            s.Garage.TryApplyMod(car.InstanceId, "mod_rims_chrome", s.Wallet);
            s.Chat.Receive("c_amaka", "Hi", 5);
            s.Clock.Advance(300);

            string json = ContentJson.SerializeSave(s.CreateSave(new[] { 1f, 2f, 3f }));
            var loaded = TestContent.NewSession(999);
            loaded.LoadSave(ContentJson.DeserializeSave(json));

            Assert.That(loaded.Wallet.Balance, Is.EqualTo(s.Wallet.Balance));
            Assert.That(loaded.Story.IsComplete("lag_03"));
            Assert.That(loaded.Story.IsUnlocked(CityId.PortHarcourt));
            Assert.That(loaded.Profile.Cred(CityId.Lagos), Is.EqualTo(42));
            Assert.That(loaded.Wardrobe.EquippedId, Is.EqualTo("fit_kaftan_white"));
            Assert.That(loaded.Businesses.Owns("biz_lag_carwash"));
            Assert.That(loaded.Garage.Owned.Single().Mods.Values, Does.Contain("mod_rims_chrome"));
            Assert.That(loaded.Chat.All.Count, Is.EqualTo(1));
            Assert.That(loaded.Clock.TotalMinutes, Is.EqualTo(s.Clock.TotalMinutes));

            var nextCar = loaded.Garage.Store("okada", "sh_lag_stilt", 1);
            Assert.That(nextCar.InstanceId, Is.Not.EqualTo(car.InstanceId), "instance ids keep counting after load");
        }

        [Test]
        public void Weather_PortHarcourtCanSootUp()
        {
            var db = TestContent.Load();
            var ph = db.City(CityId.PortHarcourt);
            var w = new WeatherDirector(new System.Random(7));
            bool soot = false;
            for (int i = 0; i < 200; i++) soot |= w.Roll(ph.Weather, day: 0) == WeatherKind.SootHaze;
            Assert.That(soot);
        }

        [Test]
        public void AmbientEvents_WrapPastMidnight()
        {
            var db = TestContent.Load();
            var sched = new AmbientEventScheduler(new System.Random(1));
            var clock = new WorldClock(startMinutes: 1 * 60); // 01:00 — inside the 14→03 owambe window
            var owambe = db.Data.AmbientEvents.Where(e => e.Id == "amb_lag_owambe").Select(e => { e.ChancePerHour = 1f; return e; }).ToList();
            sched.RollHour(owambe, CityId.Lagos, clock);
            Assert.That(sched.IsActive("amb_lag_owambe"));
        }
    }
}
