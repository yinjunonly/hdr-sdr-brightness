using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.DirectX.Direct3D11;
using WinRT;

internal static class WgcCapture
{
    private static readonly object SharedRuntimeLock = new();
    private static Task<CaptureRuntime>? sharedRuntimeTask;

    private static readonly Guid IidGraphicsCaptureItem = new("79c3f95b-31f7-4ec2-a464-632ef5d30760");
    private static readonly Guid IidGraphicsCaptureItemInterop = new("3628e81b-3cac-4c60-b7f4-23ce0e0c3356");

    public static Task<CaptureRuntime> GetSharedRuntimeAsync()
    {
        lock (SharedRuntimeLock)
        {
            if (sharedRuntimeTask is null ||
                sharedRuntimeTask.IsFaulted ||
                sharedRuntimeTask.IsCanceled)
            {
                sharedRuntimeTask = CaptureRuntime.CreateAsync();
            }

            return sharedRuntimeTask;
        }
    }

    public static void ResetSharedRuntime()
    {
        Task<CaptureRuntime>? oldTask;
        lock (SharedRuntimeLock)
        {
            oldTask = sharedRuntimeTask;
            sharedRuntimeTask = null;
        }

        if (oldTask?.Status == TaskStatus.RanToCompletion)
        {
            oldTask.Result.Dispose();
        }
    }

    public static async Task<RegionCaptureSource> CapturePrimaryMonitorGpuFrameAsync(DirectXPixelFormat requestedFormat)
    {
        CaptureRuntime runtime = await GetSharedRuntimeAsync();
        CapturedGpuFrame gpuFrame = await CaptureGpuFrameAsync(runtime.Item, runtime.Device, requestedFormat);
        return new RegionCaptureSource(runtime, gpuFrame);
    }

    public static async Task<ReadbackResult> CapturePrimaryMonitorFrameAsync(
        DirectXPixelFormat requestedFormat,
        ToneMapOptions toneMap,
        bool diagnostic)
    {
        using NativeD3D native = NativeD3D.Create();
        IDirect3DDevice? winrtDevice = null;
        GraphicsCaptureItem? item = null;
        try
        {
            winrtDevice = native.CreateWinRtDevice();
            Console.WriteLine("Using primary monitor capture item.");
            await RequestProgrammaticCaptureAccessAsync();
            item = CreatePrimaryMonitorItem();
            Console.WriteLine($"Selected: {item.DisplayName}");
            Console.WriteLine($"Item size: {item.Size.Width} x {item.Size.Height}");
            return await CaptureFrameAsync(item, winrtDevice, native, requestedFormat, toneMap, diagnostic);
        }
        finally
        {
            DisposeIfPossible(winrtDevice);
            DisposeIfPossible(item);
        }
    }

    public static async Task<ReadbackResult> CaptureFrameAsync(
        GraphicsCaptureItem item,
        IDirect3DDevice device,
        NativeD3D native,
        DirectXPixelFormat requestedFormat,
        ToneMapOptions toneMap,
        bool diagnostic,
        System.Drawing.Rectangle? readbackRegion = null)
    {
        using CapturedGpuFrame gpuFrame = await CaptureGpuFrameAsync(item, device, requestedFormat);
        ReadbackResult result = native.ReadbackTexture(gpuFrame.Texture, toneMap, diagnostic, readbackRegion);
        if (diagnostic)
        {
            Direct3DSurfaceDescription surfaceDesc = gpuFrame.Surface.Description;
            Console.WriteLine($"Frame content size: {gpuFrame.ContentSize.Width} x {gpuFrame.ContentSize.Height}");
            Console.WriteLine($"Surface desc: {surfaceDesc.Width} x {surfaceDesc.Height}, format {surfaceDesc.Format}");
            result.PrintStats();
        }
        return result;
    }

    public static GraphicsCaptureItem CreatePrimaryMonitorItem()
    {
        nint monitor = MonitorFromWindow(GetDesktopWindow(), 1);
        if (monitor == 0)
        {
            throw new InvalidOperationException("Could not resolve primary monitor handle.");
        }

        nint className = 0;
        nint factoryPtr = 0;
        nint itemPtr = 0;
        try
        {
            int hr = WindowsCreateString("Windows.Graphics.Capture.GraphicsCaptureItem", 44, out className);
            ThrowIfFailed(hr, "WindowsCreateString(GraphicsCaptureItem)");
            Guid factoryIid = IidGraphicsCaptureItemInterop;
            hr = RoGetActivationFactory(className, ref factoryIid, out factoryPtr);
            ThrowIfFailed(hr, "RoGetActivationFactory(GraphicsCaptureItem)");

            IGraphicsCaptureItemInterop interop = (IGraphicsCaptureItemInterop)Marshal.GetObjectForIUnknown(factoryPtr);
            interop.CreateForMonitor(monitor, IidGraphicsCaptureItem, out itemPtr);
            return MarshalInterface<GraphicsCaptureItem>.FromAbi(itemPtr);
        }
        finally
        {
            if (itemPtr != 0) Marshal.Release(itemPtr);
            if (factoryPtr != 0) Marshal.Release(factoryPtr);
            if (className != 0) WindowsDeleteString(className);
        }
    }

    public static Task RequestProgrammaticCaptureAccessAsync()
    {
        Console.WriteLine("Programmatic capture access: skipped (using GraphicsCaptureItem interop).");
        return Task.CompletedTask;
    }

