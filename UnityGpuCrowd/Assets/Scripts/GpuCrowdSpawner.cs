using UnityEngine;

namespace GpuCrowd
{
    public sealed class GpuCrowdSpawner : MonoBehaviour
    {
        [SerializeField] private GpuCrowdAnimator crowdAnimator;

        private void Reset()
        {
            crowdAnimator = GetComponent<GpuCrowdAnimator>();
        }

        private void OnValidate()
        {
            if (crowdAnimator == null)
            {
                crowdAnimator = GetComponent<GpuCrowdAnimator>();
            }
        }
    }
}
