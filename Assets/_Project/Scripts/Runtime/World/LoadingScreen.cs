using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.World
{
    /// <summary>Full-screen loader with city name, tagline, progress bar and rotating tips.</summary>
    public sealed class LoadingScreen : MonoBehaviour
    {
        [SerializeField] private CanvasGroup group;
        [SerializeField] private TMP_Text cityName;
        [SerializeField] private TMP_Text tagline;
        [SerializeField] private TMP_Text status;
        [SerializeField] private TMP_Text tip;
        [SerializeField] private Image bar;
        [SerializeField, TextArea] private string[] tips =
        {
            "Danfo conductors earn more when passengers arrive with their wigs still on.",
            "In Port Harcourt, Marine Patrol owns the water. Get to land to change the game.",
            "Abuja security escalates fast. Dress well; protocol notices.",
            "Settling at checkpoints is quick. Someone is always filming.",
            "Collect business income often — the cash box has a limit.",
        };

        private float _tipTimer;

        public void Show(string city, string line)
        {
            gameObject.SetActive(true);
            group.alpha = 1f;
            group.blocksRaycasts = true;
            cityName.text = city.ToUpperInvariant();
            tagline.text = line;
            NextTip();
        }

        public void SetProgress(float p, string text)
        {
            bar.fillAmount = Mathf.Clamp01(p);
            status.text = text;
        }

        public void Hide()
        {
            group.alpha = 0f;
            group.blocksRaycasts = false;
            gameObject.SetActive(false);
        }

        private void Update()
        {
            _tipTimer += Time.unscaledDeltaTime;
            if (_tipTimer > 6f) NextTip();
        }

        private void NextTip()
        {
            _tipTimer = 0f;
            if (tips.Length > 0) tip.text = tips[Random.Range(0, tips.Length)];
        }
    }
}
