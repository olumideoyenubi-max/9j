using System;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Missions
{
    public enum ObjectiveType
    {
        /// <summary>Reach a marker (TargetId = marker id).</summary>
        GoTo,
        /// <summary>Get into a vehicle of a class or specific id.</summary>
        EnterVehicle,
        /// <summary>Bring an item/vehicle/person to a marker.</summary>
        Deliver,
        /// <summary>Keep an escorted entity alive until it reaches its destination.</summary>
        Escort,
        /// <summary>Catch or disable a fleeing target.</summary>
        Chase,
        /// <summary>Lose all wanted stars.</summary>
        LoseHeat,
        /// <summary>Pick up N items (Count).</summary>
        Collect,
        /// <summary>Move through an area without being detected.</summary>
        Stealth,
        /// <summary>Use the port/oil-yard crane to place Count containers.</summary>
        OperateCrane,
        /// <summary>Survive or protect a location for TimeLimit seconds.</summary>
        Defend,
        /// <summary>Take Count photos of evidence with the phone camera.</summary>
        Photograph,
        /// <summary>Speak to an NPC.</summary>
        Talk,
        /// <summary>Wait out a timer (cutscene beat, bus ride).</summary>
        Wait,
        /// <summary>Make a dialogue choice; TargetId = choice set id.</summary>
        Choose,
    }

    [Serializable]
    public sealed class ObjectiveDefinition
    {
        public ObjectiveType Type { get; set; }
        public string TargetId { get; set; }
        public int Count { get; set; } = 1;
        /// <summary>Seconds; 0 = no limit.</summary>
        public float TimeLimit { get; set; }
        /// <summary>HUD text, e.g. "Collect fares from 6 passengers".</summary>
        public string Text { get; set; }
        /// <summary>Optional objectives can be failed without failing the mission (bonus pay).</summary>
        public bool Optional { get; set; }
    }

    [Serializable]
    public sealed class MissionDefinition
    {
        public string Id { get; set; }
        public CityId City { get; set; }
        public string Title { get; set; }
        /// <summary>Contact id of the mission giver (phone + map blip).</summary>
        public string Giver { get; set; }
        public string Summary { get; set; }
        /// <summary>Story order inside the city (1-based). Side missions use 0.</summary>
        public int Order { get; set; }
        public bool IsStory { get; set; } = true;
        public string[] Requires { get; set; } = Array.Empty<string>();
        public string[] RequiresFlags { get; set; } = Array.Empty<string>();
        public ObjectiveDefinition[] Objectives { get; set; } = Array.Empty<ObjectiveDefinition>();
        public long Reward { get; set; }
        public int CredReward { get; set; }
        public int IntegrityDelta { get; set; }
        /// <summary>Flags set on completion, e.g. "flight_unlocked".</summary>
        public string[] SetsFlags { get; set; } = Array.Empty<string>();
        public CityId? UnlocksCity { get; set; }
        /// <summary>Addressables scene/prefab key containing this mission's set dressing.</summary>
        public string AssetKey { get; set; }
        /// <summary>Should the player be wanted-free to start it?</summary>
        public bool RequiresNoHeat { get; set; } = true;
    }
}
