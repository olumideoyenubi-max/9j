using System;

namespace NaijaHustle.Core.Missions
{
    public enum MissionState
    {
        Running,
        Passed,
        Failed,
        Abandoned,
    }

    /// <summary>A game event reported by the runtime layer that may advance an objective.</summary>
    public readonly struct ObjectiveEvent
    {
        public readonly ObjectiveType Type;
        public readonly string TargetId;
        public readonly int Amount;

        public ObjectiveEvent(ObjectiveType type, string targetId, int amount = 1)
        {
            Type = type;
            TargetId = targetId;
            Amount = amount;
        }
    }

    /// <summary>One play-through of a mission: walks objectives in order, tracks counts and timers.</summary>
    public sealed class MissionRun
    {
        private float _objectiveTimer;

        public MissionDefinition Definition { get; }
        public int ObjectiveIndex { get; private set; }
        public int Progress { get; private set; }
        public MissionState State { get; private set; } = MissionState.Running;
        public string FailReason { get; private set; }
        public int OptionalFailed { get; private set; }

        public ObjectiveDefinition CurrentObjective =>
            State == MissionState.Running && ObjectiveIndex < Definition.Objectives.Length ? Definition.Objectives[ObjectiveIndex] : null;

        /// <summary>Seconds left on the current objective, or null if untimed.</summary>
        public float? TimeRemaining
        {
            get
            {
                var obj = CurrentObjective;
                if (obj == null || obj.TimeLimit <= 0f) return null;
                return Math.Max(0f, obj.TimeLimit - _objectiveTimer);
            }
        }

        public event Action<ObjectiveDefinition> ObjectiveStarted;
        public event Action<MissionRun> Finished;

        public MissionRun(MissionDefinition definition)
        {
            Definition = definition ?? throw new ArgumentNullException(nameof(definition));
            if (definition.Objectives.Length == 0) throw new ArgumentException("Mission has no objectives: " + definition.Id);
        }

        public void Begin() => ObjectiveStarted?.Invoke(CurrentObjective);

        public void Tick(float deltaSeconds)
        {
            var obj = CurrentObjective;
            if (obj == null) return;
            _objectiveTimer += deltaSeconds;

            if (obj.Type == ObjectiveType.Wait || obj.Type == ObjectiveType.Defend)
            {
                // Surviving the timer IS the objective.
                if (obj.TimeLimit > 0f && _objectiveTimer >= obj.TimeLimit) Advance();
                return;
            }

            if (obj.TimeLimit > 0f && _objectiveTimer >= obj.TimeLimit)
            {
                if (obj.Optional) { OptionalFailed++; Advance(); }
                else Fail("Time up");
            }
        }

        /// <returns>true if the event matched the current objective.</returns>
        public bool Report(ObjectiveEvent e)
        {
            var obj = CurrentObjective;
            if (obj == null || obj.Type != e.Type) return false;
            if (!string.IsNullOrEmpty(obj.TargetId) && !string.Equals(obj.TargetId, e.TargetId, StringComparison.Ordinal)) return false;

            Progress += Math.Max(1, e.Amount);
            if (Progress >= Math.Max(1, obj.Count)) Advance();
            return true;
        }

        /// <summary>Escort died, cover blown in a Stealth objective, cargo destroyed...</summary>
        public void Fail(string reason)
        {
            if (State != MissionState.Running) return;
            var obj = CurrentObjective;
            if (obj != null && obj.Optional)
            {
                OptionalFailed++;
                Advance();
                return;
            }
            State = MissionState.Failed;
            FailReason = reason;
            Finished?.Invoke(this);
        }

        public void Abandon()
        {
            if (State != MissionState.Running) return;
            State = MissionState.Abandoned;
            Finished?.Invoke(this);
        }

        private void Advance()
        {
            ObjectiveIndex++;
            Progress = 0;
            _objectiveTimer = 0f;
            if (ObjectiveIndex >= Definition.Objectives.Length)
            {
                State = MissionState.Passed;
                Finished?.Invoke(this);
                return;
            }
            ObjectiveStarted?.Invoke(CurrentObjective);
        }
    }
}
