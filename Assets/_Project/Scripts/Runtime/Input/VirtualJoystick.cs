using UnityEngine;
using UnityEngine.EventSystems;

namespace NaijaHustle.Runtime.Input
{
    /// <summary>
    /// Floating left-thumb joystick. Appears where the thumb lands inside its area, so it works for
    /// small and large hands and different phone aspect ratios.
    /// </summary>
    public sealed class VirtualJoystick : MonoBehaviour, IPointerDownHandler, IDragHandler, IPointerUpHandler
    {
        [SerializeField] private RectTransform area;
        [SerializeField] private RectTransform background;
        [SerializeField] private RectTransform knob;
        [SerializeField] private float radius = 90f;
        [SerializeField, Range(0f, 0.5f)] private float deadZone = 0.12f;

        private Vector2 _origin;
        private Canvas _canvas;

        private void Awake()
        {
            _canvas = GetComponentInParent<Canvas>();
            background.gameObject.SetActive(false);
        }

        public void OnPointerDown(PointerEventData e)
        {
            RectTransformUtility.ScreenPointToLocalPointInRectangle(area, e.position, Cam, out _origin);
            background.anchoredPosition = _origin;
            knob.anchoredPosition = Vector2.zero;
            background.gameObject.SetActive(true);
            OnDrag(e);
        }

        public void OnDrag(PointerEventData e)
        {
            RectTransformUtility.ScreenPointToLocalPointInRectangle(area, e.position, Cam, out var p);
            var delta = Vector2.ClampMagnitude(p - _origin, radius);
            knob.anchoredPosition = delta;

            var v = delta / radius;
            float m = v.magnitude;
            v = m < deadZone ? Vector2.zero : v.normalized * ((m - deadZone) / (1f - deadZone));
            if (PlayerInputHub.Instance != null) PlayerInputHub.Instance.TouchMove = v;
        }

        public void OnPointerUp(PointerEventData e)
        {
            background.gameObject.SetActive(false);
            if (PlayerInputHub.Instance != null) PlayerInputHub.Instance.TouchMove = Vector2.zero;
        }

        private Camera Cam => _canvas != null && _canvas.renderMode != RenderMode.ScreenSpaceOverlay ? _canvas.worldCamera : null;
    }
}
