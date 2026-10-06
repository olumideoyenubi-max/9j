using System;
using System.IO;
using System.Linq;
using NaijaHustle.Core;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Missions;
using NaijaHustle.Core.Travel;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Core.World;

// Scripted playthrough of the real game rules. Every number below comes from the shipped content JSON.
// Usage: dotnet run --project Tools/Playthrough [seed]
int seed = args.Length > 0 && int.TryParse(args[0], out var s0) ? s0 : 2026;
var dir = Path.Combine(AppContext.BaseDirectory, "Content");
var db = ContentJson.Load(Directory.GetFiles(dir, "*.json").OrderBy(f => f).Select(File.ReadAllText));
var g = new GameSession(db, seed);

string Cash() => Wallet.Format(g.Wallet.Balance);
void H(string t) { Console.WriteLine(); Console.WriteLine("=== " + t + " " + new string('=', Math.Max(0, 60 - t.Length))); }
void Say(string t) => Console.WriteLine("  " + t);

g.Missions.MissionPassed += (run, pay) => Say($"JOB DONE: {run.Definition.Title}  +{Wallet.Format(pay)}  (cash {Cash()})");
g.Missions.MissionFailed += run => Say($"WAHALA! {run.Definition.Title} failed: {run.FailReason}");
g.Wanted.StarsChanged += st => Say($"Wanted: {new string('*', st)}{new string('.', 5 - st)}  ({g.Wanted.CurrentUnit(false) ?? "clear"})");
g.Social.Posted += (p, c) => Say($"[Yarns] {p.Author} {p.Handle}: \"{p.Text}\"");
g.Story.CityUnlocked += c => Say($">>> {c.DisplayName()} UNLOCKED");

void Play(string id, bool failOptional = false)
{
    var def = g.Missions.Get(id);
    if (!g.Missions.TryStart(id)) { Say($"(could not start {id})"); return; }
    Say($"> {def.Title}  [{db.Data.Contacts.Find(c => c.Id == def.Giver)?.Name}]");
    var run = g.Missions.Active;
    while (g.Missions.Active == run)
    {
        var o = run.CurrentObjective;
        Say($"    - {o.Text}");
        if (o.Optional && failOptional) { run.Fail("bonus missed"); continue; }
        if (o.Type == ObjectiveType.LoseHeat)
        {
            g.Wanted.AddHeat(2);
            while (g.Wanted.IsWanted) g.Tick(1f, seenByResponders: false, inSafeZone: false);
            g.Missions.Report(new ObjectiveEvent(o.Type, o.TargetId));
        }
        else if (o.Type == ObjectiveType.Wait || o.Type == ObjectiveType.Defend) g.Missions.Tick(o.TimeLimit + 0.1f);
        else g.Missions.Report(new ObjectiveEvent(o.Type, o.TargetId, o.Count));
    }
}

void PlayCity(CityId city)
{
    MissionDefinition next;
    while ((next = g.Missions.NextStory(city)) != null) Play(next.Id);
}

Console.WriteLine($"NAIJA HUSTLE — core test run (seed {seed})");

H("LAGOS: The Hustle");
Say($"{g.Profile.Name} wakes up in Oshoja with {Cash()}. {g.Clock}");
foreach (var m in g.Missions.Available(CityId.Lagos)) g.Chat.Receive(m.Giver, m.Summary, g.Clock.TotalMinutes, m.Id);
Say($"[Gist] {g.Chat.UnreadCount} unread — Baba Sule: \"{g.Chat.All[0].Text.Substring(0, 60)}...\"");
Play("lag_01");

H("Side hustle: Danfo Conductor");
g.StartShift("hus_lag_conductor");
float[] legs = { 0.9f, 1f, 0.8f, 0.1f, 1f };
foreach (var q in legs) Say($"Leg quality {q:0.0} -> paid {Wallet.Format(g.CompleteShiftJob(q))}  streak x{g.ActiveShift.Streak}");
Say($"Shift earned {Wallet.Format(g.EndShift().Earned)}. Cash {Cash()}");

