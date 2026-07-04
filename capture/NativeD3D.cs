using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using WinRT;
using Windows.Graphics.DirectX.Direct3D11;

internal sealed class NativeD3D : IDisposable
{
    private const uint D3d11CreateDeviceBgraSupport = 0x20;
    private const uint D3d11SdkVersion = 7;
    private const uint D3dDriverTypeHardware = 1;
    private const uint D3d11UsageStaging = 3;
    private const uint D3d11CpuAccessRead = 0x20000;
    private const uint D3d11MapRead = 1;

    private static readonly Guid IidIdxgiDevice = new("54ec77fa-1377-44e6-8c32-88fd5f44c84c");

    private readonly nint device;
    private readonly nint context;

    private NativeD3D(nint device, nint context)
    {
        this.device = device;
        this.context = context;
    }

    public static NativeD3D Create()
    {
        uint[] featureLevels = { 0xb100, 0xb000 };
        int hr = D3D11CreateDevice(
            0,
            D3dDriverTypeHardware,
            0,
            D3d11CreateDeviceBgraSupport,
            featureLevels,
            (uint)featureLevels.Length,
            D3d11SdkVersion,
            out nint device,
            out uint createdFeatureLevel,
            out nint context);
        ThrowIfFailed(hr, "D3D11CreateDevice");
        Console.WriteLine($"D3D feature level: 0x{createdFeatureLevel:x}");
        return new NativeD3D(device, context);
    }

