using TMPro;
using UnityEngine;

namespace NaijaHustle.Runtime.UI
{
    /// <summary>Short top-of-screen notification (radio station, "Saved", new chat).</summary>
    public sealed class Toast : MonoBehaviour
    {
        [SerializeField] private TMP_Text label;
        [SerializeField] private CanvasGroup group;
        [SerializeField] private float seconds = 2.5f;

        private float _t;

        public void Show(string text)
        {
            label.text = text;
            _t = seconds;
        }

        private void Update()
        {
            _t -= Time.unscaledDeltaTime;
            group.alpha = Mathf.Clamp01(_t * 2f);
        }
    }
}
