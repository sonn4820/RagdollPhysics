//------------------------------------------------------------------------------------------------
Texture2D diffuseMap : register(t0);
Texture2D specularMap : register(t2);

SamplerState textureSampler : register(s0);

//
float RangeMapClamped(float value, float inMin, float inMax, float outMin, float outMax)
{
    float normalizedValue = (value - inMin) / (inMax - inMin);
    float mappedValue = lerp(outMin, outMax, normalizedValue);
    return clamp(mappedValue, outMin, outMax);
}

//------------------------------------------------------------------------------------------------
cbuffer DirectionalLight : register(b1)
{
	float3 SunDirection;
	float SunIntensity;
    float AmbientIntensity;
    float3 WorldEyePosition;
    float minFallOff; 
    float maxFallOff; 
    float minFallOffMultiplier; 
    float maxFallOffMultiplier;
	int RenderAmbient;
	int RenderDiffuse;
	int RenderSpecular;
	int RenderEmissive;
	int UseDiffuseMap;
	int UseNormalMap;
	int UseSpecularMap;
	int UseGlossinessMap;
	int UseEmissiveMap;
	float3 pad2;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 ProjectionMatrix;
	float4x4 ViewMatrix;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelMatrix;
	float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 localTangent : TANGENT;
	float3 localBitangent : BITANGENT;
	float3 localNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------

struct v2p_t
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 tangent : TANGENT;
	float4 bitangent : BITANGENT;
	float4 normal : NORMAL;
    float4 fragPosition : POSITION;
};

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition, 1);
	float4 worldPosition = mul(ModelMatrix, localPosition);
	float4 viewPosition = mul(ViewMatrix, worldPosition);
	float4 clipPosition = mul(ProjectionMatrix, viewPosition);
	float4 localNormal = float4(input.localNormal, 0);
	float4 worldNormal = mul(ModelMatrix, localNormal);
    worldNormal.xyz = normalize(worldNormal.xyz);
	
	v2p_t v2p;
	v2p.position = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.tangent = float4(0, 0, 0, 0);
	v2p.bitangent = float4(0, 0, 0, 0);
	v2p.normal = worldNormal;
    v2p.fragPosition = worldPosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
	float4 color;

    float ambient = AmbientIntensity * RenderAmbient;
    float directional = SunIntensity * saturate(dot(normalize(input.normal.xyz), -SunDirection)) * RenderDiffuse;
    float4 diffuseColor = UseDiffuseMap ? diffuseMap.Sample(textureSampler, input.uv) : float4(1, 1, 1, 1);
    
	float4 specularColor = specularMap.Sample(textureSampler, input.uv) * UseSpecularMap;
    float specularIntensity = specularColor.r;
    float specularPower =  lerp(1, 32, specularColor.g);
   
	float3 viewVector = normalize(WorldEyePosition - input.fragPosition.xyz);
    float3 halfwayVector = normalize(-SunDirection + viewVector);
    
	float specularValue = specularIntensity * pow(saturate(dot(input.normal.xyz, halfwayVector)), specularPower) * RenderSpecular;
    
	float4 lightColor = float4((ambient + directional + specularValue).xxx, 1.0);
    float4 vertexColor = input.color;
    float4 modelColor = ModelColor;
    color = lightColor * diffuseColor * vertexColor * modelColor;
    clip(color.a - 0.01f);

    return color;
}
