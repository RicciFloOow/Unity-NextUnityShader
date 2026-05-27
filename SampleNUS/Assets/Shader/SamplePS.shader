Shader "Unlit/SamplePS"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "white" {}
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" }
        LOD 100

        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #pragma only_renderers d3d11

            #pragma use_dxc

            #include "UnityCG.cginc"
            #include "NUSLib.hlsl"

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f
            {
                float2 uv : TEXCOORD0;
                float4 vertex : SV_POSITION;
            };

            sampler2D _MainTex;
            float4 _MainTex_ST;

            v2f vert (appdata v)
            {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                return o;
            }

            float4 frag (v2f i, uint primitiveID : SV_PrimitiveID) : SV_Target
            {
                float4 col = tex2D(_MainTex, i.uv);
                uint4 rgba8 = (uint4)(col * 255.0 + 0.5);
                uint packedCol = (rgba8.x) | (rgba8.y << 8) | (rgba8.z << 16) | (rgba8.w << 24);
                uint acc = 0;
                uint prod = UNITY_SM6_SELECT(
                    0, 
                    dot4add_u8packed(primitiveID, packedCol, acc)
                );
                return float4((prod & 0xFF) / 255.0, ((prod >> 8) & 0xFF) / 255.0, ((prod >> 16) & 0xFF) / 255.0, ((prod >> 24) & 0xFF) / 255.0);
            }
            ENDHLSL
        }
    }
}
