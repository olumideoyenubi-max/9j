using UnityEngine;
using UnityEngine.EventSystems;

namespace NaijaHustle.Runtime.Input
{
    /// <summary>Right side of the screen: drag to orbit the camera.</summary>
    public sealed class TouchLookArea : MonoBehaviour, IDragHandler
    {
        [SerializeField] private float sensitivity = 0.15f;

        public void OnDrag(PointerEventData e)
        {
            if (PlayerInputHub.Instance == null) return;
            // Normalize by DPI so feel is the same on a 5" budget phone and a 7" tablet.
            float dpiScale = Screen.dpi > 0 ? 160f / Screen.dpi : 1f;
            PlayerInputHub.Instance.TouchLookDelta += e.delta * (sensitivity * dpiScale);
        }
    }
}
