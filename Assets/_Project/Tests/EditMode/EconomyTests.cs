using NaijaHustle.Core.Economy;
using NUnit.Framework;

namespace NaijaHustle.Tests
{
    public class EconomyTests
    {
        [Test]
        public void Wallet_SpendFailsWhenBroke()
        {
            var w = new Wallet(1000);
            Assert.That(w.TrySpend(1500, "x"), Is.False);
            Assert.That(w.Balance, Is.EqualTo(1000));
            Assert.That(w.TrySpend(400, "x"), Is.True);
            Assert.That(w.Balance, Is.EqualTo(600));
        }

        [Test]
        public void Wallet_FormatsNaira()
        {
            Assert.That(Wallet.Format(2500000), Is.EqualTo("₦2,500,000"));
        }

        [Test]
        public void Business_AccruesUpToCapAndCollects()
        {
            var s = TestContent.NewSession();
            s.Story.MarkComplete("lag_03");
            s.Wallet.Earn(1_000_000, "test");

            Assert.That(s.Businesses.TryBuy("biz_lag_carwash", s.Wallet, s.Story.IsComplete), Is.True);
            s.Businesses.Accrue(24); // one day = 9,000
            Assert.That(s.Businesses.Collect("biz_lag_carwash", s.Wallet), Is.EqualTo(9000));

            s.Businesses.Accrue(24 * 100); // capped
            Assert.That(s.Businesses.Collect("biz_lag_carwash", s.Wallet), Is.EqualTo(60000));
        }

        [Test]
        public void Business_LockedUntilMissionDone()
        {
            var s = TestContent.NewSession();
            s.Wallet.Earn(10_000_000, "test");
            Assert.That(s.Businesses.TryBuy("biz_lag_danfo_fleet", s.Wallet, s.Story.IsComplete), Is.False);
        }

        [Test]
        public void Clock_DrivesBusinessIncome()
        {
            var s = TestContent.NewSession();
            s.Story.MarkComplete("lag_03");
            s.Wallet.Earn(200_000, "test");
            s.Businesses.TryBuy("biz_lag_carwash", s.Wallet, s.Story.IsComplete);
            s.Clock.Advance(12 * 60);
            Assert.That(s.Businesses.Collect("biz_lag_carwash", s.Wallet), Is.EqualTo(4500));
        }

        [Test]
        public void Garage_RespectsSlotsAndModRestrictions()
        {
            var s = TestContent.NewSession();
            s.Wallet.Earn(5_000_000, "test");
            var danfo = s.Garage.Store("danfo", "sh_lag_oshoja", 1);
            Assert.That(danfo, Is.Not.Null);
            Assert.That(s.Garage.Store("okada", "sh_lag_oshoja", 1), Is.Null, "garage full");

            Assert.That(s.Garage.TryApplyMod(danfo.InstanceId, "mod_sticker_danfo", s.Wallet), Is.True);
            Assert.That(s.Garage.TryApplyMod(danfo.InstanceId, "mod_horn_siren", s.Wallet), Is.False, "siren not allowed on danfo");
            Assert.That(s.Garage.TryApplyMod(danfo.InstanceId, "mod_engine_1", s.Wallet), Is.True);
            Assert.That(s.Garage.EffectiveStats(danfo).topSpeed, Is.EqualTo(120f));
        }

        [Test]
        public void Wardrobe_BuyEquipRaisesRespect()
        {
            var s = TestContent.NewSession();
            s.Wallet.Earn(2_000_000, "test");
            Assert.That(s.Wardrobe.Respect, Is.EqualTo(0));
            Assert.That(s.Wardrobe.Equip("fit_agbada_grand"), Is.False, "not owned yet");
            Assert.That(s.Wardrobe.TryBuy("fit_agbada_grand", s.Wallet), Is.True);
            Assert.That(s.Wardrobe.Equip("fit_agbada_grand"), Is.True);
            Assert.That(s.Wardrobe.Respect, Is.EqualTo(40));
        }
    }
}
