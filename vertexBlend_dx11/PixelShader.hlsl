// ピクセルシェーダー

// ピクセルの情報の構造体（受け取り用）
struct PS_IN
{
  float4 pos : SV_POSITION;
  float blend : BLEND;
  float4 col : COLOR;
};
 
// ピクセルシェーダーのエントリポイント
float4 ps_main( PS_IN input ) : SV_Target
{
  return input.col;
}