using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// Generic in-world objective hook placed inside mission set-dressing prefabs. One component
    /// covers the common verbs: Collect (walk/drive into it), Talk (Interact nearby),
    /// Photograph (aim phone camera at it), Chase (ram/catch it), Escort (dies → fail).
    /// </summary>
    public sealed class ObjectiveTarget : MonoBehaviour
    {
        [SerializeField] private ObjectiveType type = ObjectiveType.Collect;
        [SerializeField] private string targetId;
        [SerializeField] private float interactRange = 2.5f;
        [SerializeField] private bool disableWhenDone = true;
        [Tooltip("Escort / Chase: hit points before it's destroyed (Escort fails, Chase succeeds).")]
        [SerializeField] private float health = 100f;

        private bool _done;

        public ObjectiveType Type => type;
        public string TargetId => targetId;

        private bool IsCurrent
        {
            get
            {
                var obj = GameBootstrap.Session?.Missions.Active?.CurrentObjective;
                return obj != null && obj.Type == type && obj.TargetId == targetId;
            }
        }

        private void Update()
        {
            if (_done || type != ObjectiveType.Talk || !IsCurrent) return;
            var player = GameBootstrap.Instance.Player;
            var input = PlayerInputHub.Instance;
            if (player == null || input == null) return;
            if ((player.position - transform.position).sqrMagnitude < interactRange * interactRange && input.Pressed(ActionButton.Interact))
                Complete();
        }

        private void OnTriggerEnter(Collider other)
        {
            if (_done || !other.CompareTag("Player") || !IsCurrent) return;
            if (type == ObjectiveType.Collect || type == ObjectiveType.Chase) Complete();
        }

        /// <summary>Called by <see cref="PhoneCamera"/> when this is in frame and in focus.</summary>
        public void Photographed()
        {
            if (!_done && type == ObjectiveType.Photograph && IsCurrent) Complete();
        }

        public void Damage(float amount)
        {
            if (_done) return;
            health -= amount;
            if (health > 0f) return;

            var board = GameBootstrap.Session.Missions;
            if (type == ObjectiveType.Escort) board.Active?.Fail("The escort was destroyed");
            else if (type == ObjectiveType.Chase && IsCurrent) Complete();
        }

        /// <summary>Escort reached its destination — called by a MissionMarker or path script.</summary>
        public void Arrived()
        {
            if (type == ObjectiveType.Escort && IsCurrent) Complete();
        }

        private void Complete()
        {
            GameBootstrap.Session.Missions.Report(new ObjectiveEvent(type, targetId));
            _done = true;
            // Photo subjects stay visible; several distinct objects can share one target id (count = 3).
            if (disableWhenDone && type != ObjectiveType.Photograph) gameObject.SetActive(false);
        }
    }
}