    private static async Task<CapturedGpuFrame> CaptureGpuFrameAsync(
        GraphicsCaptureItem item,
        IDirect3DDevice device,
        DirectXPixelFormat requestedFormat)
    {
        Direct3D11CaptureFramePool? pool = null;
        GraphicsCaptureSession? session = null;
        Direct3D11CaptureFrame? frame = null;
        try
        {
            pool = Direct3D11CaptureFramePool.CreateFreeThreaded(device, requestedFormat, 1, item.Size);
            session = pool.CreateCaptureSession(item);
            try
            {
                session.IsCursorCaptureEnabled = false;
                TryDisableCaptureBorder(session);
            }
            catch
            {
            }

            TaskCompletionSource<Direct3D11CaptureFrame> frameReady = new(TaskCreationOptions.RunContinuationsAsynchronously);
            TypedEventHandler<Direct3D11CaptureFramePool, object> handler = (sender, _) =>
            {
                Direct3D11CaptureFrame? f = sender.TryGetNextFrame();
                if (f is not null && !frameReady.TrySetResult(f))
                {
                    f.Dispose();
                }
            };
            pool.FrameArrived += handler;
            session.StartCapture();

            try
            {
                Task completed = await Task.WhenAny(frameReady.Task, Task.Delay(TimeSpan.FromSeconds(10)));
                if (completed != frameReady.Task)
                {
                    throw new TimeoutException();
                }
                frame = await frameReady.Task;
                pool.FrameArrived -= handler;
            }
            catch
            {
                pool.FrameArrived -= handler;
                throw;
            }

            nint texture = WinRtCaptureInterop.GetTextureFromSurface(frame.Surface);
            CapturedGpuFrame result = new(texture, frame, pool, session);
            frame = null;
            pool = null;
            session = null;
            return result;
        }
        finally
        {
            frame?.Dispose();
            pool?.Dispose();
            session?.Dispose();
        }
    }

    private static void TryDisableCaptureBorder(GraphicsCaptureSession session)
    {
        System.Reflection.PropertyInfo? property = session.GetType().GetProperty("IsBorderRequired");
        if (property?.CanWrite == true)
        {
            property.SetValue(session, false);
        }
    }

    private static void DisposeIfPossible(object? value)
    {
        if (value is IDisposable disposable)
        {
            disposable.Dispose();
        }
    }

    private static void ThrowIfFailed(int hr, string operation)
    {
        if (hr < 0)
        {
            Marshal.ThrowExceptionForHR(hr);
            throw new InvalidOperationException(operation);
        }
    }

    [DllImport("combase.dll")]
    private static extern int WindowsCreateString(
        [MarshalAs(UnmanagedType.LPWStr)] string sourceString,
        int length,
        out nint hstring);

    [DllImport("combase.dll")]
    private static extern int WindowsDeleteString(nint hstring);

    [DllImport("combase.dll")]
    private static extern int RoGetActivationFactory(nint activatableClassId, ref Guid iid, out nint factory);

    [DllImport("user32.dll")]
    private static extern nint GetDesktopWindow();

    [DllImport("user32.dll")]
    private static extern nint MonitorFromWindow(nint hwnd, uint flags);

    [ComImport]
    [Guid("3628E81B-3CAC-4C60-B7F4-23CE0E0C3356")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IGraphicsCaptureItemInterop
    {
        void CreateForWindow(nint window, in Guid iid, out nint result);
        void CreateForMonitor(nint monitor, in Guid iid, out nint result);
    }
}

internal sealed class CaptureRuntime : IDisposable
{
    private CaptureRuntime(NativeD3D native, IDirect3DDevice device, GraphicsCaptureItem item)
    {
        Native = native;
        Device = device;
        Item = item;
    }

    public NativeD3D Native { get; }
    public IDirect3DDevice Device { get; }
    public GraphicsCaptureItem Item { get; }

    public static async Task<CaptureRuntime> CreateAsync()
    {
        NativeD3D? native = null;
        IDirect3DDevice? device = null;
        GraphicsCaptureItem? item = null;
        try
        {
            native = NativeD3D.Create();
            device = native.CreateWinRtDevice();
            await WgcCapture.RequestProgrammaticCaptureAccessAsync();

            item = WgcCapture.CreatePrimaryMonitorItem();
            CaptureRuntime runtime = new(native, device, item);
            native = null;
            device = null;
            item = null;
            return runtime;
        }
        finally
        {
            DisposeIfPossible(item);
            DisposeIfPossible(device);
            native?.Dispose();
        }
    }

    public void Dispose()
    {
        DisposeIfPossible(Item);
        DisposeIfPossible(Device);
        Native.Dispose();
    }

    private static void DisposeIfPossible(object? value)
    {
        if (value is IDisposable disposable)
        {
            disposable.Dispose();
        }
    }
}

internal sealed class RegionCaptureSource : IDisposable
{
    public RegionCaptureSource(CaptureRuntime runtime, CapturedGpuFrame gpuFrame)
    {
        Runtime = runtime;
        GpuFrame = gpuFrame;
    }

    public CaptureRuntime Runtime { get; }
    public CapturedGpuFrame GpuFrame { get; }

    public void Dispose()
    {
        GpuFrame.Dispose();
    }
}
