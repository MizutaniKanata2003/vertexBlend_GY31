#include <d3d11.h>  // DirectX11を使うためのヘッダーファイル
#include <atltypes.h> // CRectを使うためのヘッダーファイル
#include <DirectXMath.h>

// コンパイル済みシェーダーをインクルード
#include "VertexShader.h"
#include "PixelShader.h"

// ライブラリのリンク設定
#pragma comment (lib, "d3d11.lib")

// Direct3D解放の簡略化マクロ
#define SAFE_RELEASE(p)      { if( NULL != p ) { p->Release(); p = NULL; } }

typedef struct
{
	int			status;		// ステータス
	DirectX::XMFLOAT3 position;	// 座標
	DirectX::XMFLOAT3 rotation;	// 回転角
	float	degree;		// 角度変化サインカーブ用角度
} PRISM;
PRISM g_Prism;			// プリズムオブジェクト

// 定数バッファ用構造体
struct ConstBuffer
{
	// 投影行列
	DirectX::XMMATRIX matrixProj;
	// ワールド変換行列
	DirectX::XMMATRIX matrixWorld1;
	DirectX::XMMATRIX matrixWorld2;
};

//----- 頂点フォーマット定義
struct VERTEX
{
	float x,y,z;			// 頂点座標（座標変換あり）
	float blend;				// ブレンド値
	DirectX::XMFLOAT4 color;				// ディフューズ色
};
#define FVF_VERTEX (D3DFVF_XYZB1 | D3DFVF_DIFFUSE)


// グローバル変数の宣言
// ※ID3D11で始まるポインタ型の変数は、解放する必要がある
ID3D11Device* m_pDevice; // デバイス＝DirectXの各種機能を作る
// コンテキスト＝描画関連を司る機能
ID3D11DeviceContext* m_pImmediateContext;
// スワップチェイン＝ダブルバッファ機能
IDXGISwapChain* m_pSwapChain;
// レンダーターゲット＝描画先を表す機能
ID3D11RenderTargetView* m_pRenderTargetView;
// 深度バッファ用テクスチャ
ID3D11Texture2D* m_pDepthStencilTexture;
// 深度バッファ
ID3D11DepthStencilView* m_pDepthStencilView;
// 頂点バッファ
ID3D11Buffer* m_pVertexBuffer0;
ID3D11Buffer* m_pVertexBuffer1;
ID3D11Buffer* m_pVertexBuffer2;
// インプットレイアウト
ID3D11InputLayout* m_pInputLayout;
// 頂点シェーダーオブジェクト
ID3D11VertexShader* m_pVertexShader;
// ピクセルシェーダーオブジェクト
ID3D11PixelShader* m_pPixelShader;
// ビューポート
D3D11_VIEWPORT m_Viewport;

// 定数バッファ用変数
ID3D11Buffer* m_pConstantBuffer;

// 関数のプロトタイプ宣言
HRESULT D3D_Create(HWND hwnd);
void    D3D_Release();
void    D3D_Render();