H("Checkpoint on Lekka Strip");
foreach (var choice in new[] { CheckpointChoice.Comply, CheckpointChoice.Settle, CheckpointChoice.Talk })
{
    var o = g.ResolveCheckpoint(choice, carryingCargo: false);
    Say($"{choice,-7} -> {o.Kind}: {o.Flavour}  (integrity {g.Profile.Integrity}, cash {Cash()})");
}
g.Wanted.Clear();

H("Lagos story");
Play("lag_02"); Play("lag_03", failOptional: true);
PlayCity(CityId.Lagos);
g.Wallet.TrySpend(0, "");
Say($"Lagos cred {g.Profile.Cred(CityId.Lagos)}, cash {Cash()}");
if (g.Businesses.TryBuy("biz_lag_carwash", g.Wallet, g.Story.IsComplete)) Say($"Bought Mainland Shine Car Wash. Cash {Cash()}");

H("Night bus to Port Harcourt");
var quote = g.Travel.Quote(CityId.Lagos, CityId.PortHarcourt, TravelMode.LuxuryBus, g.Story, g.Wallet, false);
var plan = g.Travel.Book(quote, g.Wallet);
Say($"Fare {Wallet.Format(quote.Cost)}, {quote.Hours}h. On the bus:");
foreach (var e in plan.MiniEvents) Say($"  * {e.Text}");
g.Clock.Advance(quote.Hours * 60);
g.EnterCity(CityId.PortHarcourt);
Say($"Arrived. {g.Clock}. Weather: {g.Weather.Current}");
Say($"Car wash income while away: {Wallet.Format(g.Businesses.Collect("biz_lag_carwash", g.Wallet))}");

H("PORT HARCOURT: Garden City");
PlayCity(CityId.PortHarcourt);
g.StartShift("hus_ph_crane");
Say($"Crane shift: {string.Join(", ", new[] { 0.95f, 0.7f, 0.85f }.Select(q => Wallet.Format(g.CompleteShiftJob(q))))}");
g.EndShift();
g.Wanted.AddHeat(2);
Say($"On the water with 2 stars, PH sends: {g.Wanted.CurrentUnit(onWater: true)}");
g.Wanted.Clear();

H("Flight to Abuja");
var fq = g.Travel.Quote(CityId.PortHarcourt, CityId.Abuja, TravelMode.Flight, g.Story, g.Wallet, false);
Say(fq.Allowed ? $"Flight {Wallet.Format(fq.Cost)}" : "Blocked: " + fq.BlockedReason);
g.Travel.Book(fq, g.Wallet);
g.EnterCity(CityId.Abuja);

H("ABUJA: The Capital");
g.Wardrobe.TryBuy("fit_agbada_grand", g.Wallet); g.Wardrobe.Equip("fit_agbada_grand");
Say($"Dressed in {g.Wardrobe.Equipped.Name} (respect {g.Wardrobe.Respect}) for the gala.");
PlayCity(CityId.Abuja);

H("Final state");
Say($"Story complete: {g.Story.HasFlag("story_complete")}   Missions done: {g.Story.CompletedMissions.Count()}");
Say($"Cash {Cash()}   Integrity {g.Profile.Integrity}   Clout {g.Profile.Clout}");
Say($"Cred — Lagos {g.Profile.Cred(CityId.Lagos)}, PH {g.Profile.Cred(CityId.PortHarcourt)}, Abuja {g.Profile.Cred(CityId.Abuja)}");
var json = ContentJson.SerializeSave(g.CreateSave(new[] { 0f, 0f, 0f }));
var reload = new GameSession(db, 1); reload.LoadSave(ContentJson.DeserializeSave(json));
Say($"Save file {json.Length:N0} bytes; reloaded cash {Wallet.Format(reload.Wallet.Balance)} — match: {reload.Wallet.Balance == g.Wallet.Balance}");
