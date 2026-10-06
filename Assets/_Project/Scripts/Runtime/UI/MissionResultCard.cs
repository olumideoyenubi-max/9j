using NaijaHustle.Core.Economy;
using TMPro;
using UnityEngine;

namespace NaijaHustle.Runtime.UI
{
    /// <summary>"MISSION PASSED / +₦60,000 / +40 cred" and "WASTED-style" fail card (own wording: "WAHALA!").</summary>
    public sealed class MissionResultCard : MonoBehaviour
    {
        [SerializeField] private CanvasGroup group;
        [SerializeField] private TMP_Text headline;
        [SerializeField] private TMP_Text title;
        [SerializeField] private TMP_Text detail;
        [SerializeField] private AudioSource passSting;
        [SerializeField] private AudioSource failSting;
        [SerializeField] private float seconds = 4f;

        private float _t;

        public void ShowPassed(string missionTitle, long pay, int cred)
        {
            headline.text = "JOB DONE!";
            title.text = missionTitle;
            detail.text = $"+{Wallet.Format(pay)}   +{cred} cred";
            _t = seconds;
            if (passSting != null) passSting.Play();
        }

        public void ShowFailed(string missionTitle, string reason)
        {
            headline.text = "WAHALA!";
            title.text = missionTitle;
            detail.text = reason;
            _t = seconds;
            if (failSting != null) failSting.Play();
        }

        private void Update()
        {
            _t -= Time.unscaledDeltaTime;
            group.alpha = Mathf.Clamp01(_t);
        }
    }
}
