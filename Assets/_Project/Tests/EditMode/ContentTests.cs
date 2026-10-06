using System.Linq;
using NaijaHustle.Core.World;
using NUnit.Framework;

namespace NaijaHustle.Tests
{
    public class ContentTests
    {
        [Test]
        public void ShippedContent_PassesValidation()
        {
            var errors = TestContent.Load().Validate();
            Assert.That(errors, Is.Empty, string.Join("\n", errors));
        }

        [TestCase(CityId.Lagos)]
        [TestCase(CityId.PortHarcourt)]
        [TestCase(CityId.Abuja)]
        public void EachCity_HasStoryOrderWithoutGapsAndEndsByUnlockingOrFinishing(CityId city)
        {
            var story = TestContent.Load().Data.Missions.Where(m => m.City == city && m.IsStory).OrderBy(m => m.Order).ToList();
            Assert.That(story.Select(m => m.Order), Is.EqualTo(Enumerable.Range(1, story.Count)));

            var last = story.Last();
            bool unlocksNext = last.UnlocksCity.HasValue;
            bool endsGame = last.SetsFlags.Contains("story_complete");
            Assert.That(unlocksNext || endsGame, $"{city} finale should unlock the next city or end the story");
        }

        [Test]
        public void EveryVehicleMissionTarget_ExistsAsVehicleIdOrClass()
        {
            var db = TestContent.Load().Data;
            var known = db.Vehicles.Select(v => v.Id)
                .Concat(db.Vehicles.Select(v => v.Class.ToString().ToLowerInvariant()))
                .ToHashSet();

            var targets = db.Missions.SelectMany(m => m.Objectives)
                .Where(o => o.Type == Core.Missions.ObjectiveType.EnterVehicle)
                .Select(o => o.TargetId);

            foreach (var t in targets) Assert.That(known, Does.Contain(t), $"Unknown vehicle target '{t}'");
        }

        [Test]
        public void NoRealWorldBrandsInContent()
        {
            // Guardrail: keep it original. Extend this list as needed.
            string[] banned = { "rockstar", "grand theft", "gta", "toyota", "camry", "lexus", "shell", "chevron", "nnpc", "lastma", "whatsapp", "twitter", "instagram", "opay", "keke napep" };
            var db = TestContent.Load().Data;
            var text = string.Join("\n",
                db.Missions.Select(m => m.Title + " " + m.Summary)
                .Concat(db.Vehicles.Select(v => v.Name))
                .Concat(db.Contacts.Select(c => c.Name + " " + c.Bio))
                .Concat(db.Social.Select(s => s.Author + " " + s.Handle + " " + s.Text))
                .Concat(db.Businesses.Select(b => b.Name + " " + b.Description))
                .Concat(db.Cities.SelectMany(c => c.Radio.Select(r => r.Name + " " + r.Host)))).ToLowerInvariant();

            foreach (var word in banned)
                Assert.That(System.Text.RegularExpressions.Regex.IsMatch(text, $@"\b{System.Text.RegularExpressions.Regex.Escape(word)}\b"), Is.False, $"Found banned term '{word}'");
        }
    }
}
