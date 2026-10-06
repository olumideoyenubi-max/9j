using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Phone;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using NaijaHustle.Runtime.Missions;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.Phone
{
    public enum PhoneApp
    {
        Home,
        Gist,     // chats (WhatsApp-style)
        KoboPay,  // transfers / balance
        Yarns,    // satirical social feed
        MapAm,    // map + travel booking
        Camera,
    }

    /// <summary>
    /// In-game phone shell. Slides up from the bottom; each app is a panel. The UI is intentionally
    /// light (pooled rows, no heavy layout groups) to stay smooth on budget Android GPUs.
    /// </summary>
    public sealed class PhoneController : MonoBehaviour
    {
        [SerializeField] private RectTransform phoneRoot;
        [SerializeField] private GameObject[] appPanels; // index = (int)PhoneApp
        [SerializeField] private TMP_Text clockText;
        [SerializeField] private TMP_Text gistBadge;

        [Header("Gist")]
        [SerializeField] private RectTransform threadList;
        [SerializeField] private RectTransform messageList;
        [SerializeField] private ChatRow rowPrefab;
        [SerializeField] private Button acceptMissionButton;

        [Header("KoboPay")]
        [SerializeField] private TMP_Text balanceText;
        [SerializeField] private TMP_Text lastTransferText;

        [Header("Yarns")]
        [SerializeField] private RectTransform feedList;
        [SerializeField] private ChatRow postPrefab;

        private bool _open;
        private string _openThread;
        private string _pendingMission;

        private void Start()
        {
            var s = GameBootstrap.Session;
            s.Chat.Received += _ => RefreshBadge();
            s.Transfers.Transferred += OnTransferred;
            acceptMissionButton.onClick.AddListener(AcceptMission);
            Show(false);
            RefreshBadge();
        }

        private void Update()
        {
            var input = PlayerInputHub.Instance;
            if (input != null && input.Pressed(ActionButton.Phone)) Show(!_open);
            if (_open) clockText.text = $"{GameBootstrap.Session.Clock.Hour:00}:{GameBootstrap.Session.Clock.Minute:00}";
        }

        public void Show(bool open)
        {
            _open = open;
            phoneRoot.gameObject.SetActive(open);
            if (open) OpenApp((int)PhoneApp.Home);
        }

        /// <summary>Hooked to home-screen icon buttons.</summary>
        public void OpenApp(int app)
        {
            for (int i = 0; i < appPanels.Length; i++) appPanels[i].SetActive(i == app);
            switch ((PhoneApp)app)
            {
                case PhoneApp.Gist: BuildThreads(); break;
                case PhoneApp.KoboPay: balanceText.text = Wallet.Format(GameBootstrap.Session.Wallet.Balance); break;
                case PhoneApp.Yarns: BuildFeed(); break;
            }
        }

        private void BuildThreads()
        {
            Clear(threadList);
            var s = GameBootstrap.Session;
            foreach (var (contactId, last, unread) in s.Chat.Threads())
            {
                var c = s.Content.Data.Contacts.Find(x => x.Id == contactId);
                var row = Instantiate(rowPrefab, threadList);
                row.Set(c != null ? c.Name : contactId, last.Text, unread > 0 ? unread.ToString() : string.Empty);
                string id = contactId;
                row.Button.onClick.AddListener(() => OpenThread(id));
            }
        }

        private void OpenThread(string contactId)
        {
            var s = GameBootstrap.Session;
            _openThread = contactId;
            _pendingMission = null;
            s.Chat.MarkRead(contactId);
            RefreshBadge();
            Clear(messageList);
            foreach (var m in s.Chat.Thread(contactId))
            {
                var row = Instantiate(rowPrefab, messageList);
                row.Set(m.FromPlayer ? "You" : string.Empty, m.Text, string.Empty);
                row.AlignRight(m.FromPlayer);
                if (m.MissionId != null && s.Missions.Get(m.MissionId) is var def && def != null && s.Missions.IsAvailable(def))
                    _pendingMission = m.MissionId;
            }
            acceptMissionButton.gameObject.SetActive(_pendingMission != null);
        }

        private void AcceptMission()
        {
            if (_pendingMission == null) return;
            var s = GameBootstrap.Session;
            if (MissionDirector.TryStart(_pendingMission))
            {
                s.Chat.Send(_openThread, "I dey come. On my way.", s.Clock.TotalMinutes);
                Show(false);
            }
            else
            {
                s.Chat.Send(_openThread, s.Wanted.IsWanted ? "Wait, police dey my back. Give me small time." : "Not now, I'm busy.", s.Clock.TotalMinutes);
                OpenThread(_openThread);
            }
        }

        private void BuildFeed()
        {
            Clear(feedList);
            foreach (var p in GameBootstrap.Session.Social.Posts)
            {
                var row = Instantiate(postPrefab, feedList);
                row.Set($"{p.Author} {p.Handle}", p.Text, $"♥ {p.Likes:N0}  ↻ {p.Reposts:N0}");
            }
        }

        private void OnTransferred(string to, long amount, TransferStatus status)
        {
            lastTransferText.text = status switch
            {
                TransferStatus.Successful => $"Sent {Wallet.Format(amount)}",
                TransferStatus.PendingReversal => "Transaction pending... Reversed. Network wahala. Fee still charged 😑",
                TransferStatus.InsufficientFunds => "Insufficient funds. Hustle harder.",
                _ => "Invalid amount",
            };
            balanceText.text = Wallet.Format(GameBootstrap.Session.Wallet.Balance);
        }

        private void RefreshBadge()
        {
            int n = GameBootstrap.Session.Chat.UnreadCount;
            gistBadge.gameObject.SetActive(n > 0);
            gistBadge.text = n.ToString();
        }

        private static void Clear(RectTransform t)
        {
            for (int i = t.childCount - 1; i >= 0; i--) Destroy(t.GetChild(i).gameObject);
        }
    }
}