// Direct3Dの初期化をする
HRESULT D3D_Create(HWND hwnd)
{
	// HRESULT型・・・Windowsプログラムで関数実行の成功/失敗を受け取る
	HRESULT  hr;

	UINT flags = 0;
	//#ifdef _DEBUG
	//    flags |= D3D11_CREATE_DEVICE_DEBUG;
	//#endif
	D3D_FEATURE_LEVEL pLevels[] = {D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL level;

	CRect                rect;
	DXGI_SWAP_CHAIN_DESC scDesc;

	::GetClientRect(hwnd,&rect);
	::ZeroMemory(&scDesc,sizeof(scDesc));
	scDesc.BufferCount = 1;
	scDesc.BufferDesc.Width = rect.Width();
	scDesc.BufferDesc.Height = rect.Height();
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.OutputWindow = hwnd;
	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	scDesc.Windowed = TRUE;

	// デバイスとスワップチェインを同時に作成する関数の呼び出し
	hr = D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,flags,pLevels,1,
																		 D3D11_SDK_VERSION,&scDesc,&m_pSwapChain,&m_pDevice,&level,&m_pImmediateContext);
	// 上の関数呼び出しが失敗してないかifでチェック
	if(FAILED(hr)) return hr;
	ID3D11Texture2D* pBackBuffer;

	hr = m_pSwapChain->GetBuffer(0,__uuidof(ID3D11Texture2D),(LPVOID*)&pBackBuffer);
	if(FAILED(hr)) return hr;

	// レンダーターゲットを作成する関数を呼び出す
	hr = m_pDevice->CreateRenderTargetView(pBackBuffer,NULL,&m_pRenderTargetView);
	pBackBuffer->Release();
	if(FAILED(hr)) return hr;

	// 深度ステンシルバッファを作成
	// ※深度バッファ（Zバッファ）→奥行を判定して前後関係を正しく描画できる
	D3D11_TEXTURE2D_DESC txDesc;
	ZeroMemory(&txDesc,sizeof(txDesc));
	txDesc.Width = rect.Width();
	txDesc.Height = rect.Height();
	txDesc.MipLevels = 1;
	txDesc.ArraySize = 1;
	txDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	txDesc.SampleDesc.Count = 1;
	txDesc.SampleDesc.Quality = 0;
	txDesc.Usage = D3D11_USAGE_DEFAULT;
	txDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	txDesc.CPUAccessFlags = 0;
	txDesc.MiscFlags = 0;
	hr = m_pDevice->CreateTexture2D(&txDesc,NULL,&m_pDepthStencilTexture);
	if(FAILED(hr)) return hr;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsDesc;
	ZeroMemory(&dsDesc,sizeof(dsDesc));
	dsDesc.Format = txDesc.Format;
	dsDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsDesc.Texture2D.MipSlice = 0;
	hr = m_pDevice->CreateDepthStencilView(m_pDepthStencilTexture,&dsDesc,&m_pDepthStencilView);
	if(FAILED(hr)) return hr;

	//----- プリズム面モデル定義
	VERTEX prism_vertex0[] = {
		//   x      y      z    blend  color
		{-0.8f,-0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.8f,0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.6f,-0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.6f,0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.4f,-0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.4f,0.2f,0.0f,0.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.2f,-0.2f,0.0f,0.25f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{-0.2f,0.2f,0.0f,0.25f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.0f,-0.2f,0.0f,0.50f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.0f,0.2f,0.0f,0.50f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.2f,-0.2f,0.0f,0.75f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.2f,0.2f,0.0f,0.75f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.4f,-0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.4f,0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.6f,-0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.6f,0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.8f,-0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
			{0.8f,0.2f,0.0f,1.00f,{1.0f,1.0f,1.0f,1.0f}},	// 頂点０
	};

	VERTEX prism_vertex1[] = {
		//   x      y      z    blend  color
		{-0.8f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.8f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.6f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.6f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.4f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.4f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.2f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{-0.2f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.0f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.0f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.2f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.2f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.4f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.4f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.6f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.6f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.8f,-0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
			{0.8f,0.2f,0.0f,0.00f,{1.0f,0.0f,0.0f,1.0f}},	// 頂点０
	};

	VERTEX prism_vertex2[] = {
		//   x      y      z    blend  color
		{-0.8f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.8f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.6f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.6f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.4f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.4f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.2f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{-0.2f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.0f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.0f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.2f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.2f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.4f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.4f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.6f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.6f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.8f,-0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
			{0.8f,0.2f,0.0f,1.00f,{0.0f,1.0f,0.0f,1.0f}},	// 頂点０
	};

	{
		// 頂点バッファを作成する
		// ※頂点バッファ→VRAMに頂点データを置くための機能
		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.ByteWidth = sizeof(prism_vertex0);// 確保するバッファサイズを指定
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;// 頂点バッファ作成を指定
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA subResourceData;
		subResourceData.pSysMem = prism_vertex0;// VRAMに送るデータを指定
		subResourceData.SysMemPitch = 0;
		subResourceData.SysMemSlicePitch = 0;

		hr = m_pDevice->CreateBuffer(&bufferDesc,&subResourceData,&m_pVertexBuffer0);
		if(FAILED(hr))return hr;
	}

	{
		// 頂点バッファを作成する
// ※頂点バッファ→VRAMに頂点データを置くための機能
		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.ByteWidth = sizeof(prism_vertex1);// 確保するバッファサイズを指定
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;// 頂点バッファ作成を指定
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA subResourceData;
		subResourceData.pSysMem = prism_vertex1;// VRAMに送るデータを指定
		subResourceData.SysMemPitch = 0;
		subResourceData.SysMemSlicePitch = 0;

		hr = m_pDevice->CreateBuffer(&bufferDesc,&subResourceData,&m_pVertexBuffer1);
		if(FAILED(hr))return hr;
	}

	{
		// 頂点バッファを作成する
// ※頂点バッファ→VRAMに頂点データを置くための機能
		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.ByteWidth = sizeof(prism_vertex2);// 確保するバッファサイズを指定
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;// 頂点バッファ作成を指定
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA subResourceData;
		subResourceData.pSysMem = prism_vertex2;// VRAMに送るデータを指定
		subResourceData.SysMemPitch = 0;
		subResourceData.SysMemSlicePitch = 0;

		hr = m_pDevice->CreateBuffer(&bufferDesc,&subResourceData,&m_pVertexBuffer2);
		if(FAILED(hr))return hr;
	}
	// インプットレイアウト作成
	D3D11_INPUT_ELEMENT_DESC g_VertexDesc[]{
		{"POS",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
	{"BLD",0,DXGI_FORMAT_R32_FLOAT,0,offsetof(VERTEX,blend),D3D11_INPUT_PER_VERTEX_DATA,0},
	{"COL",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
	};

	hr = m_pDevice->CreateInputLayout(g_VertexDesc,ARRAYSIZE(g_VertexDesc),g_vs_main,sizeof(g_vs_main),&m_pInputLayout);
	if(FAILED(hr))return hr;

	// シェーダーのオブジェクトを作成
	// コンパイル済みシェーダーをVRAMに配置してGPUが実行できるようにする
	hr = m_pDevice->CreateVertexShader(&g_vs_main,sizeof(g_vs_main),NULL,&m_pVertexShader);
	if(FAILED(hr))return hr;

	hr = m_pDevice->CreatePixelShader(&g_ps_main,sizeof(g_ps_main),NULL,&m_pPixelShader);
	if(FAILED(hr))return hr;

	// ビューポートを作成
	// →画面分割などに使う、描画領域の指定のこと
	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;
	m_Viewport.Width = (FLOAT)rect.Width();
	m_Viewport.Height = (FLOAT)rect.Height();
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	// 定数バッファ作成
	D3D11_BUFFER_DESC cbDesc;
	cbDesc.ByteWidth = sizeof(ConstBuffer);// 今から作る定数バッファのサイズ
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;// 定数バッファ作成を指定
	cbDesc.CPUAccessFlags = 0;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	hr = m_pDevice->CreateBuffer(&cbDesc,NULL,&m_pConstantBuffer);

	return hr;
}

// Direct3Dの解放（終了処理）をする
// ※DirectXの各機能は作成した後、アプリ終了時に必ず解放しなければならない
void D3D_Release()
{
	if(m_pImmediateContext) m_pImmediateContext->ClearState();

	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexBuffer0);
	SAFE_RELEASE(m_pVertexBuffer1);
	SAFE_RELEASE(m_pVertexBuffer2);
	SAFE_RELEASE(m_pDepthStencilTexture);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pDevice);
	SAFE_RELEASE(m_pConstantBuffer);
}

// Direct3Dの描画をする
void D3D_Render()
{
	// 画面塗りつぶし色
	float clearColor[4] = {0.0f,0.0f,0.0f,1.0f}; //red,green,blue,alpha

	// 描画先のキャンバスと使用する深度バッファを指定する
	m_pImmediateContext->OMSetRenderTargets(1,&m_pRenderTargetView,m_pDepthStencilView);
	// 描画先キャンバスを塗りつぶす
	m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView,clearColor);
	// 深度バッファをリセットする
	m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView,D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,1.0f,0);

	UINT strides = sizeof(VERTEX);
	UINT offsets = 0;
	m_pImmediateContext->IASetInputLayout(m_pInputLayout);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->VSSetShader(m_pVertexShader,NULL,0);
	m_pImmediateContext->RSSetViewports(1,&m_Viewport);
	m_pImmediateContext->PSSetShader(m_pPixelShader,NULL,0);

	// 定数バッファを頂点シェーダーにセットする
	m_pImmediateContext->VSSetConstantBuffers(0,1,&m_pConstantBuffer);

	DirectX::XMMATRIX	matRotateX;				// Ｘ軸回転マトリックス
	DirectX::XMMATRIX	matRotateY;				// Ｙ軸回転マトリックス
	DirectX::XMMATRIX	matRotateZ;				// Ｚ軸回転マトリックス
	DirectX::XMMATRIX	matTrans;				// 平行移動マトリックス
	DirectX::XMMATRIX	matWorld1;				// ワールド変換マトリックス
	DirectX::XMMATRIX	matWorld2;				// ワールド変換マトリックス

	switch(g_Prism.status){
	case 0:
	// モデル角度、位置の初期化
	g_Prism.rotation.x = g_Prism.rotation.y = g_Prism.rotation.z = 0.0f;
	g_Prism.position.x = g_Prism.position.y = g_Prism.position.z = 0.0f;
	g_Prism.degree = 0.0f;
	g_Prism.status = 1;
	// THRU
	case 1:
	// 自動的に回転
	g_Prism.rotation.z = (float)sin(g_Prism.degree);
	g_Prism.degree += 0.01f;
	break;
	default:
	break;
	}

	// ワールドマトリックスを合成
	DirectX::XMMATRIX rotateX = DirectX::XMMatrixRotationX(g_Prism.rotation.x);
	DirectX::XMMATRIX rotateY = DirectX::XMMatrixRotationY(g_Prism.rotation.y);
	DirectX::XMMATRIX rotateZ = DirectX::XMMatrixRotationZ(g_Prism.rotation.z);
	DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(g_Prism.position.x,g_Prism.position.y,g_Prism.position.z);
	matWorld1 = rotateX * rotateY * rotateZ * translation;
	matWorld2 = translation;

	// ワールド変換行列の作成
	ConstBuffer cb;
	float scale = 1.0f;
	cb.matrixProj = DirectX::XMMatrixOrthographicLH(4.0f * scale,3.0f * scale,0.0f,3.0f);
	cb.matrixProj = XMMatrixTranspose(cb.matrixProj);
	cb.matrixWorld1 = XMMatrixTranspose(matWorld1);
	cb.matrixWorld2 = XMMatrixTranspose(matWorld2);

	// 行列をシェーダーに渡す
	m_pImmediateContext->UpdateSubresource(m_pConstantBuffer,0,NULL,&cb,0,0);

	// マトリックス１で描画
	m_pImmediateContext->IASetVertexBuffers(0,1,&m_pVertexBuffer0,&strides,&offsets);
	m_pImmediateContext->Draw(18,0);

	// ブレンディングモデルを描画
	m_pImmediateContext->IASetVertexBuffers(0,1,&m_pVertexBuffer1,&strides,&offsets);
	m_pImmediateContext->Draw(18,0);

	// マトリックス０で描画
	m_pImmediateContext->IASetVertexBuffers(0,1,&m_pVertexBuffer2,&strides,&offsets);
	m_pImmediateContext->Draw(18,0);

	// ダブルバッファの切り替えを行い画面を更新する
	m_pSwapChain->Present(1,0);
}