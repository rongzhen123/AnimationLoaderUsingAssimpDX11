Shader "Custom/GpuCrowdInstanced"
{
    Properties
    {
        _BaseMap("Base Map", 2D) = "white" {}
        _Color("Color", Color) = (1,1,1,1)
    }

    SubShader
    {
        Tags { "RenderType"="Opaque" "Queue"="Geometry" }
        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 4.5

            #include "UnityCG.cginc"

            struct appdata
            {
                float4 vertex : POSITION;
                float3 normal : NORMAL;
                float2 uv : TEXCOORD0;
                float4 boneIndices : TEXCOORD2;
                float4 boneWeights : TEXCOORD3;
                uint instanceID : SV_InstanceID;
            };

            struct v2f
            {
                float2 uv : TEXCOORD0;
                float4 pos : SV_POSITION;
                float3 normalWS : TEXCOORD1;
            };

            struct InstanceData
            {
                float4x4 objectToWorld;
                uint clipIndex;
                float normalizedTime;
                float playbackSpeed;
                float blend;
                uint nextClip;
                float3 pad;
            };

            struct ClipMeta
            {
                int startFrame;
                int frameCount;
                float fps;
                float pad;
            };

            TEXTURE2D(_BaseMap);
            SAMPLER(sampler_BaseMap);

            Texture2D<float4> _AnimTex;
            StructuredBuffer<InstanceData> _InstanceBuffer;
            StructuredBuffer<uint> _VisibleIndexBuffer;
            StructuredBuffer<ClipMeta> _ClipMetaBuffer;

            int _BoneCount;
            float4 _Color;

            float3x4 LoadBoneMatrix(uint clipIndex, uint frameIndex, uint boneIndex)
            {
                ClipMeta meta = _ClipMetaBuffer[clipIndex];
                uint row = frameIndex + (uint)meta.startFrame;
                uint x = boneIndex * 3;

                float4 r0 = _AnimTex.Load(int3(x + 0, row, 0));
                float4 r1 = _AnimTex.Load(int3(x + 1, row, 0));
                float4 r2 = _AnimTex.Load(int3(x + 2, row, 0));

                return float3x4(r0, r1, r2);
            }

            float3 MulPoint(float3x4 m, float3 p)
            {
                return float3(dot(m[0], float4(p, 1.0)), dot(m[1], float4(p, 1.0)), dot(m[2], float4(p, 1.0)));
            }

            float3 MulDir(float3x4 m, float3 d)
            {
                return float3(dot(m[0].xyz, d), dot(m[1].xyz, d), dot(m[2].xyz, d));
            }

            v2f vert(appdata v)
            {
                uint visibleIndex = _VisibleIndexBuffer[v.instanceID];
                InstanceData ins = _InstanceBuffer[visibleIndex];
                ClipMeta clip = _ClipMetaBuffer[ins.clipIndex];

                uint frame = (uint)floor(frac(ins.normalizedTime) * max(clip.frameCount - 1, 1));

                float4 weights = v.boneWeights;
                float4 indices = v.boneIndices;

                float3 skinnedPos = float3(0,0,0);
                float3 skinnedNrm = float3(0,0,0);

                [unroll]
                for (int i = 0; i < 4; i++)
                {
                    float w = weights[i];
                    if (w <= 0.0001) continue;

                    uint bone = (uint)indices[i];
                    float3x4 bm = LoadBoneMatrix(ins.clipIndex, frame, bone);
                    skinnedPos += MulPoint(bm, v.vertex.xyz) * w;
                    skinnedNrm += MulDir(bm, v.normal) * w;
                }

                float4 world = mul(ins.objectToWorld, float4(skinnedPos, 1));
                float3 normalWS = normalize(mul((float3x3)ins.objectToWorld, skinnedNrm));

                v2f o;
                o.pos = mul(UNITY_MATRIX_VP, world);
                o.uv = v.uv;
                o.normalWS = normalWS;
                return o;
            }

            float4 frag(v2f i) : SV_Target
            {
                float3 l = normalize(float3(0.3, 1.0, 0.2));
                float ndl = saturate(dot(i.normalWS, l));
                float4 albedo = SAMPLE_TEXTURE2D(_BaseMap, sampler_BaseMap, i.uv) * _Color;
                return float4(albedo.rgb * (0.2 + 0.8 * ndl), albedo.a);
            }
            ENDHLSL
        }
    }
}
