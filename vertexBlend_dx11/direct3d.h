#pragma once // 重複インクルードを自動防止する

#include <d3d11.h>  // DirectX11を使うためのヘッダーファイル

// 関数のプロトタイプ宣言
HRESULT D3D_Create(HWND hwnd);
void    D3D_Release();
void    D3D_Render();
