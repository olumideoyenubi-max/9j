using System.Collections;
using NaijaHustle.Core.Travel;
using NaijaHustle.Core.World;
using NaijaHustle.Runtime.Bootstrap;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.World
{
    /// <summary>
    /// Interstate travel UI and flow (opened from the motor park / airport counters or the MapAm
    /// phone app). Bus = cutscene with mini-event cards while the next city streams in.
    /// Road = loads the shared highway scene with checkpoints. Flight = short cutscene.
    /// </summary>
    public sealed class TravelController : MonoBehaviour
    {
        [SerializeField] private CityStreamer streamer;
        [SerializeField] private GameObject busCutscene;
        [SerializeField] private GameObject flightCutscene;
        [SerializeField] private GameObject miniEventCard;
        [SerializeField] private TMP_Text miniEventText;
        [SerializeField] private Button choiceA, choiceB;
        [SerializeField] private TMP_Text choiceAText, choiceBText;
        [SerializeField] private TMP_Text messageText;
        [SerializeField] private GameObject highwayCutscene;
        [SerializeField] private UI.CheckpointPanel checkpointPanel;

        private bool _choiceMade;

        public TravelQuote Quote(CityId to, TravelMode mode)
        {
            var s = GameBootstrap.Session;
            return s.Travel.Quote(s.Story.CurrentCity, to, mode, s.Story, s.Wallet, s.Wanted.IsWanted);
        }

        /// <summary>Hooked to the destination/mode buttons.</summary>
        public void Go(CityId to, TravelMode mode)
        {
            var s = GameBootstrap.Session;
            var quote = Quote(to, mode);
            if (!quote.Allowed) { messageText.text = quote.BlockedReason; return; }

            var plan = s.Travel.Book(quote, s.Wallet);
            if (plan == null) return;

            switch (mode)
            {
                case TravelMode.LuxuryBus:
                    streamer.EnterCity(to, () => BusRide(plan));
                    break;
                case TravelMode.Flight:
                    streamer.EnterCity(to, () => Cutscene(flightCutscene, 6f, quote.Hours));
                    break;
                case TravelMode.Road:
                    streamer.EnterCity(to, () => RoadTrip(plan));
                    break;
            }
        }

        /// <summary>
        /// Self-drive: a highway montage punctuated by checkpoint encounters. (A drivable highway
        /// scene can replace the montage later; the checkpoint logic stays the same.)
        /// </summary>
        private IEnumerator RoadTrip(TravelPlan plan)
        {
            highwayCutscene.SetActive(true);
            var s = GameBootstrap.Session;
            int stops = plan.Quote.Checkpoints;
            float hoursPerLeg = plan.Quote.Hours / Mathf.Max(1, stops + 1);

            for (int i = 0; i < stops; i++)
            {
                yield return new WaitForSecondsRealtime(2.5f);
                s.Clock.Advance(hoursPerLeg * 60f);

                bool resolved = false;
                checkpointPanel.Open(choice =>
                {
                    checkpointPanel.ShowOutcome(s.ResolveCheckpoint(choice, Wanted.CheckpointTrigger.PlayerCarryingCargo));
                    resolved = true;
                });
                while (!resolved) yield return null;
                yield return new WaitForSecondsRealtime(3f);
            }

            s.Clock.Advance(hoursPerLeg * 60f);
            highwayCutscene.SetActive(false);
        }

        private IEnumerator BusRide(TravelPlan plan)
        {
            busCutscene.SetActive(true);
            var s = GameBootstrap.Session;
            float hoursPerEvent = plan.Quote.Hours / Mathf.Max(1, plan.MiniEvents.Count);

            foreach (var e in plan.MiniEvents)
            {
                yield return new WaitForSecondsRealtime(2f);
                miniEventText.text = e.Text;
                choiceAText.text = string.IsNullOrEmpty(e.ChoiceA) ? "OK" : e.ChoiceA;
                choiceBText.text = e.ChoiceB ?? string.Empty;
                choiceB.gameObject.SetActive(!string.IsNullOrEmpty(e.ChoiceB));
                miniEventCard.SetActive(true);

                _choiceMade = false;
                bool pickedA = false;
                choiceA.onClick.RemoveAllListeners();
                choiceB.onClick.RemoveAllListeners();
                choiceA.onClick.AddListener(() => { pickedA = true; _choiceMade = true; });
                choiceB.onClick.AddListener(() => { _choiceMade = true; });

                float timeout = 12f;
                while (!_choiceMade && (timeout -= Time.unscaledDeltaTime) > 0f) yield return null;
                miniEventCard.SetActive(false);

                if (pickedA && s.Wallet.TrySpend(e.CostA, "bus:" + e.Id))
                {
                    s.Profile.AddClout(e.CloutA);
                    s.Profile.AddIntegrity(e.IntegrityA);
                }
                s.Clock.Advance(hoursPerEvent * 60f);
            }

            yield return new WaitForSecondsRealtime(1.5f);
            busCutscene.SetActive(false);
        }

        private IEnumerator Cutscene(GameObject root, float seconds, float gameHours)
        {
            root.SetActive(true);
            yield return new WaitForSecondsRealtime(seconds);
            GameBootstrap.Session.Clock.Advance(gameHours * 60f);
            root.SetActive(false);
        }
    }
}
