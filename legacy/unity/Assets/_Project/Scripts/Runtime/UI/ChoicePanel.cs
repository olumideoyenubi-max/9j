using System;
using NaijaHustle.Runtime.Missions;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.UI
{
    /// <summary>Big-button moral choice overlay. Pauses time while open.</summary>
    public sealed class ChoicePanel : MonoBehaviour
    {
        [SerializeField] private GameObject root;
        [SerializeField] private Button[] buttons;
        [SerializeField] private TMP_Text[] labels;
        [SerializeField] private TMP_Text[] consequences;

        private Action<int> _onPick;

        public bool IsOpen => root.activeSelf;

        private void Awake()
        {
            for (int i = 0; i < buttons.Length; i++)
            {
                int idx = i;
                buttons[i].onClick.AddListener(() => Pick(idx));
            }
            root.SetActive(false);
        }

        public void Open(ChoicePrompt.Option[] options, Action<int> onPick)
        {
            _onPick = onPick;
            for (int i = 0; i < buttons.Length; i++)
            {
                bool used = i < options.Length;
                buttons[i].gameObject.SetActive(used);
                if (!used) continue;
                labels[i].text = options[i].label;
                consequences[i].text = options[i].consequence;
            }
            root.SetActive(true);
            Time.timeScale = 0f;
        }

        private void Pick(int i)
        {
            root.SetActive(false);
            Time.timeScale = 1f;
            var cb = _onPick;
            _onPick = null;
            cb?.Invoke(i);
        }
    }
}
