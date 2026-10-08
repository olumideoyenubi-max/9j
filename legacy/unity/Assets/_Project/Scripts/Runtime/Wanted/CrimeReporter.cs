using NaijaHustle.Core.Wanted;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.Wanted
{
    /// <summary>
    /// Turns a crime at a position into a report. Witnesses are any colliders on the Witness layer
    /// (pedestrians, drivers, responders, CCTV in Abuja) within range with line of sight.
    /// </summary>
    public static class CrimeReporter
    {
        public static float WitnessRange = 35f;
        public static LayerMask WitnessMask = 1 << 9;   // "Witness" layer — see docs/TECH.md
        public static LayerMask OcclusionMask = 1 << 0; // Default (buildings)

        private static readonly Collider[] Hits = new Collider[16];

        /// <param name="witnessBias">0..1 — chance a lone witness actually reports it (a carjacking with the driver inside = 1).</param>
        public static void Report(CrimeType crime, Vector3 at, float witnessBias = 1f)
        {
            var session = GameBootstrap.Session;
            if (session == null) return;
            session.Wanted.Report(crime, IsWitnessed(at, witnessBias));
        }

        public static bool IsWitnessed(Vector3 at, float bias)
        {
            int n = Physics.OverlapSphereNonAlloc(at, WitnessRange, Hits, WitnessMask, QueryTriggerInteraction.Collide);
            for (int i = 0; i < n; i++)
            {
                Vector3 eye = Hits[i].bounds.center + Vector3.up * 0.6f;
                if (Physics.Linecast(eye, at + Vector3.up, OcclusionMask, QueryTriggerInteraction.Ignore)) continue;
                if (Random.value <= bias) return true;
            }
            return false;
        }
    }
}
