using System.Windows.Forms;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.DirectX.Direct3D11;

internal static partial class Program
{
    [STAThread]
    private static async Task<int> Main(string[] args)
    {
        WindowsFormsSynchronizationContext.AutoInstall = false;
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        CaptureText.Initialize(args);
        Console.WriteLine("HDR SDR Capture Helper");
        Console.WriteLine("Capturing the primary monitor by default. Pass --picker to choose a screen/window.");

        if (CaptureArgs.Has(args, "--check-store-license"))
        {
            return await CheckStoreLicenseAsync();
        }

        if (CaptureArgs.Has(args, "--server"))
        {
            return await RunCommandServerAsync(args);
        }

        ToneMapOptions toneMap = ToneMapOptions.FromArgs(args);
        bool explicitOutput = CaptureArgs.Has(args, "--output");
        string outputPath = ResolveOutputPath(args);

        string? editFilePath = CaptureArgs.Value(args, "--edit-file");
        if (!string.IsNullOrWhiteSpace(editFilePath))
        {
            return EditExistingImage(editFilePath, outputPath, !CaptureArgs.Has(args, "--skip-initial-copy"));
        }

        if (!GraphicsCaptureSession.IsSupported())
        {
            Console.WriteLine("Windows Graphics Capture is not supported on this system.");
            return 1;
        }

        DirectXPixelFormat format = CaptureArgs.Has(args, "--bgra8")
            ? DirectXPixelFormat.B8G8R8A8UIntNormalized
            : DirectXPixelFormat.R16G16B16A16Float;
        bool selectRegion = CaptureArgs.Has(args, "--select-region");
        bool picker = CaptureArgs.Has(args, "--picker");
        bool diagnostic = CaptureArgs.Has(args, "--diagnostic");
        bool fullscreenClip = CaptureArgs.Has(args, "--fullscreen-clip");
        if (selectRegion && !picker && !fullscreenClip)
        {
            return await CaptureSelectedRegionFastAsync(format, outputPath, toneMap, diagnostic);
        }

        using NativeD3D native = NativeD3D.Create();
        IDirect3DDevice winrtDevice = native.CreateWinRtDevice();

        GraphicsCaptureItem? item = null;
        using Form? owner = args.Any(arg => string.Equals(arg, "--picker", StringComparison.OrdinalIgnoreCase))
            ? CreateHiddenOwnerWindow()
            : null;
        if (owner is not null)
        {
            Console.WriteLine("Select the HDR screen/window in the Windows Graphics Capture picker.");
            item = await PickCaptureItemAsync(owner.Handle);
        }
        if (item is null)
        {
            Console.WriteLine("Using primary monitor capture item.");
            await WgcCapture.RequestProgrammaticCaptureAccessAsync();
            item = WgcCapture.CreatePrimaryMonitorItem();
        }

        Console.WriteLine($"Selected: {item.DisplayName}");
        Console.WriteLine($"Item size: {item.Size.Width} x {item.Size.Height}");

        int exitCode;
        try
        {
            exitCode = await CaptureOneFrameAsync(item, winrtDevice, native, format, outputPath, toneMap,
                selectRegion, !explicitOutput || CaptureArgs.Has(args, "--edit"),
                CaptureArgs.Has(args, "--discard-output"), explicitOutput, diagnostic,
                fullscreenClip);
        }
        finally
        {
            DisposeIfPossible(winrtDevice);
            DisposeIfPossible(item);
        }
        if (exitCode == 0 && CaptureArgs.Has(args, "--open-folder") && explicitOutput)
        {
            OpenOutputInExplorer(outputPath);
        }

        Console.WriteLine("Done.");
        return exitCode;
    }

    private static void DisposeIfPossible(object? value)
    {
        if (value is IDisposable disposable)
        {
            disposable.Dispose();
        }
    }

}
