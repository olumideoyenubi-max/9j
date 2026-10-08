using UnityEngine;
using UnityEngine.EventSystems;

namespace NaijaHustle.Runtime.Input
{
    /// <summary>On-screen action button. Context buttons (enter car, talk) toggle visibility via <see cref="ContextVisible"/>.</summary>
    public sealed class TouchButton : MonoBehaviour, IPointerDownHandler, IPointerUpHandler, IPointerExitHandler
    {
        [SerializeField] private ActionButton action;
        [SerializeField] private CanvasGroup group;

        public ActionButton Action => action;

        public bool ContextVisible
        {
            set
            {
                if (group == null) return;
                group.alpha = value ? 1f : 0f;
                group.blocksRaycasts = value;
            }
        }

        public void OnPointerDown(PointerEventData e) => Set(true);
        public void OnPointerUp(PointerEventData e) => Set(false);
        public void OnPointerExit(PointerEventData e) => Set(false);
        private void OnDisable() => Set(false);

        private void Set(bool down)
        {
            if (PlayerInputHub.Instance != null) PlayerInputHub.Instance.SetTouchButton(action, down);
        }
    }
}
