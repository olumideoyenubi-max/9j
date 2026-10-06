using System;
using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// Moral-choice beats (lag_05 Area Toll, ph_09 Oil Money, abj_06 Haruna, abj_09 ending).
    /// Each option sets a story flag and nudges Integrity, so later dialogue and the ending can react.
    /// </summary>
    public sealed class ChoicePrompt : MonoBehaviour
    {
        [Serializable]
        public struct Option
        {
            public string label;
            [TextArea] public string consequence;
            public string setsFlag;
            public int integrityDelta;
            public long cash;
        }

        [SerializeField] private string choiceId;
        [SerializeField] private Option[] options;
        [SerializeField] private UI.ChoicePanel panel;

        private void Update()
        {
            var obj = GameBootstrap.Session?.Missions.Active?.CurrentObjective;
            if (obj == null || obj.Type != ObjectiveType.Choose || obj.TargetId != choiceId || panel.IsOpen) return;
            panel.Open(options, Pick);
        }

        private void Pick(int index)
        {
            var s = GameBootstrap.Session;
            var o = options[index];
            s.Story.SetFlag(o.setsFlag);
            s.Profile.AddIntegrity(o.integrityDelta);
            if (o.cash > 0) s.Wallet.Earn(o.cash, "choice:" + choiceId);
            s.Missions.Report(new ObjectiveEvent(ObjectiveType.Choose, choiceId));
        }
    }
}