    public IDirect3DDevice CreateWinRtDevice()
    {
        Guid iid = IidIdxgiDevice;
        int hr = Marshal.QueryInterface(device, ref iid, out nint dxgiDevice);
        ThrowIfFailed(hr, "ID3D11Device::QueryInterface(IDXGIDevice)");

        try
        {
            hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, out nint graphicsDevice);
            ThrowIfFailed(hr, "CreateDirect3D11DeviceFromDXGIDevice");
            try
            {
                return MarshalInterface<IDirect3DDevice>.FromAbi(graphicsDevice);
            }
            finally
            {
                Marshal.Release(graphicsDevice);
            }
        }
        finally
        {
            Marshal.Release(dxgiDevice);
        }
    }

    public unsafe ReadbackResult ReadbackTexture(nint texture, ToneMapOptions toneMap, bool diagnostic, System.Drawing.Rectangle? readbackRegion = null)
    {
        D3d11Texture2dDesc desc = default;
        GetTextureDesc(texture, ref desc);
        if (diagnostic)
        {
            Console.WriteLine($"Native texture desc: {desc.Width} x {desc.Height}, format {DxgiFormats.Name(desc.Format)} ({desc.Format})");
        }

        System.Drawing.Rectangle region = readbackRegion ?? new System.Drawing.Rectangle(0, 0, checked((int)desc.Width), checked((int)desc.Height));
        region.Intersect(new System.Drawing.Rectangle(0, 0, checked((int)desc.Width), checked((int)desc.Height)));
        if (region.Width <= 0 || region.Height <= 0)
        {
            throw new InvalidOperationException("Selected readback region is empty.");
        }

        D3d11Texture2dDesc stagingDesc = desc;
        stagingDesc.Width = checked((uint)region.Width);
        stagingDesc.Height = checked((uint)region.Height);
        stagingDesc.Usage = D3d11UsageStaging;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3d11CpuAccessRead;
        stagingDesc.MiscFlags = 0;

        nint staging = 0;
        int hr = CreateTexture2D(device, ref stagingDesc, 0, out staging);
        ThrowIfFailed(hr, "ID3D11Device::CreateTexture2D(staging)");

        try
        {
            if (region.X == 0 && region.Y == 0 && region.Width == desc.Width && region.Height == desc.Height)
            {
                CopyResource(context, staging, texture);
            }
            else
            {
                D3d11Box sourceBox = new()
                {
                    Left = checked((uint)region.Left),
                    Top = checked((uint)region.Top),
                    Front = 0,
                    Right = checked((uint)region.Right),
                    Bottom = checked((uint)region.Bottom),
                    Back = 1
                };
                CopySubresourceRegion(context, staging, 0, 0, 0, 0, texture, 0, ref sourceBox);
            }
            D3d11MappedSubresource mapped = default;
            hr = Map(context, staging, 0, D3d11MapRead, 0, out mapped);
            ThrowIfFailed(hr, "ID3D11DeviceContext::Map");

            try
            {
                PixelStats stats = diagnostic
                    ? PixelStats.FromMapped(mapped.Data, mapped.RowPitch, desc.Format, stagingDesc.Width, stagingDesc.Height)
                    : default;
                byte[] bgra = ToneMapper.ConvertToBgra(mapped.Data, mapped.RowPitch, desc.Format, stagingDesc.Width, stagingDesc.Height, toneMap);
                return new ReadbackResult(stagingDesc.Width, stagingDesc.Height, desc.Format, stats, bgra);
            }
            finally
            {
                Unmap(context, staging, 0);
            }
        }
        finally
        {
            if (staging != 0)
            {
                Marshal.Release(staging);
            }
        }
    }

    public void Dispose()
    {
        if (context != 0) Marshal.Release(context);
        if (device != 0) Marshal.Release(device);
    }

    private static void ThrowIfFailed(int hr, string operation)
    {
        if (hr < 0)
        {
            Marshal.ThrowExceptionForHR(hr);
            throw new UnreachableException(operation);
        }
    }

    [DllImport("d3d11.dll")]
    private static extern int D3D11CreateDevice(
        nint adapter,
        uint driverType,
        nint software,
        uint flags,
        uint[] featureLevels,
        uint featureLevelCount,
        uint sdkVersion,
        out nint device,
        out uint createdFeatureLevel,
        out nint immediateContext);

    [DllImport("d3d11.dll")]
    private static extern int CreateDirect3D11DeviceFromDXGIDevice(nint dxgiDevice, out nint graphicsDevice);

    [StructLayout(LayoutKind.Sequential)]
    private struct DxgiSampleDesc
    {
        public uint Count;
        public uint Quality;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct D3d11Texture2dDesc
    {
        public uint Width;
        public uint Height;
        public uint MipLevels;
        public uint ArraySize;
        public uint Format;
        public DxgiSampleDesc SampleDesc;
        public uint Usage;
        public uint BindFlags;
        public uint CPUAccessFlags;
        public uint MiscFlags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct D3d11MappedSubresource
    {
        public nint Data;
        public uint RowPitch;
        public uint DepthPitch;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct D3d11Box
    {
        public uint Left;
        public uint Top;
        public uint Front;
        public uint Right;
        public uint Bottom;
        public uint Back;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void GetDescDelegate(nint self, ref D3d11Texture2dDesc desc);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int CreateTexture2DDelegate(nint self, ref D3d11Texture2dDesc desc, nint initialData, out nint texture);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void CopySubresourceRegionDelegate(
        nint self,
        nint destinationResource,
        uint destinationSubresource,
        uint destinationX,
        uint destinationY,
        uint destinationZ,
        nint sourceResource,
        uint sourceSubresource,
        ref D3d11Box sourceBox);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void CopyResourceDelegate(nint self, nint destination, nint source);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int MapDelegate(nint self, nint resource, uint subresource, uint mapType, uint mapFlags, out D3d11MappedSubresource mapped);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void UnmapDelegate(nint self, nint resource, uint subresource);

    private static unsafe T ComMethod<T>(nint unknown, int slot) where T : Delegate
    {
        nint vtable = *(nint*)unknown;
        nint method = *((nint*)vtable + slot);
        return Marshal.GetDelegateForFunctionPointer<T>(method);
    }

    private static void GetTextureDesc(nint texture, ref D3d11Texture2dDesc desc) =>
        ComMethod<GetDescDelegate>(texture, 10)(texture, ref desc);

    private static int CreateTexture2D(nint device, ref D3d11Texture2dDesc desc, nint initialData, out nint texture) =>
        ComMethod<CreateTexture2DDelegate>(device, 5)(device, ref desc, initialData, out texture);

    private static void CopySubresourceRegion(nint context, nint destination, uint destinationSubresource, uint destinationX, uint destinationY, uint destinationZ, nint source, uint sourceSubresource, ref D3d11Box sourceBox) =>
        ComMethod<CopySubresourceRegionDelegate>(context, 46)(context, destination, destinationSubresource, destinationX, destinationY, destinationZ, source, sourceSubresource, ref sourceBox);

    private static void CopyResource(nint context, nint destination, nint source) =>
        ComMethod<CopyResourceDelegate>(context, 47)(context, destination, source);

    private static int Map(nint context, nint resource, uint subresource, uint mapType, uint mapFlags, out D3d11MappedSubresource mapped) =>
        ComMethod<MapDelegate>(context, 14)(context, resource, subresource, mapType, mapFlags, out mapped);

    private static void Unmap(nint context, nint resource, uint subresource) =>
        ComMethod<UnmapDelegate>(context, 15)(context, resource, subresource);
}
