using System;
using System.Globalization;

namespace NaijaHustle.Core.Economy
{
    /// <summary>The player's naira. Whole naira only (no kobo), stored as long to survive late-game Abuja money.</summary>
    public sealed class Wallet
    {
        public long Balance { get; private set; }

        /// <summary>(newBalance, delta, reason)</summary>
        public event Action<long, long, string> Changed;

        public Wallet(long startingBalance = 0)
        {
            if (startingBalance < 0) throw new ArgumentOutOfRangeException(nameof(startingBalance));
            Balance = startingBalance;
        }

        public bool CanAfford(long amount) => amount >= 0 && Balance >= amount;

        public void Earn(long amount, string reason)
        {
            if (amount < 0) throw new ArgumentOutOfRangeException(nameof(amount));
            if (amount == 0) return;
            Balance = checked(Balance + amount);
            Changed?.Invoke(Balance, amount, reason);
        }

        public bool TrySpend(long amount, string reason)
        {
            if (amount < 0) throw new ArgumentOutOfRangeException(nameof(amount));
            if (!CanAfford(amount)) return false;
            if (amount == 0) return true;
            Balance -= amount;
            Changed?.Invoke(Balance, -amount, reason);
            return true;
        }

        /// <summary>Lose a fraction of cash (busted / wasted). Returns the amount lost.</summary>
        public long LosePercent(float fraction, string reason)
        {
            fraction = Math.Max(0f, Math.Min(1f, fraction));
            // Go through decimal so 0.08f means 8%, not 7.9999999%.
            long lost = (long)Math.Floor(Balance * (decimal)fraction);
            if (lost > 0)
            {
                Balance -= lost;
                Changed?.Invoke(Balance, -lost, reason);
            }
            return lost;
        }

        internal void Restore(long balance) => Balance = Math.Max(0, balance);

        public static string Format(long naira)
        {
            string sign = naira < 0 ? "-" : string.Empty;
            return sign + "₦" + Math.Abs(naira).ToString("N0", CultureInfo.InvariantCulture);
        }
    }
}
