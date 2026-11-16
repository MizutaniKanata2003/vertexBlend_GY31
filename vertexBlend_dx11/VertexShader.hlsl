// 頂点シェーダー

// 頂点のデータを表す構造体（受け取り用）
struct VS_IN
{
  float4 pos : POS;
  float blend : BLD;
  float4 col : COL;
};

// 頂点のデータを表す構造体（送信用） 
struct VS_OUT
{
  float4 pos : SV_POSITION;
  float blend : BLEND;
  float4 col : COLOR;
};
 
// グローバル変数の宣言
// 定数バッファ受け取り用
cbuffer ConstBuffer : register( b0 )
{
    // 投影行列
  matrix matrixProj;
    // ワールド変換行列
  matrix matrixWorld1;
  matrix matrixWorld2;
}


// 頂点シェーダーのエントリポイント 
VS_OUT vs_main( VS_IN input )
{
  VS_OUT output;

  // ワールド変換行列を頂点に掛ける
  float4 pos1 = mul( input.pos, matrixWorld1 );
  float4 pos2 = mul( input.pos, matrixWorld2 );
  output.pos = lerp( pos1, pos2, input.blend ); //頂点ブレンドする
    
  // 平行投影のため行列を頂点に掛ける
  output.pos = mul( output.pos, matrixProj );

  // ここで座標、ブレンディング情報、色を受け取る
  output.blend = input.blend;
  output.col = input.col;

  return output;
}