using System;
using System.Collections.Generic;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Economy
{
    [Serializable]
    public sealed class BusinessDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public CityId City { get; set; }
        public string Description { get; set; }
        public long Price { get; set; }
        /// <summary>Naira earned per in-game day while owned.</summary>
        public long IncomePerDay { get; set; }
        /// <summary>Uncollected income caps here, so the player has a reason to visit.</summary>
        public long StorageCap { get; set; }
        /// <summary>Story mission that must be done before it can be bought.</summary>
        public string RequiresMission { get; set; }
    }

    public sealed class OwnedBusiness
    {
        public string BusinessId { get; set; }
        public double Uncollected { get; set; }
    }

    /// <summary>Businesses the player owns across all three cities and their accrued, uncollected income.</summary>
    public sealed class BusinessPortfolio
    {
        private readonly Dictionary<string, BusinessDefinition> _defs = new Dictionary<string, BusinessDefinition>();
        private readonly Dictionary<string, OwnedBusiness> _owned = new Dictionary<string, OwnedBusiness>();

        public event Action<BusinessDefinition> Purchased;

        public BusinessPortfolio(IEnumerable<BusinessDefinition> definitions)
        {
            foreach (var d in definitions) _defs[d.Id] = d;
        }

        public IEnumerable<OwnedBusiness> Owned => _owned.Values;

        public bool Owns(string id) => _owned.ContainsKey(id);

        public BusinessDefinition Get(string id) => _defs.TryGetValue(id, out var d) ? d : null;

        public bool TryBuy(string id, Wallet wallet, Func<string, bool> isMissionComplete)
        {
            if (!_defs.TryGetValue(id, out var def) || Owns(id)) return false;
            if (!string.IsNullOrEmpty(def.RequiresMission) && !isMissionComplete(def.RequiresMission)) return false;
            if (!wallet.TrySpend(def.Price, "business:" + id)) return false;

            _owned[id] = new OwnedBusiness { BusinessId = id };
            Purchased?.Invoke(def);
            return true;
        }

        /// <summary>Accrue income. Hook to <see cref="WorldClock.HoursElapsed"/>.</summary>
        public void Accrue(double gameHours)
        {
            if (gameHours <= 0) return;
            foreach (var owned in _owned.Values)
            {
                var def = _defs[owned.BusinessId];
                owned.Uncollected = Math.Min(def.StorageCap, owned.Uncollected + def.IncomePerDay * gameHours / 24.0);
            }
        }

        public long Collect(string id, Wallet wallet)
        {
            if (!_owned.TryGetValue(id, out var owned)) return 0;
            long amount = (long)Math.Floor(owned.Uncollected);
            owned.Uncollected -= amount;
            wallet.Earn(amount, "collect:" + id);
            return amount;
        }

        public long DailyIncome
        {
            get
            {
                long total = 0;
                foreach (var o in _owned.Values) total += _defs[o.BusinessId].IncomePerDay;
                return total;
            }
        }

        internal void Restore(IEnumerable<OwnedBusiness> owned)
        {
            _owned.Clear();
            foreach (var o in owned)
                if (_defs.ContainsKey(o.BusinessId)) _owned[o.BusinessId] = new OwnedBusiness { BusinessId = o.BusinessId, Uncollected = o.Uncollected };
        }
    }
}
