using System;
using System.Runtime.InteropServices;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX.Direct3D11;

internal sealed class CapturedGpuFrame : IDisposable
{
    private readonly nint texture;
    private readonly Direct3D11CaptureFrame frame;
    private readonly Direct3D11CaptureFramePool pool;
    private readonly GraphicsCaptureSession session;

    public nint Texture => texture;
    public Windows.Graphics.SizeInt32 ContentSize => frame.ContentSize;
    public IDirect3DSurface Surface => frame.Surface;

    public CapturedGpuFrame(nint texture, Direct3D11CaptureFrame frame, Direct3D11CaptureFramePool pool, GraphicsCaptureSession session)
    {
        this.texture = texture;
        this.frame = frame;
        this.pool = pool;
        this.session = session;
    }

    public void Dispose()
    {
        Marshal.Release(texture);
        frame.Dispose();
        pool.Dispose();
        session.Dispose();
    }
}
