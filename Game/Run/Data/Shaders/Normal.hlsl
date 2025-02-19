//------------------------------------------------------------------------------------------------
Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D glossMap : register(t2);
SamplerState textureSampler : register(s0);


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
	float4 clipPosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 tangent : TANGENT;
	float4 bitangent : BITANGENT;
	float4 normal : NORMAL;
    float4 worldPosition : POSITION;
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
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.tangent = float4(0, 0, 0, 0);
	v2p.bitangent = float4(0, 0, 0, 0);
	v2p.normal = worldNormal;
    v2p.worldPosition = worldPosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float4 modelColor = ModelColor;
    float4 vertexColor = input.color;
    

    float4 textureColor = UseDiffuseMap ? diffuseMap.Sample(textureSampler, input.uv) : float4(1, 1, 1, 1);
    float ambient = AmbientIntensity * RenderAmbient;
    float3 worldNormal = normalize(input.normal.xyz);
    
    if (UseNormalMap)
    {
        // tbn matrix????
        float3 normalFromMap = normalMap.Sample(textureSampler, input.uv).xyz * 2.0 - 1.0;
        worldNormal = normalize(normalFromMap);
    }

    float vnDotL = dot(input.normal.xyz, -SunDirection);
    float pnDotL = dot(worldNormal, -SunDirection);

    float falloff = smoothstep(minFallOff, maxFallOff, vnDotL);
    float falloffMultiplier = lerp(minFallOffMultiplier, maxFallOffMultiplier, falloff);
    float lightingFactor = pnDotL * falloffMultiplier;

    float diffuse = SunIntensity * saturate(lightingFactor) * RenderDiffuse;

    float specular = 0.0;
    if (UseSpecularMap)
    {
        float4 specularSample = glossMap.Sample(textureSampler, input.uv);
        float specularIntensity = specularSample.r;
        float specularPower = lerp(1, 32, specularSample.g);
        
        float3 viewDir = normalize(WorldEyePosition - input.worldPosition.xyz);
        float3 halfVector = normalize(-SunDirection + viewDir);
        float specularFactor = pow(saturate(dot(worldNormal, halfVector)), specularPower); 
        specular = specularIntensity * specularFactor * RenderSpecular;
    }

    float4 color = float4((ambient + diffuse) * textureColor.rgb * vertexColor.rgb * modelColor.rgb, 1.0);
    color.rgb += specular;

   /* if (UseEmissiveMap && RenderEmissive)
    {
        color.rgb += diffuseMap.Sample(textureSampler, input.uv).a * textureColor.rgb;
    }*/

    color = saturate(color);
    clip(color.a - 0.01f);
    return color;
}
