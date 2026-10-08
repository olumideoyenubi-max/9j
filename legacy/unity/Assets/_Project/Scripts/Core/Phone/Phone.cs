using System;
using System.Collections.Generic;
using System.Linq;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Phone
{
    [Serializable]
    public sealed class ContactDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public string Role { get; set; }
        public CityId City { get; set; }
        public string Bio { get; set; }
        public string AvatarKey { get; set; }
    }

    public sealed class ChatMessage
    {
        public string ContactId { get; set; }
        public bool FromPlayer { get; set; }
        public string Text { get; set; }
        public double GameMinute { get; set; }
        /// <summary>Tapping the message offers to start this mission.</summary>
        public string MissionId { get; set; }
        public bool Read { get; set; }
    }

    /// <summary>"Gist" — the in-game chat app. Mission givers text you; voice notes are just flavour text.</summary>
    public sealed class ChatApp
    {
        private readonly List<ChatMessage> _messages = new List<ChatMessage>();

        public event Action<ChatMessage> Received;

        public IReadOnlyList<ChatMessage> All => _messages;

        public int UnreadCount => _messages.Count(m => !m.Read && !m.FromPlayer);

        public IEnumerable<ChatMessage> Thread(string contactId) => _messages.Where(m => m.ContactId == contactId);

        /// <summary>Threads, newest activity first, as (contactId, lastMessage, unread).</summary>
        public IEnumerable<(string contactId, ChatMessage last, int unread)> Threads() =>
            _messages.GroupBy(m => m.ContactId)
                .Select(g => (g.Key, g.Last(), g.Count(m => !m.Read && !m.FromPlayer)))
                .OrderByDescending(t => t.Item2.GameMinute);

        public void Receive(string contactId, string text, double gameMinute, string missionId = null)
        {
            var msg = new ChatMessage { ContactId = contactId, Text = text, GameMinute = gameMinute, MissionId = missionId };
            _messages.Add(msg);
            Received?.Invoke(msg);
        }

        public void Send(string contactId, string text, double gameMinute) =>
            _messages.Add(new ChatMessage { ContactId = contactId, Text = text, GameMinute = gameMinute, FromPlayer = true, Read = true });

        public void MarkRead(string contactId)
        {
            foreach (var m in _messages) if (m.ContactId == contactId) m.Read = true;
        }

        internal void Restore(IEnumerable<ChatMessage> messages)
        {
            _messages.Clear();
            _messages.AddRange(messages);
        }
    }

    public enum TransferStatus
    {
        Successful,
        InsufficientFunds,
        /// <summary>Satire: "network wahala" — debited... then reversed a few game minutes later.</summary>
        PendingReversal,
        InvalidAmount,
    }

    /// <summary>"KoboPay" — bank transfer app. Pays contacts, receives mission payments, and occasionally has network wahala.</summary>
    public sealed class TransferApp
    {
        private readonly Random _rng;

        public float NetworkWahalaChance { get; set; } = 0.05f;
        public long FlatFee { get; set; } = 50;

        public event Action<string, long, TransferStatus> Transferred;

        public TransferApp(Random rng)
        {
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        public TransferStatus Send(Wallet wallet, string toContactId, long amount)
        {
            if (amount <= 0) return Done(toContactId, amount, TransferStatus.InvalidAmount);
            if (!wallet.CanAfford(amount + FlatFee)) return Done(toContactId, amount, TransferStatus.InsufficientFunds);

            if (_rng.NextDouble() < NetworkWahalaChance)
            {
                // Money doesn't move; the UI shows "Pending... reversed" for the joke. Fee still charged.
                wallet.TrySpend(FlatFee, "transfer-fee");
                return Done(toContactId, amount, TransferStatus.PendingReversal);
            }

            wallet.TrySpend(amount + FlatFee, "transfer:" + toContactId);
            return Done(toContactId, amount, TransferStatus.Successful);
        }

        private TransferStatus Done(string to, long amount, TransferStatus status)
        {
            Transferred?.Invoke(to, amount, status);
            return status;
        }
    }

    public sealed class SocialPost
    {
        public string Author { get; set; }
        public string Handle { get; set; }
        public string Text { get; set; }
        public int Likes { get; set; }
        public int Reposts { get; set; }
        public double GameMinute { get; set; }
        public bool AboutPlayer { get; set; }
    }

    [Serializable]
    public sealed class SocialTemplate
    {
        /// <summary>Game event key, e.g. "mission_passed:lag_04", "checkpoint:Recorded", "ambient:owambe".</summary>
        public string Trigger { get; set; }
        public string Author { get; set; }
        public string Handle { get; set; }
        public string Text { get; set; }
        public bool AboutPlayer { get; set; }
        public int CloutDelta { get; set; }
    }

    /// <summary>
    /// "Yarns" — the satirical social feed. Reacts to what the player does (car chases, viral
    /// checkpoint videos, mission outcomes) plus ambient city chatter. All accounts are fictional.
    /// </summary>
    public sealed class SocialFeed
    {
        public const int MaxPosts = 100;

        private readonly List<SocialTemplate> _templates;
        private readonly List<SocialPost> _posts = new List<SocialPost>();
        private readonly Random _rng;

        public IReadOnlyList<SocialPost> Posts => _posts;

        public event Action<SocialPost, int> Posted; // post, clout delta

        public SocialFeed(IEnumerable<SocialTemplate> templates, Random rng)
        {
            _templates = new List<SocialTemplate>(templates);
            _rng = rng ?? throw new ArgumentNullException(nameof(rng));
        }

        /// <summary>Post a random template matching <paramref name="trigger"/>. Returns null if none match.</summary>
        public SocialPost React(string trigger, double gameMinute, string playerName = null)
        {
            var matches = _templates.FindAll(t => t.Trigger == trigger);
            if (matches.Count == 0) return null;
            var t = matches[_rng.Next(matches.Count)];

            var post = new SocialPost
            {
                Author = t.Author,
                Handle = t.Handle,
                Text = playerName == null ? t.Text : t.Text.Replace("{player}", playerName),
                Likes = _rng.Next(20, 5000) * (t.AboutPlayer ? 3 : 1),
                Reposts = _rng.Next(0, 800),
                GameMinute = gameMinute,
                AboutPlayer = t.AboutPlayer,
            };
            _posts.Insert(0, post);
            if (_posts.Count > MaxPosts) _posts.RemoveAt(_posts.Count - 1);
            Posted?.Invoke(post, t.CloutDelta);
            return post;
        }
    }
}
