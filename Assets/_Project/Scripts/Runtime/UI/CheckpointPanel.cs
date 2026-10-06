using System;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Runtime.Bootstrap;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.UI
{
    /// <summary>
    /// The checkpoint encounter. Satire first: the officer's lines, the "settle" price in naira and
    /// a reminder that phones are always recording. Outcome text feeds the Yarns social app.
    /// </summary>
    public sealed class CheckpointPanel : MonoBehaviour
    {
        [SerializeField] private GameObject root;
        [SerializeField] private TMP_Text officerLine;
        [SerializeField] private TMP_Text settleLabel;
        [SerializeField] private Button comply, settle, talk, floor;
        [SerializeField] private TMP_Text outcomeText;
        [SerializeField] private string[] openers =
        {
            "\"Oga, good afternoon. Particulars.\"",
            "\"Where you dey go? Wetin dey that boot?\"",
            "\"Na you get this motor? Park well.\"",
            "\"Anything for the boys?\"",
        };

        private Action<CheckpointChoice> _onChoice;

        private void Awake()
        {
            comply.onClick.AddListener(() => Choose(CheckpointChoice.Comply));
            settle.onClick.AddListener(() => Choose(CheckpointChoice.Settle));
            talk.onClick.AddListener(() => Choose(CheckpointChoice.Talk));
            floor.onClick.AddListener(() => Choose(CheckpointChoice.Floor));
            root.SetActive(false);
        }

        public void Open(Action<CheckpointChoice> onChoice)
        {
            _onChoice = onChoice;
            var rules = GameBootstrap.Session.CurrentCityDef?.Checkpoints;
            officerLine.text = openers[UnityEngine.Random.Range(0, openers.Length)];
            settleLabel.text = "Settle (" + Wallet.Format(rules != null ? rules.BaseSettleAmount : 0) + ")";
            outcomeText.text = string.Empty;
            SetButtons(true);
            root.SetActive(true);
        }

        public void ShowOutcome(CheckpointOutcome o)
        {
            outcomeText.text = o.Flavour;
            SetButtons(false);
            Invoke(nameof(Close), 3f);
        }

        private void Choose(CheckpointChoice c)
        {
            var cb = _onChoice;
            _onChoice = null;
            cb?.Invoke(c);
        }

        private void SetButtons(bool on)
        {
            comply.interactable = settle.interactable = talk.interactable = floor.interactable = on;
        }

        private void Close() => root.SetActive(false);
    }
}
