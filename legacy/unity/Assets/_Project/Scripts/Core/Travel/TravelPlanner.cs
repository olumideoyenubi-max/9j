using System;
using System.Collections.Generic;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Progression;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Travel
{
    public enum TravelMode
    {
        /// <summary>Luxury bus: cheap, long, plays a cutscene with mini-events.</summary>
        LuxuryBus,
        /// <summary>Drive yourself: a highway driving segment with checkpoints; keeps your car.</summary>
        Road,
        /// <summary>Domestic flight: fast and pricey; needs the "flight_unlocked" story flag.</summary>
        Flight,
    }

    [Serializable]
    public sealed class RouteDefinition
    {
        public CityId From { get; set; }
        public CityId To { get; set; }
        public long BusFare { get; set; }
        public float BusHours { get; set; }
        public float RoadHours { get; set; }
        public int RoadCheckpoints { get; set; }
        public long FlightFare { get; set; }
        public float FlightHours { get; set; } = 1.5f;
    }

    [Serializable]
    public sealed class BusMiniEvent
    {
        public string Id { get; set; }
        public string Text { get; set; }
        /// <summary>Optional quick choice: e.g. buy the miracle soap, share your suya.</summary>
        public string ChoiceA { get; set; }
        public string ChoiceB { get; set; }
        public long CostA { get; set; }
        public int CloutA { get; set; }
        public int IntegrityA { get; set; }
        public float Weight { get; set; } = 1f;
    }

    public sealed class TravelQuote
    {
        public TravelMode Mode { get; set; }
        public CityId From { get; set; }
        public CityId To { get; set; }
        public long Cost { get; set; }
        public float Hours { get; set; }
        public int Checkpoints { get; set; }
        public bool Allowed { get; set; }
        public string BlockedReason { get; set; }
    }

    public sealed class TravelPlan
    {
        public TravelQuote Quote { get; set; }
        public List<BusMiniEvent> MiniEvents { get; set; } = new List<BusMiniEvent>();
    }

    /// <summary>Interstate travel between the three cities.</summary>
    public sealed class TravelPlanner
    {
        public const string FlightFlag = "flight_unlocked";

        private readonly List<RouteDefinition> _routes;
        private readonly List<BusMiniEvent> _busEvents;
        private readonly Random _rng;

        public TravelPlanner(IEnumerable<RouteDefinition> routes, IEnumerable<BusMiniEvent> busEvents, Random rng)
        {
            _routes = new List<RouteDefinition>(routes);
            _busEvents = new List<BusMiniEvent>(busEvents);
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        /// <summary>Routes are symmetric: a Lagos→Abuja entry also serves Abuja→Lagos.</summary>
        public RouteDefinition FindRoute(CityId a, CityId b) =>
            _routes.Find(r => (r.From == a && r.To == b) || (r.From == b && r.To == a));

        public TravelQuote Quote(CityId from, CityId to, TravelMode mode, StoryProgress story, Wallet wallet, bool isWanted)
        {
            var q = new TravelQuote { Mode = mode, From = from, To = to };
            var route = FindRoute(from, to);

            if (from == to) return Block(q, "You're already here.");
            if (route == null) return Block(q, "No route.");

            switch (mode)
            {
                case TravelMode.LuxuryBus: q.Cost = route.BusFare; q.Hours = route.BusHours; break;
                case TravelMode.Road: q.Cost = 0; q.Hours = route.RoadHours; q.Checkpoints = route.RoadCheckpoints; break;
                case TravelMode.Flight: q.Cost = route.FlightFare; q.Hours = route.FlightHours; break;
            }

            if (!story.IsUnlocked(to)) return Block(q, to.DisplayName() + " is locked. Keep hustling.");
            if (mode == TravelMode.Flight && !story.HasFlag(FlightFlag)) return Block(q, "Flights unlock later in the story.");
            if (isWanted && mode != TravelMode.Road) return Block(q, "Lose your heat first; the park and airport have security.");
            if (!wallet.CanAfford(q.Cost)) return Block(q, "Not enough money. Fare is " + Wallet.Format(q.Cost) + ".");

            q.Allowed = true;
            return q;
        }

        /// <summary>Pay and build the trip. Road trips return no mini-events (the player drives the highway segment).</summary>
        public TravelPlan Book(TravelQuote quote, Wallet wallet, int miniEventCount = 3)
        {
            if (quote == null || !quote.Allowed) return null;
            if (!wallet.TrySpend(quote.Cost, "travel:" + quote.Mode)) return null;

            var plan = new TravelPlan { Quote = quote };
            if (quote.Mode == TravelMode.LuxuryBus) plan.MiniEvents = PickMiniEvents(miniEventCount);
            return plan;
        }

        private List<BusMiniEvent> PickMiniEvents(int count)
        {
            var pool = new List<BusMiniEvent>(_busEvents);
            var picked = new List<BusMiniEvent>();
            while (picked.Count < count && pool.Count > 0)
            {
                float total = 0f;
                foreach (var e in pool) total += Math.Max(0.01f, e.Weight);
                double roll = _rng.NextDouble() * total;
                int idx = pool.Count - 1;
                for (int i = 0; i < pool.Count; i++)
                {
                    roll -= Math.Max(0.01f, pool[i].Weight);
                    if (roll <= 0) { idx = i; break; }
                }
                picked.Add(pool[idx]);
                pool.RemoveAt(idx);
            }
            return picked;
        }

        private static TravelQuote Block(TravelQuote q, string reason)
        {
            q.Allowed = false;
            q.BlockedReason = reason;
            return q;
        }
    }
}
