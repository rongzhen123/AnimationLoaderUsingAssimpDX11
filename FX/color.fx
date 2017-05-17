//***************************************************************************************
// color.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject
{
	float4x4 gWorldViewProj; 
};

cbuffer cbSkinned
{
	float4x4 gBoneTransforms[96];
};

// Nonnumeric values cannot be added to a cbuffer.
Texture2D gDiffuseMap;
//Texture2D gNormalMap;

SamplerState samLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct VertexIn
{
	float3 PosL  : POSITION;
  //  float4 Color : COLOR;
//	float3 NormalL    : NORMAL;
	float2 Tex        : TEXCOORD;
//	float4 TangentL   : TANGENT;
	float4 Weights    : WEIGHTS;
	float4 BoneIndices : BONEINDICES;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
	float2 Tex        : TEXCOORD;
    //float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Init array or else we get strange warnings about SV_POSITION.
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	weights[0] = vin.Weights.x;
	weights[1] = vin.Weights.y;
	weights[2] = vin.Weights.z;
	weights[3] = vin.Weights.w;/*1.0f - weights[0] - weights[1] - weights[2]*/;
	
	float4x4 BoneTransform = gBoneTransforms[vin.BoneIndices[0]] * vin.Weights[0];
	BoneTransform += gBoneTransforms[vin.BoneIndices[1]] * vin.Weights[1];
	BoneTransform += gBoneTransforms[vin.BoneIndices[2]] * vin.Weights[2];
	BoneTransform += gBoneTransforms[vin.BoneIndices[3]] * vin.Weights[3];
	//float posy = vin.PosL.y;
	//vin.PosL.y = vin.PosL.z;
	//vin.PosL.z = - vin.PosL.z;
	float4 pos = mul(BoneTransform , float4(vin.PosL,1.0));
	//float3 posL = pos.xyz;
	/*for (int i = 0; i < 4; ++i)
	{
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.
		//unsigned int index = unsigned int();
		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
	}*/
	//if (posL.x > 0.5)
	{
		vout.PosH = mul(pos, gWorldViewProj);
	}
	/*else
	{
		// Transform to homogeneous clip space.
		vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	}*/
	
	
	// Just pass vertex color into the pixel shader.
    //vout.Color = vin.Color;
	vout.Tex = vin.Tex;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 Color;
Color = gDiffuseMap.Sample(samLinear, pin.Tex);
    return Color;
}

technique11 ColorTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS() ) );
    }
}
