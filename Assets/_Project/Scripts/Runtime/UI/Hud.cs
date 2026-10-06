using NaijaHustle.Core.Economy;
using NaijaHustle.Core.World;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Missions;
using NaijaHustle.Runtime.World;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.UI
{
    /// <summary>Cash, wanted stars, objective text + timer, district banner, clock and stealth meter.</summary>
    public sealed class Hud : MonoBehaviour
    {
        [SerializeField] private TMP_Text cash;
        [SerializeField] private TMP_Text cashDelta;
        [SerializeField] private Image[] stars;
        [SerializeField] private TMP_Text objective;
        [SerializeField] private TMP_Text timer;
        [SerializeField] private TMP_Text clock;
        [SerializeField] private TMP_Text districtBanner;
        [SerializeField] private CanvasGroup districtBannerGroup;
        [SerializeField] private Image stealthMeter;
        [SerializeField] private TMP_Text hustleLine;
        [SerializeField] private Hustles.DanfoConductorHustle conductor;

        private float _deltaTimer;
        private float _bannerTimer;

        private void Start()
        {
            var s = GameBootstrap.Session;
            s.Wallet.Changed += (bal, delta, reason) =>
            {
                cashDelta.text = (delta > 0 ? "+" : "") + Wallet.Format(delta);
                cashDelta.color = delta > 0 ? new Color(0.3f, 0.9f, 0.4f) : new Color(0.95f, 0.35f, 0.3f);
                _deltaTimer = 2.5f;
            };
            DistrictZone.Entered += OnDistrict;
        }

        private void OnDestroy() => DistrictZone.Entered -= OnDistrict;

        private void OnDistrict(DistrictZone zone)
        {
            var city = GameBootstrap.Session.CurrentCityDef;
            var def = city != null ? System.Array.Find(city.Districts, d => d.Id == zone.DistrictId) : null;
            if (def == null) return;
            districtBanner.text = def.Name.ToUpperInvariant();
            _bannerTimer = 3f;
        }

        private void Update()
        {
            var s = GameBootstrap.Session;
            if (s == null) return;

            cash.text = Wallet.Format(s.Wallet.Balance);
            _deltaTimer -= Time.deltaTime;
            cashDelta.gameObject.SetActive(_deltaTimer > 0f);

            // Stars flash while the search timer is running (player out of sight).
            int n = s.Wanted.Stars;
            bool flash = !s.Wanted.PlayerSeen && s.Wanted.IsWanted && Mathf.Repeat(Time.time * (1f + 3f * s.Wanted.SearchProgress), 1f) > 0.5f;
            for (int i = 0; i < stars.Length; i++)
            {
                stars[i].enabled = i < n;
                stars[i].color = flash ? new Color(1f, 1f, 1f, 0.35f) : Color.white;
            }

            var run = s.Missions.Active;
            var obj = run?.CurrentObjective;
            objective.gameObject.SetActive(obj != null);
            if (obj != null)
            {
                string count = obj.Count > 1 ? $" ({run.Progress}/{obj.Count})" : string.Empty;
                objective.text = obj.Text + count;
            }

            float? t = run?.TimeRemaining;
            timer.gameObject.SetActive(t.HasValue);
            if (t.HasValue) timer.text = $"{(int)t.Value / 60}:{(int)t.Value % 60:00}";

            clock.text = $"{s.Clock.Hour:00}:{s.Clock.Minute:00}";

            _bannerTimer -= Time.deltaTime;
            districtBannerGroup.alpha = Mathf.Clamp01(_bannerTimer);

            var stealth = StealthZone.Active;
            stealthMeter.gameObject.SetActive(stealth != null);
            if (stealth != null) stealthMeter.fillAmount = stealth.Suspicion;

            bool onShift = conductor != null && conductor.OnShift;
            hustleLine.gameObject.SetActive(onShift);
            if (onShift)
            {
                var shift = s.ActiveShift;
                hustleLine.text = $"\"{conductor.CurrentCall}\"  •  Streak x{shift.Streak}  •  {Wallet.Format(shift.Earned)}";
            }
        }
    }
}
