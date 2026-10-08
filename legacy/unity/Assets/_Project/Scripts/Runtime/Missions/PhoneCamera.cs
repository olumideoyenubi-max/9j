using NaijaHustle.Runtime.Input;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// The phone camera used for Photograph objectives (fake aso-ebi labels, dumping points, the
    /// ledger). Raises the phone, narrows FOV, and "snaps" any ObjectiveTarget near screen centre.
    /// </summary>
    public sealed class PhoneCamera : MonoBehaviour
    {
        [SerializeField] private Camera worldCamera;
        [SerializeField] private GameObject viewfinderUi;
        [SerializeField] private float zoomFov = 30f;
        [SerializeField] private float maxDistance = 25f;
        [SerializeField] private float centerTolerance = 0.12f;
        [SerializeField] private AudioSource shutter;

        private float _defaultFov;
        private bool _active;

        private void Awake() => _defaultFov = worldCamera.fieldOfView;

        private void Update()
        {
            var input = PlayerInputHub.Instance;
            if (input == null) return;

            if (input.Pressed(ActionButton.Camera))
            {
                _active = !_active;
                viewfinderUi.SetActive(_active);
            }

            worldCamera.fieldOfView = Mathf.Lerp(worldCamera.fieldOfView, _active ? zoomFov : _defaultFov, 10f * Time.deltaTime);
            if (_active && input.Pressed(ActionButton.Attack)) Snap();
        }

        private void Snap()
        {
            if (shutter != null) shutter.Play();
            foreach (var t in FindObjectsByType<ObjectiveTarget>(FindObjectsSortMode.None))
            {
                if (t.Type != Core.Missions.ObjectiveType.Photograph) continue;
                Vector3 vp = worldCamera.WorldToViewportPoint(t.transform.position);
                if (vp.z <= 0f || vp.z > maxDistance) continue;
                if (Mathf.Abs(vp.x - 0.5f) > centerTolerance || Mathf.Abs(vp.y - 0.5f) > centerTolerance) continue;
                if (Physics.Linecast(worldCamera.transform.position, t.transform.position, 1, QueryTriggerInteraction.Ignore)) continue;
                t.Photographed();
                return;
            }
        }
    }
}
