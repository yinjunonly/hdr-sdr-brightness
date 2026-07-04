using System;
using System.Runtime.InteropServices;
using Windows.Graphics.DirectX.Direct3D11;
using WinRT;

internal static class WinRtCaptureInterop
{
    private static readonly Guid IidId3d11Texture2D = new("6f15aaf2-d208-4e89-9ab4-489535d34f9c");

    public static nint GetTextureFromSurface(IDirect3DSurface surface)
    {
        IDirect3DDxgiInterfaceAccess access = surface.As<IDirect3DDxgiInterfaceAccess>();
        access.GetInterface(IidId3d11Texture2D, out nint texture);
        if (texture == 0)
        {
            throw new InvalidOperationException("IDirect3DSurface did not expose ID3D11Texture2D.");
        }

        return texture;
    }

    [ComImport]
    [Guid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDirect3DDxgiInterfaceAccess
    {
        void GetInterface(in Guid iid, out nint p);
    }
}
