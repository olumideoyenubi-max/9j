using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace NaijaHustle.Runtime.Phone
{
    /// <summary>One row in a phone list: chat thread, chat bubble or social post.</summary>
    public sealed class ChatRow : MonoBehaviour
    {
        [SerializeField] private TMP_Text title;
        [SerializeField] private TMP_Text body;
        [SerializeField] private TMP_Text meta;
        [SerializeField] private Button button;
        [SerializeField] private HorizontalLayoutGroup layout;

        public Button Button => button;

        public void Set(string t, string b, string m)
        {
            title.text = t;
            title.gameObject.SetActive(!string.IsNullOrEmpty(t));
            body.text = b;
            meta.text = m;
        }

        public void AlignRight(bool right)
        {
            if (layout != null) layout.childAlignment = right ? TextAnchor.UpperRight : TextAnchor.UpperLeft;
        }
    }
}
