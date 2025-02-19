//------------------------------------------------------------------------------------------------
Texture2D diffuseMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D specularMap : register(t2);
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

struct ps_output_t
{
	float4 colorRenderTarget : SV_Target0;
	float4 emissiveRenderTarget : SV_Target1;
};
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition, 1);
	float4 worldPosition = mul(ModelMatrix, localPosition);
	float4 viewPosition = mul(ViewMatrix, worldPosition);
	float4 clipPosition = mul(ProjectionMatrix, viewPosition);
	float4 localTangent = float4(input.localTangent, 0);
	float4 worldTangent = mul(ModelMatrix, localTangent);
	float4 localBitangent = float4(input.localBitangent, 0);
	float4 worldBitangent = mul(ModelMatrix, localBitangent);
	float4 localNormal = float4(input.localNormal, 0);
	float4 worldNormal = mul(ModelMatrix, localNormal);
    worldNormal.xyz = normalize(worldNormal.xyz);
	
	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.tangent = worldTangent;
	v2p.bitangent = worldBitangent;
	v2p.normal = worldNormal;
    v2p.worldPosition = worldPosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
ps_output_t PixelMain(v2p_t input) : SV_Target0
{
    float4 modelColor = ModelColor;
    float4 vertexColor = input.color;
    
    float4 textureColor = UseDiffuseMap ? diffuseMap.Sample(textureSampler, input.uv) : float4(1, 1, 1, 1);
    float ambient = AmbientIntensity;

    float3 vertexWorldNormal = normalize(input.normal.xyz);
	float3x3 tbnMatrix = float3x3(input.tangent.xyz, input.bitangent.xyz,input.normal.xyz);
    float3 tangentNormal = (normalMap.Sample(textureSampler, input.uv).xyz * 2.0 - 1.0);
    float3 pixelWorldNormal = UseNormalMap ? mul(tangentNormal, tbnMatrix) : vertexWorldNormal;

    float vnDotL = dot(vertexWorldNormal, -SunDirection);
    float nDotL = dot(pixelWorldNormal, -SunDirection);

    float falloff = clamp(vnDotL, minFallOff, maxFallOff);
	float falloffT = (falloff - minFallOff) / (maxFallOff - minFallOff);
    float falloffMultiplier = lerp(minFallOffMultiplier, maxFallOffMultiplier, falloffT);

    float diffuse = SunIntensity * saturate(nDotL) * falloffMultiplier;

	float3 worldViewDirection = normalize(WorldEyePosition - input.worldPosition.xyz);
	float3 worldHalfVector = normalize(-SunDirection + worldViewDirection);
	float nDotH = saturate(dot(pixelWorldNormal, worldHalfVector));
	
	float3 specGlossEmit = specularMap.Sample(textureSampler, input.uv);
	float specularIntensity = specGlossEmit.r * UseSpecularMap;
	float specularPower = 1.0 + 31.0 * specGlossEmit.g * UseGlossinessMap;

	float emissive = specGlossEmit.b * UseEmissiveMap;
	
	float specular = pow(nDotH, specularPower) * falloffMultiplier * specularIntensity;

	ambient *= RenderAmbient;
	diffuse *= RenderDiffuse;
	specular *= RenderSpecular;
	emissive *= RenderEmissive;
	
	float4 direct = float4((ambient + diffuse + specular + emissive).xxx, 1.0);
	
	ps_output_t output;
	output.colorRenderTarget =  direct * textureColor * vertexColor * modelColor;
	output.emissiveRenderTarget =  float4((emissive).xxx, 1.0)  * textureColor * vertexColor * modelColor;

	clip(output.colorRenderTarget.a - 0.01);
	return output;
}
