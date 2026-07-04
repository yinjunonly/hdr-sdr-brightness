using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using Windows.Foundation;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.DirectX.Direct3D11;
using Windows.Services.Store;
using WinRT;
using WinRT.Interop;

internal static class Program
{
    private const string PipeName = "HdrSdrBrightnessCapture";
    private static int activeRegionCapture;
    private const uint WdaExcludeFromCapture = 0x00000011;
    private const int DwmwaUseImmersiveDarkMode = 20;
    private const int DwmwaUseImmersiveDarkModeLegacy = 19;
    private const int DwmwaBorderColor = 34;
    private const int DwmwaCaptionColor = 35;
    private const int DwmwaTextColor = 36;
    private static readonly nint HwndTopmost = new(-1);
    private static readonly nint HwndNoTopmost = new(-2);
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpShowWindow = 0x0040;
    private const uint SwpHideWindow = 0x0080;
    private const uint SwpNoActivate = 0x0010;
    private const int SwHide = 0;
    private const int WmHotkey = 0x0312;
    private const int VkEscape = 0x1B;

    private static readonly Guid IidGraphicsCaptureItem = new("79c3f95b-31f7-4ec2-a464-632ef5d30760");
    private static readonly Guid IidGraphicsCaptureItemInterop = new("3628e81b-3cac-4c60-b7f4-23ce0e0c3356");

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

    private static async Task<int> CheckStoreLicenseAsync()
    {
        try
        {
            StoreContext context = StoreContext.GetDefault();
            StoreAppLicense license = await context.GetAppLicenseAsync();
            return license.IsActive ? 0 : 2;
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex);
            return 1;
        }
    }

    private static async Task<int> RunCommandServerAsync(string[] args)
    {
        using CancellationTokenSource cancellation = new();
        int parentPid = CaptureArgs.Int(args, "--parent-pid", 0);
        int requestedIdleTimeoutMs = CaptureArgs.Int(args, "--idle-timeout-ms", 90000);
        int idleTimeoutMs = requestedIdleTimeoutMs <= 0 ? 0 : Math.Max(5000, requestedIdleTimeoutMs);
        if (parentPid > 0)
        {
            _ = Task.Run(async () =>
            {
                try
                {
                    using Process parent = Process.GetProcessById(parentPid);
                    while (!parent.HasExited && !cancellation.IsCancellationRequested)
                    {
                        await Task.Delay(1000, cancellation.Token);
                        parent.Refresh();
                    }
                }
                catch
                {
                }

                cancellation.Cancel();
            });
        }

        _ = WgcCapture.GetSharedRuntimeAsync();

        while (!cancellation.IsCancellationRequested)
        {
            try
            {
                await using NamedPipeServerStream pipe = new(
                    PipeName,
                    PipeDirection.InOut,
                    1,
                    PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous);
                Task waitForConnection = pipe.WaitForConnectionAsync(cancellation.Token);
                Task completed = idleTimeoutMs > 0
                    ? await Task.WhenAny(waitForConnection, Task.Delay(idleTimeoutMs, cancellation.Token))
                    : await Task.WhenAny(waitForConnection);
                if (completed != waitForConnection)
                {
                    if (System.Threading.Volatile.Read(ref activeRegionCapture) == 1)
                    {
                        continue;
                    }
                    break;
                }
                await waitForConnection;
                using StreamReader reader = new(pipe, Encoding.Unicode, false, 1024, leaveOpen: true);
                string? command = await reader.ReadLineAsync(cancellation.Token);
                if (!string.IsNullOrWhiteSpace(command))
                {
                    int resultCode = await DispatchServerCommandAsync(command, cancellation.Token);
                    await TryWritePipeResponseAsync(pipe, resultCode, cancellation.Token);
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (IOException)
            {
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex);
                await Task.Delay(250);
            }
        }

        return 0;
    }

    private static async Task<int> DispatchServerCommandAsync(string command, CancellationToken cancellationToken)
    {
        string[] parts = command.Split('\t');
        if (parts.Length == 0) return 2;

        if (string.Equals(parts[0], "select-region", StringComparison.OrdinalIgnoreCase))
        {
            if (System.Threading.Interlocked.Exchange(ref activeRegionCapture, 1) == 1)
            {
                return 2;
            }

            int language = ParseCommandInt(parts, 1, 0);
            _ = Task.Run(async () =>
            {
                try
                {
                    await RunServerRegionCaptureAsync(language);
                }
                catch (Exception ex)
                {
                    Debug.WriteLine(ex);
                }
                finally
                {
                    System.Threading.Volatile.Write(ref activeRegionCapture, 0);
                }
            }, cancellationToken);
            return 0;
        }

        return await HandleServerCommandAsync(parts);
    }

    private static int ParseCommandInt(string[] parts, int index, int fallback)
    {
        return index < parts.Length &&
            int.TryParse(parts[index], System.Globalization.NumberStyles.Integer,
                System.Globalization.CultureInfo.InvariantCulture, out int value)
            ? value
            : fallback;
    }

    private static async Task TryWritePipeResponseAsync(NamedPipeServerStream pipe, int resultCode, CancellationToken cancellationToken)
    {
        if (!pipe.IsConnected || !pipe.CanWrite) return;
        try
        {
            await using StreamWriter writer = new(pipe, Encoding.Unicode, 1024, leaveOpen: true);
            await writer.WriteLineAsync(resultCode.ToString(System.Globalization.CultureInfo.InvariantCulture).AsMemory(), cancellationToken);
            await writer.FlushAsync(cancellationToken);
        }
        catch
        {
        }
    }

    private static async Task RunServerRegionCaptureAsync(int language)
    {
        if (language > 0) CaptureText.SetLanguageId(language);

        ToneMapOptions toneMap = ToneMapOptions.FromArgs(Array.Empty<string>());
        string outputPath = ResolveOutputPath(Array.Empty<string>());
        await CaptureSelectedRegionFastAsync(DirectXPixelFormat.R16G16B16A16Float, outputPath, toneMap, false);
    }

    private static async Task<int> HandleServerCommandAsync(string[] parts)
    {
        if (string.Equals(parts[0], "fullscreen-clip", StringComparison.OrdinalIgnoreCase))
        {
            int language = ParseCommandInt(parts, 1, 0);
            if (language > 0) CaptureText.SetLanguageId(language);

            string outputPath = parts.Length > 2 && !string.IsNullOrWhiteSpace(parts[2])
                ? parts[2]
                : ResolveOutputPath(Array.Empty<string>());
            ToneMapOptions toneMap = ToneMapOptions.FromArgs(Array.Empty<string>());
            try
            {
                CaptureRuntime runtime = await WgcCapture.GetSharedRuntimeAsync();
                ReadbackResult result = await WgcCapture.CaptureFrameAsync(runtime.Item, runtime.Device, runtime.Native,
                    DirectXPixelFormat.R16G16B16A16Float, toneMap, false);
                RunSta(() => CopyBgraToClipboard(result.Width, result.Height, result.Bgra));
                if (!string.IsNullOrWhiteSpace(outputPath))
                {
                    await SaveFullscreenEditImageAsync(outputPath, result.Width, result.Height, result.Bgra);
                }
                return 0;
            }
            catch (TimeoutException)
            {
                Console.WriteLine("Timed out waiting for a WGC frame.");
                WgcCapture.ResetSharedRuntime();
                return 4;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Capture failed: {ex.GetType().Name}: {ex.Message}");
                WgcCapture.ResetSharedRuntime();
                return 8;
            }
        }

        return 2;
    }

    private static void DisposeIfPossible(object? value)
    {
        if (value is IDisposable disposable)
        {
            disposable.Dispose();
        }
    }

    private static string ResolveOutputPath(string[] args)
    {
        string? requested = CaptureArgs.Value(args, "--output");
        if (!string.IsNullOrWhiteSpace(requested))
        {
            return Path.GetFullPath(Environment.ExpandEnvironmentVariables(requested));
        }

        string pictures = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);
        if (string.IsNullOrWhiteSpace(pictures)) pictures = Environment.CurrentDirectory;
        string directory = Path.Combine(pictures, "HDR SDR Brightness");
        string fileName = $"HDR-SDR-Capture-{DateTime.Now:yyyyMMdd-HHmmss}.png";
        return Path.Combine(directory, fileName);
    }

    private static void OpenOutputInExplorer(string outputPath)
    {
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = $"/select,\"{outputPath}\"",
                UseShellExecute = false
            });
        }
        catch
        {
        }
    }

    private static Form CreateHiddenOwnerWindow()
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Form owner = new()
        {
            Text = CaptureText.Get(CaptureString.HiddenOwnerTitle),
            ShowInTaskbar = false,
            StartPosition = FormStartPosition.Manual,
            Size = new System.Drawing.Size(1, 1),
            Location = new System.Drawing.Point(-32000, -32000)
        };
        owner.Show();
        return owner;
    }

    private static async Task<GraphicsCaptureItem?> PickCaptureItemAsync(nint ownerHwnd)
    {
        GraphicsCapturePicker picker = new();
        InitializeWithWindow.Initialize(picker, ownerHwnd);
        return await picker.PickSingleItemAsync();
    }

    private static async Task<int> CaptureOneFrameAsync(
        GraphicsCaptureItem item,
        IDirect3DDevice device,
        NativeD3D native,
        DirectXPixelFormat requestedFormat,
        string outputPath,
        ToneMapOptions toneMap,
        bool selectRegion,
        bool editAfterCapture,
        bool discardOutput,
        bool saveFullscreenOutput,
        bool diagnostic,
        bool fullscreenClip = false)
    {
        if (fullscreenClip)
        {
            // 无 UI 全屏截图模式：捕获一帧直接复制到剪贴板，不保存文件
            ReadbackResult result;
            try
            {
                result = await WgcCapture.CaptureFrameAsync(item, device, native, requestedFormat, toneMap, diagnostic);
            }
            catch (TimeoutException)
            {
                Console.WriteLine("Timed out waiting for a WGC frame.");
                return 4;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Capture failed: {ex.GetType().Name}: {ex.Message}");
                return 8;
            }
            RunSta(() => CopyBgraToClipboard(result.Width, result.Height, result.Bgra));
            if (saveFullscreenOutput)
            {
                try
                {
                    await SaveFullscreenEditImageAsync(outputPath, result.Width, result.Height, result.Bgra);
                    Console.WriteLine($"Saved edit image: {outputPath}");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Save failed: {ex.GetType().Name}: {ex.Message}");
                    return 8;
                }
            }
            Console.WriteLine("Fullscreen screenshot copied to clipboard.");
            return 0;
        }

        if (selectRegion)
        {
            Task<ReadbackResult> regionCaptureTask = Task.Run(() =>
                WgcCapture.CaptureFrameAsync(item, device, native, requestedFormat, toneMap, diagnostic));
            return await SelectAndCommitRegionAsync(regionCaptureTask, outputPath, diagnostic);
        }

        Task<ReadbackResult> captureTask = Task.Run(() => WgcCapture.CaptureFrameAsync(item, device, native, requestedFormat, toneMap, diagnostic));
        ReadbackResult fullResult;
        try
        {
            fullResult = await captureTask;
        }
        catch (TimeoutException)
        {
            Console.WriteLine("Timed out waiting for a WGC frame.");
            return 4;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Capture failed for {requestedFormat}: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }

        if (editAfterCapture)
        {
            ShowPreviewEditor(fullResult, outputPath);
            return 0;
        }

        if (discardOutput)
        {
            return 0;
        }

        try
        {
            await SavePngAsync(outputPath, fullResult.Width, fullResult.Height, fullResult.Bgra);
            Console.WriteLine($"Saved PNG: {outputPath}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Save failed: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }
    }

    private static async Task<int> CaptureSelectedRegionFastAsync(
        DirectXPixelFormat requestedFormat,
        string outputPath,
        ToneMapOptions toneMap,
        bool diagnostic)
    {
        Task<ReadbackResult> captureTask = Task.Run(async () =>
        {
            using RegionCaptureSource captureSource = await WgcCapture.CapturePrimaryMonitorGpuFrameAsync(requestedFormat);
            return captureSource.Runtime.Native.ReadbackTexture(
                captureSource.GpuFrame.Texture,
                toneMap,
                diagnostic);
        });
        return await SelectAndCommitRegionAsync(captureTask, outputPath, diagnostic);
    }

    private static async Task<int> SelectAndCommitRegionAsync(
        Task<ReadbackResult> captureTask,
        string outputPath,
        bool diagnostic)
    {
        Bitmap previewBitmap;
        try
        {
            previewBitmap = CreateFastScreenPreviewBitmap();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Fast preview failed: {ex.GetType().Name}: {ex.Message}");
            previewBitmap = new Bitmap(1, 1, PixelFormat.Format32bppArgb);
        }

        RegionSelectionForm.RegionSelectionResult? selection = null;
        RunSta(() =>
        {
            using RegionSelectionForm form = new(previewBitmap);
            _ = captureTask.ContinueWith(t =>
            {
                if (!t.IsCompletedSuccessfully) return;
                Bitmap hdrPreview = CreateBitmapFromBgra(t.Result.Width, t.Result.Height, t.Result.Bgra);
                try
                {
                    form.BeginInvoke(() =>
                    {
                        if (form.IsDisposed)
                        {
                            hdrPreview.Dispose();
                            return;
                        }
                        form.ReplacePreview(hdrPreview);
                    });
                }
                catch
                {
                    hdrPreview.Dispose();
                }
            }, TaskScheduler.Default);
            selection = form.ShowDialog() == DialogResult.OK
                ? new RegionSelectionForm.RegionSelectionResult(
                    form.SelectedImageRegion,
                    form.CommitAction,
                    form.SelectedPreset,
                    form.Operations)
                : null;
        });

        if (!selection.HasValue)
        {
            if (captureTask.IsFaulted)
            {
                try
                {
                    await captureTask;
                }
                catch (TimeoutException)
                {
                    Console.WriteLine("Timed out waiting for a WGC frame.");
                    return 4;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Capture failed: {ex.GetType().Name}: {ex.Message}");
                    return 8;
                }
            }
            Console.WriteLine("Region selection cancelled.");
            return 2;
        }

        ReadbackResult fullCapture;
        try
        {
            fullCapture = await captureTask;
        }
        catch (TimeoutException)
        {
            Console.WriteLine("Timed out waiting for a WGC frame.");
            return 4;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Capture failed: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }

        ReadbackResult result = CropResult(fullCapture, selection.Value.Region);
        if (diagnostic) result.PrintStats();

        result = ApplySelectionEdits(result, selection.Value);
        Console.WriteLine($"Selected region: {selection.Value.Region.X},{selection.Value.Region.Y} {selection.Value.Region.Width}x{selection.Value.Region.Height}");
        if (selection.Value.Action == RegionSelectionForm.SelectionCommitAction.Copy)
        {
            RunSta(() => CopyBgraToClipboard(result.Width, result.Height, result.Bgra));
            Console.WriteLine("Copied selected region to clipboard.");
            return 0;
        }

        string? savePath = PromptSaveAsPath(outputPath);
        if (string.IsNullOrWhiteSpace(savePath))
        {
            Console.WriteLine("Save cancelled.");
            return 2;
        }

        try
        {
            await SavePngAsync(savePath, result.Width, result.Height, result.Bgra);
            Console.WriteLine($"Saved PNG: {savePath}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Save failed: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }
    }

    private static Bitmap CreateFastScreenPreviewBitmap()
    {
        System.Drawing.Rectangle bounds = Screen.PrimaryScreen?.Bounds ??
            new System.Drawing.Rectangle(0, 0, 1, 1);
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            bounds = new System.Drawing.Rectangle(0, 0, 1, 1);
        }

        Bitmap bitmap = new(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
        bitmap.SetResolution(96.0f, 96.0f);
        using Graphics graphics = Graphics.FromImage(bitmap);
        graphics.CopyFromScreen(bounds.Left, bounds.Top, 0, 0, bounds.Size, CopyPixelOperation.SourceCopy);
        return bitmap;
    }

    private static string? PromptSaveAsPath(string defaultOutputPath)
    {
        string? selectedPath = null;
        RunSta(() =>
        {
            string? directory = Path.GetDirectoryName(defaultOutputPath);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }

            using SaveFileDialog dialog = new()
            {
                Title = CaptureText.Get(CaptureString.SaveDialogTitle),
                Filter = CaptureText.Get(CaptureString.PngFilter),
                DefaultExt = "png",
                AddExtension = true,
                OverwritePrompt = true,
                FileName = Path.GetFileName(defaultOutputPath),
                InitialDirectory = directory
            };
            selectedPath = dialog.ShowDialog() == DialogResult.OK ? dialog.FileName : null;
        });
        return selectedPath;
    }

    private static async Task SavePngAsync(string path, uint width, uint height, byte[] bgra)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        await Task.Run(() => bitmap.Save(path, ImageFormat.Png));
    }

    private static Task SaveFullscreenEditImageAsync(string path, uint width, uint height, byte[] bgra)
    {
        if (!string.Equals(Path.GetExtension(path), ".bmp", StringComparison.OrdinalIgnoreCase))
        {
            return SavePngAsync(path, width, height, bgra);
        }

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        bitmap.Save(path, ImageFormat.Bmp);
        return Task.CompletedTask;
    }

    private static void ShowPreviewEditor(ReadbackResult result, string defaultOutputPath)
    {
        using Bitmap bitmap = CreateBitmapFromBgra(result.Width, result.Height, result.Bgra);
        ShowPreviewEditor(bitmap, defaultOutputPath);
    }

    private static void ShowPreviewEditor(Bitmap source, string defaultOutputPath, bool copyOnShown = true)
    {
        RunSta(() =>
        {
            using RegionSelectionForm.PreviewEditorForm editor = new(new Bitmap(source), defaultOutputPath, copyOnShown);
            editor.ShowDialog();
        });
    }

    private static int EditExistingImage(string path, string defaultOutputPath, bool copyOnShown)
    {
        try
        {
            string fullPath = Path.GetFullPath(Environment.ExpandEnvironmentVariables(path));
            if (!File.Exists(fullPath))
            {
                Console.WriteLine($"Image not found: {fullPath}");
                return 2;
            }

            using Image loaded = Image.FromFile(fullPath);
            using Bitmap bitmap = new(loaded);
            bitmap.SetResolution(96.0f, 96.0f);
            ShowPreviewEditor(bitmap, defaultOutputPath, copyOnShown);
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Edit failed: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }
    }

    private static void CopyBgraToClipboard(uint width, uint height, byte[] bgra)
    {
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        RunSta(() =>
        {
            using Bitmap clipboardBitmap = new(bitmap);
            clipboardBitmap.SetResolution(96.0f, 96.0f);
            using MemoryStream pngStream = new();
            clipboardBitmap.Save(pngStream, ImageFormat.Png);
            DataObject data = new();
            data.SetData("PNG", false, new MemoryStream(pngStream.ToArray()));
            data.SetData(DataFormats.Bitmap, true, new Bitmap(clipboardBitmap));
            Clipboard.SetDataObject(data, true, 5, 120);
        });
    }
    private static ReadbackResult ApplySelectionEdits(ReadbackResult source, RegionSelectionForm.RegionSelectionResult selection)
    {
        if (selection.Preset == RegionSelectionForm.AdjustmentPreset.Balanced && selection.Operations.Count == 0)
        {
            return source;
        }

        byte[] bgra = source.Bgra;
        ApplyPresetToBgra(bgra, (int)source.Width, (int)source.Height, selection.Preset);

        List<RegionSelectionForm.EditOperation> drawOperations = new();
        foreach (RegionSelectionForm.EditOperation operation in selection.Operations)
        {
            System.Drawing.Rectangle rect = operation.Rect;
            rect.Offset(-selection.Region.X, -selection.Region.Y);
            rect.Intersect(new System.Drawing.Rectangle(0, 0, (int)source.Width, (int)source.Height));
            if ((operation.Type == RegionSelectionForm.EditOperationType.Pen || operation.Type == RegionSelectionForm.EditOperationType.Mosaic) && operation.Points is not null)
            {
                List<System.Drawing.Point> points = NormalizeOperationPoints(
                    operation.Points,
                    selection.Region,
                    new System.Drawing.Rectangle(0, 0, (int)source.Width, (int)source.Height));
                if (points.Count(point => !IsPathBreak(point)) > 1)
                {
                    if (operation.Type == RegionSelectionForm.EditOperationType.Mosaic)
                    {
                        ApplyMosaicBrushToBgra(bgra, (int)source.Width, (int)source.Height, points, operation.StrokeWidth);
                    }
                    else
                    {
                        drawOperations.Add(operation with { Points = points, Rect = System.Drawing.Rectangle.Empty });
                    }
                }
                continue;
            }
            if (rect.Width <= 0 || rect.Height <= 0) continue;
            if (operation.Type == RegionSelectionForm.EditOperationType.Mosaic) ApplyMosaicToBgra(bgra, (int)source.Width, (int)source.Height, rect, operation.StrokeWidth);
            else drawOperations.Add(operation with { Rect = rect });
        }

        if (drawOperations.Count > 0)
        {
            using Bitmap bitmap = CreateBitmapFromBgra(source.Width, source.Height, bgra);
            using (Graphics graphics = Graphics.FromImage(bitmap))
            {
                graphics.SmoothingMode = SmoothingMode.AntiAlias;
                foreach (RegionSelectionForm.EditOperation operation in drawOperations)
                {
                    using Pen pen = new(operation.Color, Math.Max(1, operation.StrokeWidth)) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
                    if (operation.Type == RegionSelectionForm.EditOperationType.Marker) graphics.DrawRectangle(pen, operation.Rect);
                    if (operation.Type == RegionSelectionForm.EditOperationType.Ellipse) graphics.DrawEllipse(pen, operation.Rect);
                    if (operation.Type == RegionSelectionForm.EditOperationType.Pen && operation.Points is { Count: > 1 })
                    {
                        DrawSegmentedLines(graphics, pen, operation.Points);
                    }
                }
            }
            bgra = ExtractBgra(bitmap);
        }

        return source with { Bgra = bgra };
    }

    private static List<System.Drawing.Point> NormalizeOperationPoints(
        IReadOnlyList<System.Drawing.Point> points,
        System.Drawing.Rectangle sourceRegion,
        System.Drawing.Rectangle bounds)
    {
        List<System.Drawing.Point> normalized = new();
        bool hasOpenSegment = false;
        foreach (System.Drawing.Point point in points)
        {
            if (IsPathBreak(point))
            {
                AddPathBreak(normalized, ref hasOpenSegment);
                continue;
            }

            System.Drawing.Point shifted = new(point.X - sourceRegion.X, point.Y - sourceRegion.Y);
            if (!bounds.Contains(shifted))
            {
                AddPathBreak(normalized, ref hasOpenSegment);
                continue;
            }

            normalized.Add(shifted);
            hasOpenSegment = true;
        }

        TrimPathBreaks(normalized);
        return normalized;
    }

    private static void AddPathBreak(List<System.Drawing.Point> points, ref bool hasOpenSegment)
    {
        if (hasOpenSegment && points.Count > 0 && !IsPathBreak(points[^1]))
        {
            points.Add(new System.Drawing.Point(int.MinValue, int.MinValue));
        }
        hasOpenSegment = false;
    }

    private static void TrimPathBreaks(List<System.Drawing.Point> points)
    {
        while (points.Count > 0 && IsPathBreak(points[^1]))
        {
            points.RemoveAt(points.Count - 1);
        }
    }

    private static void DrawSegmentedLines(Graphics graphics, Pen pen, IReadOnlyList<System.Drawing.Point> points)
    {
        List<System.Drawing.Point> segment = new();
        foreach (System.Drawing.Point point in points)
        {
            if (IsPathBreak(point))
            {
                if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
                segment.Clear();
                continue;
            }
            segment.Add(point);
        }
        if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
    }

    private static byte[] ExtractBgra(Bitmap bitmap)
    {
        byte[] bgra = new byte[checked(bitmap.Width * bitmap.Height * 4)];
        BitmapData data = bitmap.LockBits(new System.Drawing.Rectangle(0, 0, bitmap.Width, bitmap.Height),
            ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            int rowBytes = bitmap.Width * 4;
            for (int y = 0; y < bitmap.Height; y++)
            {
                Marshal.Copy(data.Scan0 + y * data.Stride, bgra, y * rowBytes, rowBytes);
            }
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
        return bgra;
    }

    private static void ApplyPresetToBgra(byte[] bgra, int width, int height, RegionSelectionForm.AdjustmentPreset preset)
    {
        (float exposure, float highlightProtect) = preset switch
        {
            RegionSelectionForm.AdjustmentPreset.Low => (0.88f, 1.22f),
            RegionSelectionForm.AdjustmentPreset.High => (1.12f, 0.82f),
            _ => (1.0f, 1.0f)
        };
        if (Math.Abs(exposure - 1.0f) < 0.001f && Math.Abs(highlightProtect - 1.0f) < 0.001f) return;
        for (int y = 0; y < height; y++)
        {
            int row = y * width * 4;
            for (int x = 0; x < width; x++)
            {
                int index = row + x * 4;
                float b = bgra[index] / 255.0f * exposure;
                float g = bgra[index + 1] / 255.0f * exposure;
                float r = bgra[index + 2] / 255.0f * exposure;
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance > 0.72f)
                {
                    float over = luminance - 0.72f;
                    float mapped = 0.72f + over / (1.0f + over * 4.0f * highlightProtect);
                    float scale = mapped / Math.Max(0.0001f, luminance);
                    r *= scale;
                    g *= scale;
                    b *= scale;
                }
                bgra[index] = PixelMath.ToByte(b);
                bgra[index + 1] = PixelMath.ToByte(g);
                bgra[index + 2] = PixelMath.ToByte(r);
            }
        }
    }

    private static void ApplyMosaicToBgra(byte[] bgra, int width, int height, System.Drawing.Rectangle rect)
    {
        int block = Math.Max(16, Math.Min(rect.Width, rect.Height) / 6);
        ApplyMosaicToBgra(bgra, width, height, rect, block);
    }

    private static void ApplyMosaicToBgra(byte[] bgra, int width, int height, System.Drawing.Rectangle rect, int block)
    {
        rect.Intersect(new System.Drawing.Rectangle(0, 0, width, height));
        if (rect.Width <= 0 || rect.Height <= 0) return;
        using Bitmap crop = CreateBitmapFromBgraRegion(bgra, width, rect);
        using Bitmap pixelated = CreatePseudoPixelatedBitmap(crop, Math.Max(10, block));
        CopyBitmapToBgraRegion(pixelated, bgra, width, rect);
    }

    private static Bitmap CreateBitmapFromBgraRegion(byte[] bgra, int sourceWidth, System.Drawing.Rectangle rect)
    {
        Bitmap bitmap = new(rect.Width, rect.Height, PixelFormat.Format32bppArgb);
        bitmap.SetResolution(96.0f, 96.0f);
        BitmapData data = bitmap.LockBits(new System.Drawing.Rectangle(0, 0, rect.Width, rect.Height),
            ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            int rowBytes = rect.Width * 4;
            for (int y = 0; y < rect.Height; y++)
            {
                int sourceOffset = ((rect.Y + y) * sourceWidth + rect.X) * 4;
                Marshal.Copy(bgra, sourceOffset, data.Scan0 + y * data.Stride, rowBytes);
            }
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
        return bitmap;
    }

    private static void CopyBitmapToBgraRegion(Bitmap bitmap, byte[] bgra, int destinationWidth, System.Drawing.Rectangle rect)
    {
        BitmapData data = bitmap.LockBits(new System.Drawing.Rectangle(0, 0, rect.Width, rect.Height),
            ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            int rowBytes = rect.Width * 4;
            byte[] row = new byte[rowBytes];
            for (int y = 0; y < rect.Height; y++)
            {
                Marshal.Copy(data.Scan0 + y * data.Stride, row, 0, rowBytes);
                int destinationOffset = ((rect.Y + y) * destinationWidth + rect.X) * 4;
                Buffer.BlockCopy(row, 0, bgra, destinationOffset, rowBytes);
            }
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
    }

    private static byte QuantizeColor(long value)
    {
        int quantized = (int)Math.Clamp(((value + 16) / 32) * 32, 0, 255);
        return (byte)quantized;
    }

    private static void ApplyMosaicBrushToBgra(byte[] bgra, int width, int height, IReadOnlyList<System.Drawing.Point> points, int brushSize)
    {
        if (points.Count == 0) return;
        int radius = Math.Max(4, brushSize / 2);
        int block = Math.Max(18, brushSize * 2);
        foreach (System.Drawing.Point point in InterpolatePath(points, Math.Max(2, radius / 2)))
        {
            System.Drawing.Rectangle rect = new(point.X - radius, point.Y - radius, radius * 2, radius * 2);
            rect.Intersect(new System.Drawing.Rectangle(0, 0, width, height));
            if (rect.Width > 0 && rect.Height > 0) ApplyPseudoMosaicToBgra(bgra, width, height, rect, block);
        }
    }

    private static void ApplyPseudoMosaicToBgra(byte[] bgra, int width, int height, System.Drawing.Rectangle rect, int block)
    {
        block = Math.Max(8, block);
        for (int y = rect.Top; y < rect.Bottom; y += block)
        {
            for (int x = rect.Left; x < rect.Right; x += block)
            {
                int blockWidth = Math.Min(block, rect.Right - x);
                int blockHeight = Math.Min(block, rect.Bottom - y);
                (byte b, byte g, byte r) = PseudoBgraBlockColor(bgra, width, height, rect, x, y, blockWidth, blockHeight);
                for (int py = y; py < y + blockHeight; py++)
                {
                    int row = py * width * 4;
                    for (int px = x; px < x + blockWidth; px++)
                    {
                        int index = row + px * 4;
                        bgra[index] = b;
                        bgra[index + 1] = g;
                        bgra[index + 2] = r;
                    }
                }
            }
        }
    }

    private static (byte B, byte G, byte R) PseudoBgraBlockColor(byte[] bgra, int width, int height, System.Drawing.Rectangle rect, int x, int y, int blockWidth, int blockHeight)
    {
        double u = rect.Width <= 1 ? 0.0 : (x + blockWidth * 0.5 - rect.Left) / Math.Max(1.0, rect.Width - 1.0);
        double v = rect.Height <= 1 ? 0.0 : (y + blockHeight * 0.5 - rect.Top) / Math.Max(1.0, rect.Height - 1.0);
        int topX = Math.Clamp((int)Math.Round(rect.Left + u * (rect.Width - 1)), 0, width - 1);
        int sideY = Math.Clamp((int)Math.Round(rect.Top + v * (rect.Height - 1)), 0, height - 1);
        ColorBgra top = GetBgra(bgra, width, topX, Math.Clamp(rect.Top, 0, height - 1));
        ColorBgra bottom = GetBgra(bgra, width, topX, Math.Clamp(rect.Bottom - 1, 0, height - 1));
        ColorBgra left = GetBgra(bgra, width, Math.Clamp(rect.Left, 0, width - 1), sideY);
        ColorBgra right = GetBgra(bgra, width, Math.Clamp(rect.Right - 1, 0, width - 1), sideY);
        double horizontalWeight = Math.Clamp(0.5 + (Math.Min(u, 1.0 - u) - Math.Min(v, 1.0 - v)), 0.15, 0.85);
        double verticalWeight = 1.0 - horizontalWeight;
        int noise = DeterministicNoise(x / Math.Max(1, blockWidth), y / Math.Max(1, blockHeight));
        byte b = QuantizeColor((long)(horizontalWeight * Lerp(left.B, right.B, u) + verticalWeight * Lerp(top.B, bottom.B, v) + noise));
        byte g = QuantizeColor((long)(horizontalWeight * Lerp(left.G, right.G, u) + verticalWeight * Lerp(top.G, bottom.G, v) + noise));
        byte r = QuantizeColor((long)(horizontalWeight * Lerp(left.R, right.R, u) + verticalWeight * Lerp(top.R, bottom.R, v) + noise));
        return (b, g, r);
    }

    private readonly record struct ColorBgra(byte B, byte G, byte R);

    private static ColorBgra GetBgra(byte[] bgra, int width, int x, int y)
    {
        int index = (y * width + x) * 4;
        return new ColorBgra(bgra[index], bgra[index + 1], bgra[index + 2]);
    }

    private static Bitmap CreatePseudoPixelatedBitmap(Bitmap source, int pixelSize)
    {
        pixelSize = Math.Max(2, pixelSize);
        int smallWidth = Math.Max(1, source.Width / pixelSize);
        int smallHeight = Math.Max(1, source.Height / pixelSize);
        using Bitmap small = new(smallWidth, smallHeight, PixelFormat.Format32bppArgb);
        for (int y = 0; y < smallHeight; y++)
        {
            double v = smallHeight <= 1 ? 0.0 : y / (double)(smallHeight - 1);
            for (int x = 0; x < smallWidth; x++)
            {
                double u = smallWidth <= 1 ? 0.0 : x / (double)(smallWidth - 1);
                Color top = source.GetPixel(Math.Clamp((int)Math.Round(u * (source.Width - 1)), 0, source.Width - 1), 0);
                Color bottom = source.GetPixel(Math.Clamp((int)Math.Round(u * (source.Width - 1)), 0, source.Width - 1), source.Height - 1);
                Color left = source.GetPixel(0, Math.Clamp((int)Math.Round(v * (source.Height - 1)), 0, source.Height - 1));
                Color right = source.GetPixel(source.Width - 1, Math.Clamp((int)Math.Round(v * (source.Height - 1)), 0, source.Height - 1));
                double horizontalWeight = 0.5 + (Math.Min(u, 1.0 - u) - Math.Min(v, 1.0 - v));
                horizontalWeight = Math.Clamp(horizontalWeight, 0.15, 0.85);
                double verticalWeight = 1.0 - horizontalWeight;
                int noise = DeterministicNoise(x, y);
                byte r = QuantizeColor((long)((horizontalWeight * Lerp(left.R, right.R, u)) + (verticalWeight * Lerp(top.R, bottom.R, v)) + noise));
                byte g = QuantizeColor((long)((horizontalWeight * Lerp(left.G, right.G, u)) + (verticalWeight * Lerp(top.G, bottom.G, v)) + noise));
                byte b = QuantizeColor((long)((horizontalWeight * Lerp(left.B, right.B, u)) + (verticalWeight * Lerp(top.B, bottom.B, v)) + noise));
                small.SetPixel(x, y, Color.FromArgb(r, g, b));
            }
        }

        Bitmap result = new(source.Width, source.Height, PixelFormat.Format32bppArgb);
        result.SetResolution(96.0f, 96.0f);
        using Graphics resultGraphics = Graphics.FromImage(result);
        resultGraphics.InterpolationMode = InterpolationMode.NearestNeighbor;
        resultGraphics.PixelOffsetMode = PixelOffsetMode.Half;
        resultGraphics.DrawImage(small, new Rectangle(0, 0, result.Width, result.Height));
        return result;
    }

    private static double Lerp(int a, int b, double amount) => a + ((b - a) * amount);

    private static int DeterministicNoise(int x, int y)
    {
        unchecked
        {
            int hash = (x * 73856093) ^ (y * 19349663) ^ 0x5f3759df;
            hash ^= hash >> 13;
            hash *= 1274126177;
            return ((hash & 0xff) % 31) - 15;
        }
    }

    private static GraphicsPath BuildBrushPath(IReadOnlyList<System.Drawing.Point> points, int radius)
    {
        GraphicsPath path = new();
        if (points.Count == 0) return path;
        List<System.Drawing.Point> segment = new();
        foreach (System.Drawing.Point point in points)
        {
            if (IsPathBreak(point))
            {
                AddBrushSegment(path, segment, radius);
                segment.Clear();
                continue;
            }
            segment.Add(point);
        }
        AddBrushSegment(path, segment, radius);
        return path;
    }

    private static void AddBrushSegment(GraphicsPath path, IReadOnlyList<System.Drawing.Point> segment, int radius)
    {
        foreach (System.Drawing.Point point in segment)
        {
            path.AddEllipse(point.X - radius, point.Y - radius, radius * 2, radius * 2);
        }
        if (segment.Count > 1)
        {
            using GraphicsPath linePath = new();
            linePath.AddLines(segment.ToArray());
            using Pen pen = new(Color.Black, radius * 2) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            using GraphicsPath widened = (GraphicsPath)linePath.Clone();
            widened.Widen(pen);
            path.AddPath(widened, false);
        }
    }

    private static IEnumerable<System.Drawing.Point> InterpolatePath(IReadOnlyList<System.Drawing.Point> points, int maxStep)
    {
        if (points.Count == 0) yield break;
        maxStep = Math.Max(1, maxStep);
        System.Drawing.Point? previousPoint = null;
        foreach (System.Drawing.Point point in points)
        {
            if (IsPathBreak(point))
            {
                previousPoint = null;
                continue;
            }
            if (previousPoint is null)
            {
                previousPoint = point;
                yield return point;
                continue;
            }
            System.Drawing.Point previous = previousPoint.Value;
            int dx = point.X - previous.X;
            int dy = point.Y - previous.Y;
            int steps = Math.Max(1, (int)Math.Ceiling(Math.Sqrt(dx * dx + dy * dy) / maxStep));
            for (int step = 1; step <= steps; step++)
            {
                int x = previous.X + (int)Math.Round(dx * (step / (double)steps));
                int y = previous.Y + (int)Math.Round(dy * (step / (double)steps));
                yield return new System.Drawing.Point(x, y);
            }
            previousPoint = point;
        }
    }

    private static bool IsPathBreak(System.Drawing.Point point) => point.X == int.MinValue && point.Y == int.MinValue;

    private static void RunSta(Action action)
    {
        Exception? failure = null;
        Thread thread = new(() =>
        {
            try
            {
                action();
            }
            catch (Exception ex)
            {
                failure = ex;
            }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();
        if (failure is not null) throw failure;
    }

    private static Bitmap CreateBitmapFromBgra(uint width, uint height, byte[] bgra)
    {
        Bitmap bitmap = new((int)width, (int)height, PixelFormat.Format32bppArgb);
        bitmap.SetResolution(96.0f, 96.0f);
        BitmapData data = bitmap.LockBits(new System.Drawing.Rectangle(0, 0, bitmap.Width, bitmap.Height),
            ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            int rowBytes = checked((int)width * 4);
            for (int y = 0; y < height; y++)
            {
                Marshal.Copy(bgra, y * rowBytes, data.Scan0 + y * data.Stride, rowBytes);
            }
        }
        finally
        {
            bitmap.UnlockBits(data);
        }
        return bitmap;
    }

    private static ReadbackResult CropResult(ReadbackResult source, System.Drawing.Rectangle region)
    {
        int sourceWidth = (int)source.Width;
        int rowBytes = region.Width * 4;
        byte[] output = new byte[checked(rowBytes * region.Height)];
        for (int y = 0; y < region.Height; y++)
        {
            int sourceOffset = ((region.Y + y) * sourceWidth + region.X) * 4;
            Buffer.BlockCopy(source.Bgra, sourceOffset, output, y * rowBytes, rowBytes);
        }
        return source with { Width = (uint)region.Width, Height = (uint)region.Height, Bgra = output };
    }

    private sealed class RegionSelectionForm : Form
    {
        private const int EscapeHotkeyId = 6201;
        private Bitmap preview;
        private readonly System.Drawing.Rectangle targetBounds;

    public enum EditMode
    {
        None,
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum EditOperationType
    {
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum AdjustmentPreset
    {
        Low,
        Balanced,
        High
    }

    private enum ButtonIcon
    {
        None,
        Marker,
        Ellipse,
        Pen,
        Color,
        Mosaic,
        Undo,
        Redo,
        Reset,
        Save,
        Cancel,
        Done,
        Low,
        Balanced,
        High
    }

    private enum ToolbarAction
    {
        Cancel,
        ToolMarker,
        ToolEllipse,
        ToolPen,
        ToolMosaic,
        Color,
        Undo,
        Redo,
        Reset,
        PresetLow,
        PresetBalanced,
        PresetHigh,
        Save,
        Copy
    }

    private readonly record struct ToolbarItem(
        ToolbarAction Action,
        ButtonIcon Icon,
        System.Drawing.Rectangle Rect,
        string Tooltip);

    private static readonly Color[] AnnotationColors =
    {
        Color.FromArgb(255, 59, 48),
        Color.FromArgb(255, 204, 0),
        Color.FromArgb(52, 199, 89),
        Color.FromArgb(64, 156, 255),
        Color.FromArgb(191, 90, 242),
        Color.White
    };

    public enum SelectionCommitAction
    {
        Copy,
        Save
    }

    public sealed record EditOperation(
        EditOperationType Type,
        System.Drawing.Rectangle Rect,
        Color Color,
        IReadOnlyList<System.Drawing.Point>? Points = null,
        string Text = "",
        int StrokeWidth = 2);

    public readonly record struct RegionSelectionResult(
        System.Drawing.Rectangle Region,
        SelectionCommitAction Action,
        AdjustmentPreset Preset,
        IReadOnlyList<EditOperation> Operations);

    private sealed class PillButton : Button
    {
        private static readonly Color ToolbarBackColor = Color.FromArgb(28, 31, 34);
        private bool hot;
        private bool pressed;

        public bool Primary { get; set; }

        public bool Selected { get; set; }

        public ButtonIcon Icon { get; set; }

        public PillButton()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            FlatStyle = FlatStyle.Flat;
            FlatAppearance.BorderSize = 0;
            UseVisualStyleBackColor = false;
            BackColor = ToolbarBackColor;
            ForeColor = Color.White;
            Cursor = Cursors.Hand;
        }

        protected override void OnMouseEnter(EventArgs e)
        {
            hot = true;
            Invalidate();
            base.OnMouseEnter(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            hot = false;
            pressed = false;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                pressed = true;
                Invalidate();
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            pressed = false;
            Invalidate();
            base.OnMouseUp(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? ToolbarBackColor);

            Rectangle rect = new(1, 1, Width - 3, Height - 3);
            Rectangle iconRect = Icon != ButtonIcon.None
                ? string.IsNullOrEmpty(Text)
                    ? new Rectangle((Width - 26) / 2, (Height - 26) / 2, 26, 26)
                    : new Rectangle(rect.Left + 12, (Height - 18) / 2, 18, 18)
                : rect;
            if (hot || pressed)
            {
                int size = Math.Min(38, Math.Min(rect.Width, rect.Height));
                int centerX = iconRect.Left + iconRect.Width / 2;
                int centerY = iconRect.Top + iconRect.Height / 2;
                Rectangle hoverRect = new(centerX - size / 2, centerY - size / 2, size, size);
                using GraphicsPath hoverPath = RoundedRect(hoverRect, size / 2);
                using SolidBrush hoverBrush = new(Color.FromArgb(pressed ? 58 : 36, 255, 255, 255));
                e.Graphics.FillPath(hoverBrush, hoverPath);
            }

            Color iconColor = Icon switch
            {
                ButtonIcon.Cancel => Color.FromArgb(255, 92, 92),
                ButtonIcon.Done => Color.FromArgb(68, 214, 111),
                ButtonIcon.Color => ForeColor,
                _ when Primary || Selected => Color.FromArgb(64, 178, 255),
                _ => Color.FromArgb(225, 229, 232)
            };

            if (Icon != ButtonIcon.None)
            {
                DrawIcon(e.Graphics, iconRect, Icon, iconColor);
            }

            if (!string.IsNullOrEmpty(Text))
            {
                Rectangle textRect = Icon == ButtonIcon.None ? rect : new Rectangle(rect.Left + 34, rect.Top, rect.Width - 40, rect.Height);
                TextRenderer.DrawText(e.Graphics, Text, Font, textRect, iconColor,
                    TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
            }
        }

        public static void DrawIcon(Graphics graphics, Rectangle rect, ButtonIcon icon, Color color)
        {
            string glyph = icon switch
            {
                ButtonIcon.Marker => string.Empty,
                ButtonIcon.Ellipse => "\uEA3A",
                ButtonIcon.Pen => "\uED63",
                ButtonIcon.Color => "\uE790",
                ButtonIcon.Mosaic => string.Empty,
                ButtonIcon.Undo => "\uE7A7",
                ButtonIcon.Redo => "\uE7A6",
                ButtonIcon.Reset => "\uE72C",
                ButtonIcon.Save => "\uE74E",
                ButtonIcon.Cancel => string.Empty,
                ButtonIcon.Done => string.Empty,
                ButtonIcon.Low => string.Empty,
                ButtonIcon.Balanced => string.Empty,
                ButtonIcon.High => string.Empty,
                _ => string.Empty
            };
            if (!string.IsNullOrEmpty(glyph))
            {
                DrawFluentGlyph(graphics, rect, glyph, color);
                if (icon is ButtonIcon.Low or ButtonIcon.Balanced or ButtonIcon.High)
                {
                    int level = icon == ButtonIcon.Low ? 1 : icon == ButtonIcon.Balanced ? 2 : 3;
                    using SolidBrush levelBrush = new(color);
                    using Pen levelPen = new(color, 1.4f);
                    for (int i = 0; i < 3; i++)
                    {
                        Rectangle dot = new(rect.Left + 5 + i * 7, rect.Bottom - 2, 3, 3);
                        if (i < level) graphics.FillEllipse(levelBrush, dot);
                        else graphics.DrawEllipse(levelPen, dot);
                    }
                }
                return;
            }

            using Pen pen = new(color, 2.1f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            using SolidBrush brush = new(color);
            int cx = rect.Left + rect.Width / 2;
            int cy = rect.Top + rect.Height / 2;
            switch (icon)
            {
                case ButtonIcon.Marker:
                    graphics.DrawLine(pen, rect.Left + 4, rect.Top + 6, rect.Right - 4, rect.Top + 6);
                    graphics.DrawLine(pen, rect.Right - 4, rect.Top + 6, rect.Right - 4, rect.Bottom - 6);
                    graphics.DrawLine(pen, rect.Right - 4, rect.Bottom - 6, rect.Left + 4, rect.Bottom - 6);
                    graphics.DrawLine(pen, rect.Left + 4, rect.Bottom - 6, rect.Left + 4, rect.Top + 6);
                    break;
                case ButtonIcon.Ellipse:
                    graphics.DrawEllipse(pen, rect.Left + 4, rect.Top + 5, rect.Width - 8, rect.Height - 10);
                    break;
                case ButtonIcon.Pen:
                    graphics.DrawLine(pen, rect.Left + 6, rect.Bottom - 5, rect.Right - 5, rect.Top + 6);
                    graphics.DrawLine(pen, rect.Right - 8, rect.Top + 5, rect.Right - 4, rect.Top + 9);
                    graphics.DrawLine(pen, rect.Left + 5, rect.Bottom - 5, rect.Left + 9, rect.Bottom - 4);
                    break;
                case ButtonIcon.Color:
                    using (Pen ring = new(Color.FromArgb(230, 225, 229, 232), 1.5f))
                    {
                        graphics.DrawEllipse(ring, rect.Left + 4, rect.Top + 4, rect.Width - 8, rect.Height - 8);
                    }
                    graphics.FillEllipse(brush, rect.Left + 7, rect.Top + 7, rect.Width - 14, rect.Height - 14);
                    using (SolidBrush shine = new(Color.FromArgb(210, 255, 255, 255)))
                    {
                        graphics.FillEllipse(shine, rect.Left + 10, rect.Top + 9, 4, 4);
                    }
                    break;
                case ButtonIcon.Mosaic:
                    using (SolidBrush dimBrush = new(Color.FromArgb(115, color)))
                    {
                        int cell = 5;
                        int gap = 2;
                        int left = cx - (cell * 3 + gap * 2) / 2;
                        int top = cy - (cell * 3 + gap * 2) / 2;
                        for (int y = 0; y < 3; y++)
                        {
                            for (int x = 0; x < 3; x++)
                            {
                                Rectangle cellRect = new(left + x * (cell + gap), top + y * (cell + gap), cell, cell);
                                if (((x + y) & 1) == 0) graphics.FillRectangle(brush, cellRect);
                                else graphics.FillRectangle(dimBrush, cellRect);
                            }
                        }
                    }
                    break;
                case ButtonIcon.Undo:
                    graphics.DrawArc(pen, rect.Left + 4, rect.Top + 4, 13, 13, 200, 230);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Left + 5, rect.Top + 10), new System.Drawing.Point(rect.Left + 1, rect.Top + 10), new System.Drawing.Point(rect.Left + 4, rect.Top + 6) });
                    break;
                case ButtonIcon.Reset:
                    graphics.DrawArc(pen, rect.Left + 4, rect.Top + 4, 13, 13, 30, 300);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Right - 4, rect.Top + 7), new System.Drawing.Point(rect.Right - 2, rect.Top + 3), new System.Drawing.Point(rect.Right - 7, rect.Top + 4) });
                    break;
                case ButtonIcon.Save:
                    graphics.DrawRectangle(pen, rect.Left + 4, rect.Top + 3, rect.Width - 8, rect.Height - 6);
                    graphics.DrawLine(pen, cx, rect.Top + 6, cx, rect.Bottom - 6);
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(cx - 4, rect.Bottom - 10), new System.Drawing.Point(cx, rect.Bottom - 6), new System.Drawing.Point(cx + 4, rect.Bottom - 10) });
                    break;
                case ButtonIcon.Cancel:
                    graphics.DrawLine(pen, rect.Left + 5, rect.Top + 5, rect.Right - 5, rect.Bottom - 5);
                    graphics.DrawLine(pen, rect.Right - 5, rect.Top + 5, rect.Left + 5, rect.Bottom - 5);
                    break;
                case ButtonIcon.Done:
                    graphics.DrawLines(pen, new[] { new System.Drawing.Point(rect.Left + 3, cy), new System.Drawing.Point(cx - 2, rect.Bottom - 5), new System.Drawing.Point(rect.Right - 3, rect.Top + 5) });
                    break;
                case ButtonIcon.Low:
                case ButtonIcon.Balanced:
                case ButtonIcon.High:
                    int level = icon == ButtonIcon.Low ? 1 : icon == ButtonIcon.Balanced ? 2 : 3;
                    int core = 8;
                    graphics.FillEllipse(brush, cx - core / 2, cy - core / 2, core, core);
                    int rays = level == 1 ? 4 : level == 2 ? 6 : 8;
                    int inner = 9;
                    int outer = level == 1 ? 11 : level == 2 ? 12 : 13;
                    for (int ray = 0; ray < rays; ray++)
                    {
                        double angle = Math.PI * 2 * ray / rays - Math.PI / 2;
                        int x1 = cx + (int)Math.Round(Math.Cos(angle) * inner);
                        int y1 = cy + (int)Math.Round(Math.Sin(angle) * inner);
                        int x2 = cx + (int)Math.Round(Math.Cos(angle) * outer);
                        int y2 = cy + (int)Math.Round(Math.Sin(angle) * outer);
                        graphics.DrawLine(pen, x1, y1, x2, y2);
                    }
                    break;
            }
        }

        private static void DrawFluentGlyph(Graphics graphics, Rectangle rect, string glyph, Color color)
        {
            using Font font = CreateIconFont(22.0f);
            TextRenderer.DrawText(graphics, glyph, font, rect, color,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }

        private static Font CreateIconFont(float size)
        {
            try
            {
                return new Font("Segoe Fluent Icons", size, FontStyle.Regular, GraphicsUnit.Pixel);
            }
            catch
            {
                return new Font("Segoe MDL2 Assets", size, FontStyle.Regular, GraphicsUnit.Pixel);
            }
        }
    }

    private sealed class ModernStatusLabel : Control
    {
        public ModernStatusLabel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            ForeColor = Color.FromArgb(231, 235, 238);
        }

        protected override void OnTextChanged(EventArgs e)
        {
            Invalidate();
            base.OnTextChanged(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.FromArgb(15, 17, 18));

            Rectangle rect = new(0, 2, Width - 1, Height - 4);
            using GraphicsPath path = RoundedRect(rect, 10);
            using SolidBrush fill = new(Color.FromArgb(31, 36, 39));
            using Pen border = new(Color.FromArgb(52, 59, 63), 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);

            using SolidBrush dot = new(Color.FromArgb(0, 103, 192));
            e.Graphics.FillEllipse(dot, 14, (Height - 7) / 2, 7, 7);
            Rectangle textRect = new(30, 0, Math.Max(0, Width - 42), Height);
            TextRenderer.DrawText(e.Graphics, Text, Font, textRect, ForeColor,
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
        }
    }

    private sealed class ToolbarTooltipLabel : Control
    {
        public ToolbarTooltipLabel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            BackColor = Color.FromArgb(31, 33, 36);
            ForeColor = Color.White;
            Font = new Font("Microsoft YaHei UI", 9.0f, FontStyle.Regular, GraphicsUnit.Point);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            Rectangle rect = new(0, 0, Width - 1, Height - 1);
            using GraphicsPath path = RoundedRect(rect, 8);
            using SolidBrush fill = new(Color.FromArgb(242, 32, 34, 37));
            e.Graphics.FillPath(fill, path);
            TextRenderer.DrawText(e.Graphics, Text, Font, rect, ForeColor,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
        }
    }

    private sealed class FloatingToolbarPanel : Panel
    {
        private static readonly Color ToolbarBackColor = Color.FromArgb(28, 31, 34);
        private static readonly Color ToolbarBorderColor = Color.FromArgb(95, 255, 255, 255);

        public FloatingToolbarPanel()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.SupportsTransparentBackColor, true);
            BackColor = Color.FromArgb(28, 31, 34);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.Black);

            Rectangle shadowRect = new(2, 4, Width - 5, Height - 7);
            using GraphicsPath shadowPath = RoundedRect(shadowRect, Math.Max(1, shadowRect.Height / 2));
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            e.Graphics.FillPath(shadow, shadowPath);

            Rectangle rect = new(0, 0, Width - 2, Height - 2);
            using GraphicsPath path = RoundedRect(rect, Math.Max(1, rect.Height / 2));
            using SolidBrush fill = new(Color.FromArgb(178, ToolbarBackColor));
            using Pen border = new(ToolbarBorderColor, 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);
        }
    }

    private sealed class PreviewToolbarControl : Control
    {
        private readonly List<PreviewToolbarItem> items = new();
        private readonly List<OptionPopoverItem> optionItems = new();
        private int hoveredIndex = -1;
        private int pressedIndex = -1;
        private int hoveredOptionIndex = -1;
        private int pressedOptionIndex = -1;
        private Rectangle optionsBounds;
        private bool optionsVisible;

        public event EventHandler? OptionsVisibilityChanged;

        public PreviewToolbarControl()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.SupportsTransparentBackColor, true);
            BackColor = Color.Transparent;
            Cursor = Cursors.Default;
            Height = 62;
            Width = 820;
        }

        public bool OptionsVisible => optionsVisible;

        public int PreferredToolbarHeight => optionsVisible ? 118 : 62;

        public void Add(ToolbarAction action, ButtonIcon icon, Action<Rectangle> click, Func<bool>? selected = null, Func<Color>? color = null)
        {
            int x = items.Count == 0 ? 14 : items[^1].Rect.Right + 8;
            if (action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
            {
                x += 13;
            }
            Rectangle rect = new(x, 10, 46, 42);
            items.Add(new PreviewToolbarItem(action, icon, rect, click, selected, color));
        }

        public void Add(ToolbarAction action, ButtonIcon icon, Action click, Func<bool>? selected = null, Func<Color>? color = null)
        {
            Add(action, icon, _ => click(), selected, color);
        }

        public void ShowOptions(Rectangle anchor, IReadOnlyList<OptionPopoverItem> options)
        {
            optionItems.Clear();
            int width = 24 + options.Count * 42 + Math.Max(0, options.Count - 1) * 8;
            int left = Math.Clamp(anchor.Left + (anchor.Width - width) / 2, 0, Math.Max(0, Width - width));
            optionsBounds = new Rectangle(left, 0, width, 48);
            for (int i = 0; i < options.Count; i++)
            {
                OptionPopoverItem item = options[i];
                Rectangle rect = new(optionsBounds.Left + 12 + i * 50, optionsBounds.Top + 7, 42, 34);
                optionItems.Add(new OptionPopoverItem(rect, item.Text, item.Color, item.Selected, item.Click));
            }
            optionsVisible = optionItems.Count > 0;
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Height = PreferredToolbarHeight;
            OptionsVisibilityChanged?.Invoke(this, EventArgs.Empty);
            Invalidate();
        }

        public void HideOptions()
        {
            if (!optionsVisible && optionItems.Count == 0) return;
            optionsVisible = false;
            optionItems.Clear();
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Height = PreferredToolbarHeight;
            OptionsVisibilityChanged?.Invoke(this, EventArgs.Empty);
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? Color.Black);

            if (optionsVisible)
            {
                DrawOptions(e.Graphics);
            }

            int barTop = BarTop();
            Rectangle shadowRect = new(2, barTop + 4, Width - 5, 55);
            using GraphicsPath shadowPath = RoundedRect(shadowRect, Math.Max(1, shadowRect.Height / 2));
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            e.Graphics.FillPath(shadow, shadowPath);

            Rectangle pill = new(0, barTop, Width - 2, 60);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using SolidBrush fill = new(Color.FromArgb(178, 27, 30, 33));
            using Pen border = new(Color.FromArgb(95, 255, 255, 255), 1.0f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);

            foreach (PreviewToolbarItem item in items)
            {
                Rectangle itemRect = OffsetBarRect(item.Rect);
                if (item.Action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
                {
                    DrawSeparator(e.Graphics, itemRect.Left - 11);
                }

                int index = items.IndexOf(item);
                bool selected = item.Selected?.Invoke() == true;
                bool hot = index == hoveredIndex || index == pressedIndex;
                Color iconColor = item.Color?.Invoke() ?? ToolbarIconColor(item.Action, selected);
                if (hot || selected)
                {
                    int size = hot ? 38 : 34;
                    int cx = itemRect.Left + itemRect.Width / 2;
                    int cy = itemRect.Top + itemRect.Height / 2;
                    using GraphicsPath hover = RoundedRect(new Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 44 : 28, selected ? 64 : 255, selected ? 178 : 255, selected ? 255 : 255));
                    e.Graphics.FillPath(hoverBrush, hover);
                }

                Rectangle iconRect = new(itemRect.Left + 10, itemRect.Top + 8, 26, 26);
                PillButton.DrawIcon(e.Graphics, iconRect, item.Icon, iconColor);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            int optionHit = HitTestOptions(e.Location);
            int hit = optionHit >= 0 ? -1 : HitTest(e.Location);
            if (optionHit != hoveredOptionIndex || hit != hoveredIndex)
            {
                hoveredOptionIndex = optionHit;
                hoveredIndex = hit;
                Cursor = hit >= 0 || optionHit >= 0 ? Cursors.Hand : Cursors.Default;
                Invalidate();
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            hoveredIndex = -1;
            pressedIndex = -1;
            hoveredOptionIndex = -1;
            pressedOptionIndex = -1;
            Cursor = Cursors.Default;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                pressedOptionIndex = HitTestOptions(e.Location);
                pressedIndex = pressedOptionIndex >= 0 ? -1 : HitTest(e.Location);
                if (pressedIndex < 0 && pressedOptionIndex < 0)
                {
                    HideOptions();
                }
                Invalidate();
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            int pressed = pressedIndex;
            int pressedOption = pressedOptionIndex;
            pressedIndex = -1;
            pressedOptionIndex = -1;
            if (e.Button == MouseButtons.Left && pressedOption >= 0 && pressedOption == HitTestOptions(e.Location))
            {
                Action click = optionItems[pressedOption].Click;
                HideOptions();
                click();
                Invalidate();
                base.OnMouseUp(e);
                return;
            }
            if (e.Button == MouseButtons.Left && pressed >= 0 && pressed == HitTest(e.Location))
            {
                items[pressed].Click(items[pressed].Rect);
            }
            Invalidate();
            base.OnMouseUp(e);
        }

        private int HitTest(System.Drawing.Point point)
        {
            for (int i = 0; i < items.Count; i++)
            {
                if (OffsetBarRect(items[i].Rect).Contains(point)) return i;
            }
            return -1;
        }

        private int HitTestOptions(System.Drawing.Point point)
        {
            if (!optionsVisible || !optionsBounds.Contains(point)) return -1;
            for (int i = 0; i < optionItems.Count; i++)
            {
                if (optionItems[i].Rect.Contains(point)) return i;
            }
            return -1;
        }

        private int BarTop() => optionsVisible ? 56 : 0;

        private Rectangle OffsetBarRect(Rectangle rect)
        {
            return new Rectangle(rect.Left, rect.Top + BarTop(), rect.Width, rect.Height);
        }

        private void DrawSeparator(Graphics graphics, int x)
        {
            using Pen pen = new(Color.FromArgb(70, 255, 255, 255), 1.0f);
            int barTop = BarTop();
            graphics.DrawLine(pen, x, barTop + 19, x, barTop + 43);
        }

        private void DrawOptions(Graphics graphics)
        {
            using GraphicsPath shadowPath = RoundedRect(new Rectangle(optionsBounds.Left + 2, optionsBounds.Top + 4, optionsBounds.Width - 4, optionsBounds.Height - 4), (optionsBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(70, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            Rectangle pill = new(optionsBounds.Left, optionsBounds.Top, optionsBounds.Width - 1, optionsBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using SolidBrush fill = new(Color.FromArgb(184, 27, 30, 33));
            using Pen border = new(Color.FromArgb(96, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            for (int i = 0; i < optionItems.Count; i++)
            {
                OptionPopoverItem item = optionItems[i];
                bool hot = i == hoveredOptionIndex || i == pressedOptionIndex;
                if (hot || item.Selected)
                {
                    int size = item.Selected ? 34 : 32;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hoverPath = RoundedRect(new Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 50 : 34, item.Selected ? 64 : 255, item.Selected ? 178 : 255, item.Selected ? 255 : 255));
                    graphics.FillPath(hoverBrush, hoverPath);
                }

                if (item.Color.HasValue)
                {
                    DrawColorOption(graphics, item.Rect, item.Color.Value, item.Selected);
                }
                else
                {
                    Color textColor = item.Selected ? Color.FromArgb(64, 178, 255) : Color.FromArgb(225, 229, 232);
                    TextRenderer.DrawText(graphics, item.Text, Font, item.Rect, textColor,
                        TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                }
            }
        }

        private static Color ToolbarIconColor(ToolbarAction action, bool selected)
        {
            if (action == ToolbarAction.Cancel) return Color.FromArgb(255, 92, 92);
            if (action == ToolbarAction.Copy) return Color.FromArgb(68, 214, 111);
            if (selected) return Color.FromArgb(64, 178, 255);
            return Color.FromArgb(225, 229, 232);
        }
    }

    private sealed record PreviewToolbarItem(
        ToolbarAction Action,
        ButtonIcon Icon,
        Rectangle Rect,
        Action<Rectangle> Click,
        Func<bool>? Selected,
        Func<Color>? Color);

    private sealed class OptionPopoverItem
    {
        public OptionPopoverItem(System.Drawing.Rectangle rect, string text, Color? color, bool selected, Action click)
        {
            Rect = rect;
            Text = text;
            Color = color;
            Selected = selected;
            Click = click;
        }

        public System.Drawing.Rectangle Rect { get; }

        public string Text { get; }

        public Color? Color { get; }

        public bool Selected { get; }

        public Action Click { get; }
    }

    private sealed class PreviewTitleBarControl : Control
    {
        private bool closeHot;
        private bool closePressed;

        public event EventHandler? CloseClicked;

        public PreviewTitleBarControl()
        {
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            BackColor = Color.FromArgb(10, 12, 13);
            ForeColor = Color.FromArgb(235, 239, 242);
            Height = 52;
            Dock = DockStyle.Top;
        }

        public Rectangle CloseButtonBounds => new(Width - 58, 8, 42, 36);

        public bool IsCloseButtonPoint(System.Drawing.Point point) => CloseButtonBounds.Contains(point);

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(BackColor);
            using SolidBrush titleBrush = new(ForeColor);
            TextRenderer.DrawText(e.Graphics, Text, Font, new Rectangle(14, 0, Math.Max(1, Width - 86), Height),
                ForeColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);

            Rectangle closeRect = CloseButtonBounds;
            Color iconColor = closeHot || closePressed ? Color.White : Color.FromArgb(224, 229, 232);
            if (closeHot || closePressed)
            {
                Color closeColor = closePressed
                    ? Color.FromArgb(198, 35, 48)
                    : Color.FromArgb(228, 43, 58);
                using GraphicsPath closePath = RoundedRect(closeRect, 9);
                using SolidBrush closeBrush = new(closeColor);
                e.Graphics.FillPath(closeBrush, closePath);
            }

            using Pen pen = new(iconColor, 2.0f) { StartCap = LineCap.Round, EndCap = LineCap.Round };
            int cx = closeRect.Left + closeRect.Width / 2;
            int cy = closeRect.Top + closeRect.Height / 2;
            e.Graphics.DrawLine(pen, cx - 6, cy - 6, cx + 6, cy + 6);
            e.Graphics.DrawLine(pen, cx + 6, cy - 6, cx - 6, cy + 6);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            bool hot = CloseButtonBounds.Contains(e.Location);
            if (hot != closeHot)
            {
                closeHot = hot;
                Cursor = closeHot ? Cursors.Hand : Cursors.Default;
                Invalidate(CloseButtonBounds);
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            closeHot = false;
            closePressed = false;
            Cursor = Cursors.Default;
            Invalidate(CloseButtonBounds);
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left && CloseButtonBounds.Contains(e.Location))
            {
                closePressed = true;
                Invalidate(CloseButtonBounds);
            }
            base.OnMouseDown(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            bool shouldClose = closePressed && e.Button == MouseButtons.Left && CloseButtonBounds.Contains(e.Location);
            closePressed = false;
            Invalidate(CloseButtonBounds);
            if (shouldClose)
            {
                CloseClicked?.Invoke(this, EventArgs.Empty);
            }
            base.OnMouseUp(e);
        }
    }

    private static GraphicsPath RoundedRect(Rectangle rect, int radius)
    {
        GraphicsPath path = new();
        int arc = Math.Max(1, radius * 2);
        path.AddArc(rect.Left, rect.Top, arc, arc, 180, 90);
        path.AddArc(rect.Right - arc, rect.Top, arc, arc, 270, 90);
        path.AddArc(rect.Right - arc, rect.Bottom - arc, arc, arc, 0, 90);
        path.AddArc(rect.Left, rect.Bottom - arc, arc, arc, 90, 90);
        path.CloseFigure();
        return path;
    }

    public sealed class PreviewEditorForm : Form
    {
        private enum WindowResizeHit
        {
            None,
            Left,
            Right,
            Top,
            Bottom,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        }

        private readonly Bitmap source;
        private readonly string defaultOutputPath;
        private readonly bool copyOnShown;
        private readonly PictureBox preview = new();
        private readonly PreviewTitleBarControl titleBar = new();
        private readonly PreviewToolbarControl toolbar = new();
        private readonly Panel actionArea = new();
        private readonly Panel content = new();
        private readonly List<EditOperation> operations = new();
        private readonly List<EditOperation> redoOperations = new();
        private Bitmap current;
        private EditMode mode = EditMode.None;
        private AdjustmentPreset adjustmentPreset = AdjustmentPreset.Balanced;
        private int annotationColorIndex;
        private int shapeStrokeWidth = 4;
        private int penStrokeWidth = 6;
        private int mosaicBrushSize = 28;
        private bool dragging;
        private bool movingWindow;
        private bool resizingWindow;
        private WindowResizeHit activeResizeHit = WindowResizeHit.None;
        private Control? chromeCaptureControl;
        private System.Drawing.Point windowDragStartScreen;
        private System.Drawing.Rectangle windowDragStartBounds;
        private System.Drawing.Point dragStart;
        private System.Drawing.Point dragCurrent;
        private readonly List<System.Drawing.Point> currentPenPoints = new();

        public PreviewEditorForm(Bitmap source, string defaultOutputPath, bool copyOnShown = true)
        {
            this.source = CloneAsArgb32(source);
            this.current = new Bitmap(this.source);
            this.defaultOutputPath = defaultOutputPath;
            this.copyOnShown = copyOnShown;
            Text = CaptureText.Get(CaptureString.PreviewTitle);
            StartPosition = FormStartPosition.CenterScreen;
            Size = new System.Drawing.Size(1180, 760);
            MinimumSize = new System.Drawing.Size(900, 520);
            FormBorderStyle = FormBorderStyle.None;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            Padding = new Padding(0);
            BackColor = Color.FromArgb(10, 12, 13);
            ForeColor = Color.White;
            Font = new Font(CaptureText.FontFamily, 9.0f, FontStyle.Regular, GraphicsUnit.Point);

            titleBar.Text = Text;
            titleBar.Font = Font;
            titleBar.CloseClicked += (_, _) => Close();
            AttachWindowChromeMouseHandlers(titleBar);

            preview.Dock = DockStyle.Fill;
            preview.SizeMode = PictureBoxSizeMode.Zoom;
            preview.BackColor = Color.FromArgb(10, 12, 13);
            preview.Image = current;
            preview.MouseDown += PreviewMouseDown;
            preview.MouseMove += PreviewMouseMove;
            preview.MouseUp += PreviewMouseUp;
            preview.Paint += PreviewPaint;
            AttachWindowChromeMouseHandlers(preview);

            toolbar.Font = Font;
            toolbar.Add(ToolbarAction.Cancel, ButtonIcon.Cancel, Close);
            toolbar.Add(ToolbarAction.ToolMarker, ButtonIcon.Marker, anchor => SelectToolbarTool(EditMode.Marker, anchor), () => mode == EditMode.Marker);
            toolbar.Add(ToolbarAction.ToolEllipse, ButtonIcon.Ellipse, anchor => SelectToolbarTool(EditMode.Ellipse, anchor), () => mode == EditMode.Ellipse);
            toolbar.Add(ToolbarAction.ToolPen, ButtonIcon.Pen, anchor => SelectToolbarTool(EditMode.Pen, anchor), () => mode == EditMode.Pen);
            toolbar.Add(ToolbarAction.ToolMosaic, ButtonIcon.Mosaic, anchor => SelectToolbarTool(EditMode.Mosaic, anchor), () => mode == EditMode.Mosaic);
            toolbar.Add(ToolbarAction.Color, ButtonIcon.Color, ShowColorOptions, color: () => AnnotationColors[annotationColorIndex]);
            toolbar.Add(ToolbarAction.Undo, ButtonIcon.Undo, UndoLastEdit);
            toolbar.Add(ToolbarAction.Redo, ButtonIcon.Redo, RedoLastEdit);
            toolbar.Add(ToolbarAction.Reset, ButtonIcon.Reset, ResetEdits);
            toolbar.Add(ToolbarAction.PresetLow, ButtonIcon.Low, () => SetAdjustmentPreset(AdjustmentPreset.Low), () => adjustmentPreset == AdjustmentPreset.Low);
            toolbar.Add(ToolbarAction.PresetBalanced, ButtonIcon.Balanced, () => SetAdjustmentPreset(AdjustmentPreset.Balanced), () => adjustmentPreset == AdjustmentPreset.Balanced);
            toolbar.Add(ToolbarAction.PresetHigh, ButtonIcon.High, () => SetAdjustmentPreset(AdjustmentPreset.High), () => adjustmentPreset == AdjustmentPreset.High);
            toolbar.Add(ToolbarAction.Save, ButtonIcon.Save, SaveToFile);
            toolbar.Add(ToolbarAction.Copy, ButtonIcon.Done, CopyToClipboardAndClose);
            toolbar.OptionsVisibilityChanged += (_, _) => LayoutToolbarOverlay();

            actionArea.Dock = DockStyle.Bottom;
            actionArea.Height = 82;
            actionArea.BackColor = Color.FromArgb(10, 12, 13);
            actionArea.Padding = new Padding(0);
            AttachWindowChromeMouseHandlers(actionArea);

            content.Dock = DockStyle.Fill;
            content.BackColor = Color.FromArgb(10, 12, 13);
            content.Padding = new Padding(0);
            AttachWindowChromeMouseHandlers(content);
            content.Controls.Add(preview);
            content.Controls.Add(actionArea);
            content.Controls.Add(toolbar);
            content.Resize += (_, _) => LayoutToolbarOverlay();
            preview.Resize += (_, _) => LayoutToolbarOverlay();

            Controls.Add(content);
            Controls.Add(titleBar);
            KeyPreview = true;
            LayoutToolbarOverlay();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (copyOnShown)
            {
                BeginInvoke(CopyToClipboard);
            }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            e.Graphics.Clear(BackColor);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Control && e.KeyCode == Keys.Z)
            {
                UndoLastEdit();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.Control && e.KeyCode == Keys.Y)
            {
                RedoLastEdit();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.KeyCode == Keys.Escape) Close();
            base.OnKeyDown(e);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                current.Dispose();
                source.Dispose();
            }
            base.Dispose(disposing);
        }

        private void AttachWindowChromeMouseHandlers(Control control)
        {
            control.MouseDown += WindowChromeMouseDown;
            control.MouseMove += WindowChromeMouseMove;
            control.MouseUp += WindowChromeMouseUp;
            control.MouseLeave += WindowChromeMouseLeave;
        }

        private void WindowChromeMouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left || sender is not Control control) return;
            System.Drawing.Point screenPoint = control.PointToScreen(e.Location);
            System.Drawing.Point clientPoint = PointToClient(screenPoint);
            WindowResizeHit resizeHit = HitTestResize(clientPoint);
            if (resizeHit != WindowResizeHit.None)
            {
                BeginWindowResize(resizeHit, screenPoint, control);
                return;
            }
            if (control == titleBar && !titleBar.IsCloseButtonPoint(e.Location))
            {
                movingWindow = true;
                windowDragStartScreen = screenPoint;
                windowDragStartBounds = Bounds;
                chromeCaptureControl = control;
                control.Capture = true;
            }
        }

        private void WindowChromeMouseMove(object? sender, MouseEventArgs e)
        {
            if (sender is not Control control) return;
            System.Drawing.Point screenPoint = control.PointToScreen(e.Location);
            if (movingWindow)
            {
                int dx = screenPoint.X - windowDragStartScreen.X;
                int dy = screenPoint.Y - windowDragStartScreen.Y;
                Location = new System.Drawing.Point(windowDragStartBounds.Left + dx, windowDragStartBounds.Top + dy);
                return;
            }
            if (resizingWindow)
            {
                ResizeWindow(screenPoint);
                return;
            }

            if (dragging || mode != EditMode.None && control == preview)
            {
                return;
            }
            WindowResizeHit hit = HitTestResize(PointToClient(screenPoint));
            Cursor = CursorForResizeHit(hit);
        }

        private void WindowChromeMouseUp(object? sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;
            movingWindow = false;
            resizingWindow = false;
            activeResizeHit = WindowResizeHit.None;
            if (chromeCaptureControl is not null)
            {
                chromeCaptureControl.Capture = false;
                chromeCaptureControl = null;
            }
            Cursor = Cursors.Default;
        }

        private void WindowChromeMouseLeave(object? sender, EventArgs e)
        {
            if (!movingWindow && !resizingWindow)
            {
                Cursor = Cursors.Default;
            }
        }

        private void BeginWindowResize(WindowResizeHit hit, System.Drawing.Point screenPoint, Control control)
        {
            resizingWindow = true;
            activeResizeHit = hit;
            windowDragStartScreen = screenPoint;
            windowDragStartBounds = Bounds;
            chromeCaptureControl = control;
            control.Capture = true;
        }

        private void ResizeWindow(System.Drawing.Point screenPoint)
        {
            int dx = screenPoint.X - windowDragStartScreen.X;
            int dy = screenPoint.Y - windowDragStartScreen.Y;
            Rectangle next = windowDragStartBounds;
            System.Drawing.Size min = MinimumSize;

            if (activeResizeHit is WindowResizeHit.Left or WindowResizeHit.TopLeft or WindowResizeHit.BottomLeft)
            {
                int newLeft = Math.Min(next.Right - min.Width, windowDragStartBounds.Left + dx);
                next.Width += next.Left - newLeft;
                next.X = newLeft;
            }
            if (activeResizeHit is WindowResizeHit.Right or WindowResizeHit.TopRight or WindowResizeHit.BottomRight)
            {
                next.Width = Math.Max(min.Width, windowDragStartBounds.Width + dx);
            }
            if (activeResizeHit is WindowResizeHit.Top or WindowResizeHit.TopLeft or WindowResizeHit.TopRight)
            {
                int newTop = Math.Min(next.Bottom - min.Height, windowDragStartBounds.Top + dy);
                next.Height += next.Top - newTop;
                next.Y = newTop;
            }
            if (activeResizeHit is WindowResizeHit.Bottom or WindowResizeHit.BottomLeft or WindowResizeHit.BottomRight)
            {
                next.Height = Math.Max(min.Height, windowDragStartBounds.Height + dy);
            }
            Bounds = next;
        }

        private WindowResizeHit HitTestResize(System.Drawing.Point clientPoint)
        {
            int grip = Math.Max(8, (int)Math.Round(10 * DeviceDpi / 96.0));
            bool left = clientPoint.X >= 0 && clientPoint.X < grip;
            bool right = clientPoint.X < ClientSize.Width && clientPoint.X >= ClientSize.Width - grip;
            bool top = clientPoint.Y >= 0 && clientPoint.Y < grip;
            bool bottom = clientPoint.Y < ClientSize.Height && clientPoint.Y >= ClientSize.Height - grip;
            if (top && left) return WindowResizeHit.TopLeft;
            if (top && right) return WindowResizeHit.TopRight;
            if (bottom && left) return WindowResizeHit.BottomLeft;
            if (bottom && right) return WindowResizeHit.BottomRight;
            if (left) return WindowResizeHit.Left;
            if (right) return WindowResizeHit.Right;
            if (top) return WindowResizeHit.Top;
            if (bottom) return WindowResizeHit.Bottom;
            return WindowResizeHit.None;
        }

        private static Cursor CursorForResizeHit(WindowResizeHit hit)
        {
            return hit switch
            {
                WindowResizeHit.Left or WindowResizeHit.Right => Cursors.SizeWE,
                WindowResizeHit.Top or WindowResizeHit.Bottom => Cursors.SizeNS,
                WindowResizeHit.TopLeft or WindowResizeHit.BottomRight => Cursors.SizeNWSE,
                WindowResizeHit.TopRight or WindowResizeHit.BottomLeft => Cursors.SizeNESW,
                _ => Cursors.Default
            };
        }

        private void LayoutToolbarOverlay()
        {
            toolbar.Width = 820;
            toolbar.Height = toolbar.PreferredToolbarHeight;
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            int centerX = content.ClientSize.Width / 2;
            if (imageBounds.Width > 0)
            {
                System.Drawing.Point imageCenter = preview.PointToScreen(new System.Drawing.Point(imageBounds.Left + imageBounds.Width / 2, imageBounds.Bottom));
                centerX = content.PointToClient(imageCenter).X;
            }
            int mainBarTop = content.ClientSize.Height - actionArea.Height + (actionArea.Height - 62) / 2;
            int top = toolbar.OptionsVisible ? mainBarTop - 56 : mainBarTop;
            toolbar.Left = Math.Clamp(centerX - toolbar.Width / 2, 16, Math.Max(16, content.ClientSize.Width - toolbar.Width - 16));
            toolbar.Top = Math.Max(8, top);
            toolbar.BringToFront();
        }

        private void SetStatus(string message)
        {
            _ = message;
        }

        private void UpdateToolButtons()
        {
            toolbar.Invalidate();
        }

        private void SetAdjustmentPreset(AdjustmentPreset preset)
        {
            adjustmentPreset = preset;
            toolbar.Invalidate();
            RebuildCurrent();
        }

        private void SetMode(EditMode nextMode)
        {
            mode = nextMode;
            UpdateToolButtons();
        }

        private void SelectToolbarTool(EditMode nextMode, Rectangle anchor)
        {
            bool alreadyActive = mode == nextMode;
            mode = nextMode;
            toolbar.Invalidate();
            if (alreadyActive)
            {
                ShowToolOptions(nextMode, anchor);
            }
        }

        private void ShowToolOptions(EditMode toolMode, Rectangle anchor)
        {
            int[] values = toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => new[] { 2, 4, 6 },
                EditMode.Pen => new[] { 3, 6, 10 },
                EditMode.Mosaic => new[] { 16, 28, 42 },
                _ => Array.Empty<int>()
            };
            if (values.Length == 0) return;

            List<OptionPopoverItem> options = new();
            foreach (int value in values)
            {
                options.Add(new OptionPopoverItem(
                    Rectangle.Empty,
                    value.ToString(System.Globalization.CultureInfo.InvariantCulture),
                    null,
                    IsToolSizeSelected(toolMode, value),
                    () =>
                    {
                        SetToolSize(toolMode, value);
                        toolbar.Invalidate();
                    }));
            }
            toolbar.ShowOptions(anchor, options);
        }

        private void ShowColorOptions(Rectangle anchor)
        {
            List<OptionPopoverItem> options = new();
            for (int i = 0; i < AnnotationColors.Length; i++)
            {
                int colorIndex = i;
                options.Add(new OptionPopoverItem(
                    Rectangle.Empty,
                    string.Empty,
                    AnnotationColors[colorIndex],
                    annotationColorIndex == colorIndex,
                    () =>
                    {
                        annotationColorIndex = colorIndex;
                        toolbar.Invalidate();
                    }));
            }
            toolbar.ShowOptions(anchor, options);
        }

        private bool IsToolSizeSelected(EditMode toolMode, int value)
        {
            return toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => shapeStrokeWidth == value,
                EditMode.Pen => penStrokeWidth == value,
                EditMode.Mosaic => mosaicBrushSize == value,
                _ => false
            };
        }

        private void SetToolSize(EditMode toolMode, int value)
        {
            if (toolMode is EditMode.Marker or EditMode.Ellipse) shapeStrokeWidth = value;
            if (toolMode == EditMode.Pen) penStrokeWidth = value;
            if (toolMode == EditMode.Mosaic) mosaicBrushSize = value;
        }

        private void CycleAnnotationColor()
        {
            annotationColorIndex = (annotationColorIndex + 1) % AnnotationColors.Length;
            toolbar.Invalidate();
        }

        private void SaveToFile()
        {
            using SaveFileDialog dialog = new()
            {
                Title = CaptureText.Get(CaptureString.SaveDialogTitle),
                Filter = CaptureText.Get(CaptureString.PngFilter),
                FileName = Path.GetFileName(defaultOutputPath),
                InitialDirectory = Path.GetDirectoryName(defaultOutputPath)
            };
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            Directory.CreateDirectory(Path.GetDirectoryName(dialog.FileName)!);
            current.Save(dialog.FileName, ImageFormat.Png);
            SetStatus(CaptureText.Get(CaptureString.SavedStatus));
        }

        private void CopyToClipboard()
        {
            try
            {
                using Bitmap clipboardBitmap = new(current);
                clipboardBitmap.SetResolution(96.0f, 96.0f);
                using MemoryStream pngStream = new();
                clipboardBitmap.Save(pngStream, ImageFormat.Png);
                DataObject data = new();
                data.SetData("PNG", false, new MemoryStream(pngStream.ToArray()));
                data.SetData(DataFormats.Bitmap, true, new Bitmap(clipboardBitmap));
                Clipboard.SetDataObject(data, true, 5, 120);
                SetStatus(CaptureText.Get(CaptureString.CopiedStatus));
            }
            catch (Exception ex)
            {
                SetStatus(CaptureText.Get(CaptureString.CopyFailedPrefix) + ex.Message);
            }
        }

        private void CopyToClipboardAndClose()
        {
            CopyToClipboard();
            Close();
        }

        private void UndoLastEdit()
        {
            if (operations.Count == 0)
            {
                SetStatus(CaptureText.Get(CaptureString.NoUndoStatus));
                return;
            }

            EditOperation operation = operations[^1];
            operations.RemoveAt(operations.Count - 1);
            redoOperations.Add(operation);
            RebuildCurrent();
        }

        private void RedoLastEdit()
        {
            if (redoOperations.Count == 0) return;
            operations.Add(redoOperations[^1]);
            redoOperations.RemoveAt(redoOperations.Count - 1);
            RebuildCurrent();
        }

        private void ResetEdits()
        {
            operations.Clear();
            redoOperations.Clear();
            currentPenPoints.Clear();
            adjustmentPreset = AdjustmentPreset.Balanced;
            annotationColorIndex = 0;
            mode = EditMode.None;
            UpdateToolButtons();
            RebuildCurrent();
        }

        private void RebuildCurrent()
        {
            Bitmap next = BuildAdjustedBitmap();
            foreach (EditOperation operation in operations)
            {
                if (operation.Type == EditOperationType.Mosaic) ApplyMosaic(next, operation.Rect, operation.StrokeWidth);
            }
            using (Graphics graphics = Graphics.FromImage(next))
            {
                graphics.SmoothingMode = SmoothingMode.AntiAlias;
                foreach (EditOperation operation in operations)
                {
                    if (operation.Type == EditOperationType.Mosaic) continue;
                    using Pen pen = new(operation.Color, Math.Max(1, operation.StrokeWidth))
                    {
                        StartCap = LineCap.Round,
                        EndCap = LineCap.Round,
                        LineJoin = LineJoin.Round
                    };
                    if (operation.Type == EditOperationType.Marker) graphics.DrawRectangle(pen, operation.Rect);
                    if (operation.Type == EditOperationType.Ellipse) graphics.DrawEllipse(pen, operation.Rect);
                    if (operation.Type == EditOperationType.Pen && operation.Points is { Count: > 1 })
                    {
                        DrawSegmentedLines(graphics, pen, operation.Points);
                    }
                }
            }

            Bitmap old = current;
            current = next;
            preview.Image = current;
            old.Dispose();
            preview.Invalidate();
        }

        private static Bitmap CloneAsArgb32(Bitmap input)
        {
            Bitmap clone = new(input.Width, input.Height, PixelFormat.Format32bppArgb);
            clone.SetResolution(input.HorizontalResolution, input.VerticalResolution);
            using Graphics graphics = Graphics.FromImage(clone);
            graphics.DrawImageUnscaled(input, 0, 0);
            return clone;
        }

        private unsafe Bitmap BuildAdjustedBitmap()
        {
            (float exposure, float highlightProtect) = adjustmentPreset switch
            {
                AdjustmentPreset.Low => (0.88f, 1.22f),
                AdjustmentPreset.High => (1.12f, 0.82f),
                _ => (1.0f, 1.0f)
            };
            Bitmap output = new(source.Width, source.Height, PixelFormat.Format32bppArgb);
            output.SetResolution(96.0f, 96.0f);

            System.Drawing.Rectangle rect = new(0, 0, source.Width, source.Height);
            BitmapData sourceData = source.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            BitmapData outputData = output.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            try
            {
                for (int y = 0; y < source.Height; y++)
                {
                    byte* sourceRow = (byte*)sourceData.Scan0 + y * sourceData.Stride;
                    byte* outputRow = (byte*)outputData.Scan0 + y * outputData.Stride;
                    for (int x = 0; x < source.Width; x++)
                    {
                        byte* src = sourceRow + x * 4;
                        float b = src[0] / 255.0f * exposure;
                        float g = src[1] / 255.0f * exposure;
                        float r = src[2] / 255.0f * exposure;
                        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        if (luminance > 0.72f)
                        {
                            float over = luminance - 0.72f;
                            float mapped = 0.72f + over / (1.0f + over * 4.0f * highlightProtect);
                            float scale = mapped / Math.Max(0.0001f, luminance);
                            r *= scale;
                            g *= scale;
                            b *= scale;
                        }

                        byte* dst = outputRow + x * 4;
                        dst[0] = PixelMath.ToByte(b);
                        dst[1] = PixelMath.ToByte(g);
                        dst[2] = PixelMath.ToByte(r);
                        dst[3] = 255;
                    }
                }
            }
            finally
            {
                source.UnlockBits(sourceData);
                output.UnlockBits(outputData);
            }

            return output;
        }

        private static void ApplyMosaic(Bitmap bitmap, System.Drawing.Rectangle rect, int preferredBlockSize)
        {
            rect.Intersect(new System.Drawing.Rectangle(0, 0, bitmap.Width, bitmap.Height));
            if (rect.Width <= 0 || rect.Height <= 0) return;
            int block = Math.Max(8, preferredBlockSize);
            using Graphics graphics = Graphics.FromImage(bitmap);
            for (int y = rect.Top; y < rect.Bottom; y += block)
            {
                for (int x = rect.Left; x < rect.Right; x += block)
                {
                    int w = Math.Min(block, rect.Right - x);
                    int h = Math.Min(block, rect.Bottom - y);
                    Color c = bitmap.GetPixel(x + w / 2, y + h / 2);
                    using SolidBrush brush = new(c);
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
        }

        private void PreviewMouseDown(object? sender, MouseEventArgs e)
        {
            if (HitTestResize(PointToClient(preview.PointToScreen(e.Location))) != WindowResizeHit.None) return;
            if (mode == EditMode.None || e.Button != MouseButtons.Left) return;
            dragging = true;
            dragStart = e.Location;
            dragCurrent = e.Location;
            currentPenPoints.Clear();
            if (mode == EditMode.Pen && PreviewImageBounds().Contains(e.Location))
            {
                currentPenPoints.Add(ClientPointToImage(e.Location));
            }
            preview.Capture = true;
            preview.Invalidate();
        }

        private void PreviewMouseMove(object? sender, MouseEventArgs e)
        {
            if (!dragging) return;
            dragCurrent = e.Location;
            if (mode == EditMode.Pen)
            {
                System.Drawing.Rectangle imageBounds = PreviewImageBounds();
                if (imageBounds.Contains(e.Location))
                {
                    currentPenPoints.Add(ClientPointToImage(e.Location));
                }
                else if (currentPenPoints.Count > 0 && !IsPathBreak(currentPenPoints[^1]))
                {
                    currentPenPoints.Add(new System.Drawing.Point(int.MinValue, int.MinValue));
                }
            }
            preview.Invalidate();
        }

        private void PreviewMouseUp(object? sender, MouseEventArgs e)
        {
            if (!dragging || e.Button != MouseButtons.Left) return;
            dragging = false;
            preview.Capture = false;
            dragCurrent = e.Location;
            if (mode == EditMode.Pen)
            {
                TrimPathBreaks(currentPenPoints);
                if (currentPenPoints.Count(point => !IsPathBreak(point)) > 1)
                {
                    operations.Add(new EditOperation(EditOperationType.Pen, System.Drawing.Rectangle.Empty,
                        AnnotationColors[annotationColorIndex], currentPenPoints.ToList(), string.Empty, penStrokeWidth));
                    redoOperations.Clear();
                    RebuildCurrent();
                }
                currentPenPoints.Clear();
                return;
            }

            System.Drawing.Rectangle imageRect = PreviewSelectionToImage();
            if (imageRect.Width < 4 || imageRect.Height < 4) return;
            EditOperationType operationType = mode switch
            {
                EditMode.Ellipse => EditOperationType.Ellipse,
                EditMode.Mosaic => EditOperationType.Mosaic,
                _ => EditOperationType.Marker
            };
            int strokeWidth = mode == EditMode.Mosaic ? mosaicBrushSize : shapeStrokeWidth;
            operations.Add(new EditOperation(operationType, imageRect, AnnotationColors[annotationColorIndex], null, string.Empty, strokeWidth));
            redoOperations.Clear();
            RebuildCurrent();
        }

        private void PreviewPaint(object? sender, PaintEventArgs e)
        {
            if (!dragging) return;
            System.Drawing.Rectangle rect = CurrentPreviewSelection();
            if (rect.Width <= 0 || rect.Height <= 0) return;
            float previewStrokeWidth = mode == EditMode.Pen ? penStrokeWidth : shapeStrokeWidth;
            using Pen pen = new(AnnotationColors[annotationColorIndex], previewStrokeWidth)
            {
                StartCap = LineCap.Round,
                EndCap = LineCap.Round,
                LineJoin = LineJoin.Round
            };
            if (mode == EditMode.Ellipse) e.Graphics.DrawEllipse(pen, rect);
            else if (mode == EditMode.Pen)
            {
                List<System.Drawing.Point> points = currentPenPoints
                    .Select(point => IsPathBreak(point) ? point : ImagePointToPreview(point))
                    .ToList();
                DrawSegmentedLines(e.Graphics, pen, points);
            }
            else e.Graphics.DrawRectangle(pen, rect);
        }

        private System.Drawing.Rectangle CurrentPreviewSelection()
        {
            int left = Math.Min(dragStart.X, dragCurrent.X);
            int top = Math.Min(dragStart.Y, dragCurrent.Y);
            int right = Math.Max(dragStart.X, dragCurrent.X);
            int bottom = Math.Max(dragStart.Y, dragCurrent.Y);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private System.Drawing.Rectangle PreviewSelectionToImage()
        {
            System.Drawing.Rectangle selection = CurrentPreviewSelection();
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            selection.Intersect(imageBounds);
            if (selection.Width <= 0 || selection.Height <= 0) return System.Drawing.Rectangle.Empty;
            double scaleX = current.Width / (double)imageBounds.Width;
            double scaleY = current.Height / (double)imageBounds.Height;
            int x = Math.Clamp((int)Math.Round((selection.X - imageBounds.X) * scaleX), 0, current.Width - 1);
            int y = Math.Clamp((int)Math.Round((selection.Y - imageBounds.Y) * scaleY), 0, current.Height - 1);
            int right = Math.Clamp((int)Math.Round((selection.Right - imageBounds.X) * scaleX), x + 1, current.Width);
            int bottom = Math.Clamp((int)Math.Round((selection.Bottom - imageBounds.Y) * scaleY), y + 1, current.Height);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Point ClientPointToImage(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = current.Width / (double)Math.Max(1, imageBounds.Width);
            double scaleY = current.Height / (double)Math.Max(1, imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((point.X - imageBounds.X) * scaleX), 0, current.Width - 1);
            int y = Math.Clamp((int)Math.Round((point.Y - imageBounds.Y) * scaleY), 0, current.Height - 1);
            return new System.Drawing.Point(x, y);
        }

        private System.Drawing.Point ImagePointToPreview(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / (double)Math.Max(1, current.Width);
            double scaleY = imageBounds.Height / (double)Math.Max(1, current.Height);
            return new System.Drawing.Point(
                imageBounds.X + (int)Math.Round(point.X * scaleX),
                imageBounds.Y + (int)Math.Round(point.Y * scaleY));
        }

        private System.Drawing.Rectangle PreviewImageBounds()
        {
            if (current.Width <= 0 || current.Height <= 0 || preview.Width <= 0 || preview.Height <= 0)
            {
                return preview.ClientRectangle;
            }
            double ratio = Math.Min(preview.Width / (double)current.Width, preview.Height / (double)current.Height);
            int width = (int)Math.Round(current.Width * ratio);
            int height = (int)Math.Round(current.Height * ratio);
            int x = (preview.Width - width) / 2;
            int y = (preview.Height - height) / 2;
            return new System.Drawing.Rectangle(x, y, width, height);
        }
    }
        private System.Drawing.Point dragStart;
        private System.Drawing.Point dragCurrent;
        private bool dragging;
        private bool selectionReady;
        private EditMode mode = EditMode.None;
        private AdjustmentPreset selectedPreset = AdjustmentPreset.Balanced;
        private int annotationColorIndex;
        private int shapeStrokeWidth = 4;
        private int penStrokeWidth = 6;
        private int mosaicBrushSize = 28;
        private bool editDragging;
        private System.Drawing.Point editStart;
        private System.Drawing.Point editCurrent;
        private readonly List<System.Drawing.Point> currentPenPoints = new();
        private bool currentStrokeInside;
        private readonly ToolbarTooltipLabel selectionTooltip = new();
        private readonly System.Windows.Forms.Timer selectionTooltipTimer = new();
        private readonly List<EditOperation> operations = new();
        private readonly List<EditOperation> redoOperations = new();
        private Bitmap? selectionBackgroundBitmap;
        private Bitmap? mosaicPreviewBitmap;
        private System.Drawing.Rectangle mosaicPreviewSelection;
        private int mosaicPreviewBrushSize;
        private System.Drawing.Rectangle toolbarBounds;
        private readonly List<ToolbarItem> toolbarItems = new();
        private int hoveredToolbarItem = -1;
        private int pressedToolbarItem = -1;
        private System.Drawing.Rectangle optionsBounds;
        private readonly List<OptionPopoverItem> optionItems = new();
        private bool optionsVisible;
        private int hoveredOptionItem = -1;
        private int pressedOptionItem = -1;
        private bool toolbarTooltipActive;
        private System.Drawing.Point toolbarTooltipAnchor;
        private bool mouseInputReady;
        private bool closingHidden;
        private readonly nint previousForegroundWindow;

        public System.Drawing.Rectangle SelectedImageRegion { get; private set; }

        public SelectionCommitAction CommitAction { get; private set; } = SelectionCommitAction.Copy;

        public AdjustmentPreset SelectedPreset => selectedPreset;

        public IReadOnlyList<EditOperation> Operations => operations;

        public RegionSelectionForm(Bitmap preview)
        {
            this.preview = preview;
            previousForegroundWindow = GetForegroundWindow();
            targetBounds = Screen.PrimaryScreen?.Bounds ?? new System.Drawing.Rectangle(0, 0, preview.Width, preview.Height);
            Bounds = new System.Drawing.Rectangle(-32000, -32000, targetBounds.Width, targetBounds.Height);
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.Manual;
            TopMost = true;
            ShowInTaskbar = false;
            DoubleBuffered = true;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            KeyPreview = true;
            Cursor = Cursors.Cross;
            BackColor = Color.Black;
            mouseInputReady = true;
            Font = new Font(CaptureText.FontFamily, 9.0f, FontStyle.Regular, GraphicsUnit.Point);

            selectionTooltip.Visible = false;
            selectionTooltipTimer.Interval = 1000;
            selectionTooltipTimer.Tick += (_, _) => ShowSelectionTooltip();

            Controls.Add(selectionTooltip);
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            RegisterHotKey(Handle, EscapeHotkeyId, 0, VkEscape);
        }

        protected override void OnHandleDestroyed(EventArgs e)
        {
            UnregisterHotKey(Handle, EscapeHotkeyId);
            base.OnHandleDestroyed(e);
        }

        public void ReplacePreview(Bitmap nextPreview)
        {
            Bitmap oldPreview = preview;
            preview = nextPreview;
            BuildSelectionBackgroundCache();
            DisposeMosaicPreviewCache();
            Invalidate();
            oldPreview.Dispose();
        }

        private void BuildSelectionBackgroundCache()
        {
            selectionBackgroundBitmap?.Dispose();
            selectionBackgroundBitmap = null;
            if (preview.Width <= 0 || preview.Height <= 0) return;

            Bitmap bitmap = new(preview.Width, preview.Height, PixelFormat.Format32bppPArgb);
            bitmap.SetResolution(preview.HorizontalResolution, preview.VerticalResolution);
            using Graphics graphics = Graphics.FromImage(bitmap);
            graphics.Clear(Color.Black);
            graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
            graphics.PixelOffsetMode = PixelOffsetMode.Half;
            graphics.DrawImageUnscaled(preview, 0, 0);
            using SolidBrush shade = new(Color.FromArgb(96, 0, 0, 0));
            graphics.FillRectangle(shade, 0, 0, bitmap.Width, bitmap.Height);
            selectionBackgroundBitmap = bitmap;
        }

        private void InvalidateSelectionChange(System.Drawing.Rectangle oldSelection, System.Drawing.Rectangle newSelection)
        {
            System.Drawing.Rectangle dirty = oldSelection.Width <= 0 || oldSelection.Height <= 0
                ? newSelection
                : newSelection.Width <= 0 || newSelection.Height <= 0
                    ? oldSelection
                    : System.Drawing.Rectangle.Union(oldSelection, newSelection);
            dirty = InflateRectangle(dirty, 18);
            dirty.Intersect(ClientRectangle);
            if (dirty.Width > 0 && dirty.Height > 0)
            {
                Invalidate(dirty);
            }
        }

        private void ShowSelectionTooltip()
        {
            selectionTooltipTimer.Stop();
            if (!toolbarTooltipActive || !selectionReady) return;
            System.Drawing.Size textSize = TextRenderer.MeasureText(selectionTooltip.Text, selectionTooltip.Font);
            selectionTooltip.Width = Math.Max(64, textSize.Width + 24);
            selectionTooltip.Height = 30;
            System.Drawing.Point center = toolbarTooltipAnchor;
            int left = Math.Clamp(center.X - selectionTooltip.Width / 2, 12, Math.Max(12, ClientSize.Width - selectionTooltip.Width - 12));
            int top = toolbarBounds.Top - selectionTooltip.Height - 8;
            if (top < 12) top = toolbarBounds.Bottom + 8;
            selectionTooltip.Location = new System.Drawing.Point(left, top);
            selectionTooltip.Visible = true;
            selectionTooltip.BringToFront();
        }

        private void ShowToolOptions(EditMode toolMode, System.Drawing.Rectangle anchor)
        {
            int[] values = toolMode switch
            {
                EditMode.Marker or EditMode.Ellipse => new[] { 2, 4, 6 },
                EditMode.Pen => new[] { 3, 6, 10 },
                EditMode.Mosaic => new[] { 16, 28, 42 },
                _ => Array.Empty<int>()
            };
            if (values.Length == 0)
            {
                HideOptionsPopover();
                return;
            }

            PrepareOptionsPopover(anchor, Math.Max(174, CalculateOptionsWidth(values.Length)));
            for (int index = 0; index < values.Length; index++)
            {
                int value = values[index];
                System.Drawing.Rectangle itemRect = OptionItemRect(index);
                optionItems.Add(new OptionPopoverItem(
                    itemRect,
                    value.ToString(System.Globalization.CultureInfo.InvariantCulture),
                    null,
                    IsToolSizeSelected(toolMode, value),
                    () =>
                    {
                        SetToolSize(toolMode, value);
                        HideOptionsPopover();
                        Invalidate();
                    }));
            }

            ShowOptionsPopover();
        }

        private void ShowColorOptions(System.Drawing.Rectangle anchor)
        {
            PrepareOptionsPopover(anchor, CalculateOptionsWidth(AnnotationColors.Length));
            for (int index = 0; index < AnnotationColors.Length; index++)
            {
                int colorIndex = index;
                System.Drawing.Rectangle itemRect = OptionItemRect(index);
                optionItems.Add(new OptionPopoverItem(
                    itemRect,
                    string.Empty,
                    AnnotationColors[colorIndex],
                    annotationColorIndex == colorIndex,
                    () =>
                    {
                        annotationColorIndex = colorIndex;
                        HideOptionsPopover();
                        Invalidate();
                    }));
            }
            ShowOptionsPopover();
        }

        private static int CalculateOptionsWidth(int count)
        {
            const int itemWidth = 42;
            const int gap = 8;
            const int padX = 12;
            return padX * 2 + count * itemWidth + Math.Max(0, count - 1) * gap;
        }

        private void PrepareOptionsPopover(System.Drawing.Rectangle anchor, int width)
        {
            System.Drawing.Rectangle oldBounds = optionsBounds;
            optionItems.Clear();
            hoveredOptionItem = -1;
            pressedOptionItem = -1;
            optionsBounds = PositionOptionsPanel(anchor, width, 48);
            if (optionsVisible && !oldBounds.IsEmpty) Invalidate(InflateRectangle(oldBounds, 8));
        }

        private System.Drawing.Rectangle OptionItemRect(int index)
        {
            const int itemWidth = 42;
            const int itemHeight = 34;
            const int gap = 8;
            return new System.Drawing.Rectangle(
                optionsBounds.Left + 12 + index * (itemWidth + gap),
                optionsBounds.Top + (optionsBounds.Height - itemHeight) / 2,
                itemWidth,
                itemHeight);
        }

        private void ShowOptionsPopover()
        {
            optionsVisible = optionItems.Count > 0;
            if (optionsVisible)
            {
                selectionTooltip.Visible = false;
                Invalidate(InflateRectangle(optionsBounds, 8));
            }
        }

        private void HideOptionsPopover()
        {
            if (!optionsVisible && optionItems.Count == 0) return;
            System.Drawing.Rectangle oldBounds = optionsBounds;
            optionsVisible = false;
            optionItems.Clear();
            hoveredOptionItem = -1;
            pressedOptionItem = -1;
            if (!oldBounds.IsEmpty) Invalidate(InflateRectangle(oldBounds, 8));
        }

        private int HitTestOptions(System.Drawing.Point point)
        {
            if (!optionsVisible || !optionsBounds.Contains(point)) return -1;
            for (int index = 0; index < optionItems.Count; index++)
            {
                if (optionItems[index].Rect.Contains(point)) return index;
            }
            return -1;
        }

        private void DrawOptionsPopover(Graphics graphics)
        {
            if (!optionsVisible || optionsBounds.Width <= 0 || optionsBounds.Height <= 0) return;
            graphics.SmoothingMode = SmoothingMode.AntiAlias;

            using GraphicsPath shadowPath = RoundedRect(new System.Drawing.Rectangle(optionsBounds.Left + 2, optionsBounds.Top + 4, optionsBounds.Width - 4, optionsBounds.Height - 4), (optionsBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(70, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            System.Drawing.Rectangle pill = new(optionsBounds.Left, optionsBounds.Top, optionsBounds.Width - 1, optionsBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using Region oldClip = graphics.Clip;
            graphics.SetClip(path);
            DrawToolbarBackdrop(graphics, pill);
            graphics.Clip = oldClip;

            using SolidBrush fill = new(Color.FromArgb(184, 27, 30, 33));
            using Pen border = new(Color.FromArgb(96, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            for (int index = 0; index < optionItems.Count; index++)
            {
                OptionPopoverItem item = optionItems[index];
                bool hot = index == hoveredOptionItem || index == pressedOptionItem;
                if (hot || item.Selected)
                {
                    int size = item.Selected ? 34 : 32;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hoverPath = RoundedRect(new System.Drawing.Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 50 : 34, item.Selected ? 64 : 255, item.Selected ? 178 : 255, item.Selected ? 255 : 255));
                    graphics.FillPath(hoverBrush, hoverPath);
                }

                if (item.Color.HasValue)
                {
                    DrawColorOption(graphics, item.Rect, item.Color.Value, item.Selected);
                }
                else
                {
                    Color textColor = item.Selected ? Color.FromArgb(64, 178, 255) : Color.FromArgb(225, 229, 232);
                    TextRenderer.DrawText(graphics, item.Text, Font, item.Rect, textColor,
                        TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                }
            }
        }

        private static void DrawColorOption(Graphics graphics, System.Drawing.Rectangle rect, Color color, bool selected)
        {
            int diameter = selected ? 21 : 19;
            int x = rect.Left + (rect.Width - diameter) / 2;
            int y = rect.Top + (rect.Height - diameter) / 2;
            using SolidBrush swatch = new(color);
            using Pen border = new(color.ToArgb() == Color.White.ToArgb()
                ? Color.FromArgb(220, 225, 229, 232)
                : Color.FromArgb(90, 255, 255, 255), 1.2f);
            graphics.FillEllipse(swatch, x, y, diameter, diameter);
            graphics.DrawEllipse(border, x, y, diameter, diameter);
            if (selected)
            {
                using Pen ring = new(Color.FromArgb(220, 225, 229, 232), 1.5f);
                graphics.DrawEllipse(ring, x - 4, y - 4, diameter + 8, diameter + 8);
            }
        }

        private System.Drawing.Rectangle PositionOptionsPanel(System.Drawing.Rectangle anchor, int width, int height)
        {
            System.Drawing.Point center = new(anchor.Left + anchor.Width / 2, anchor.Top);
            int left = Math.Clamp(center.X - width / 2, 12, Math.Max(12, ClientSize.Width - width - 12));
            int top = toolbarBounds.Top - height - 8;
            if (top < 12) top = toolbarBounds.Bottom + 8;
            return new System.Drawing.Rectangle(left, top, width, height);
        }

        private bool IsToolSizeSelected(EditMode toolMode, int value) => toolMode switch
        {
            EditMode.Marker or EditMode.Ellipse => shapeStrokeWidth == value,
            EditMode.Pen => penStrokeWidth == value,
            EditMode.Mosaic => mosaicBrushSize == value,
            _ => false
        };

        private void SetToolSize(EditMode toolMode, int value)
        {
            if (toolMode is EditMode.Marker or EditMode.Ellipse) shapeStrokeWidth = value;
            if (toolMode == EditMode.Pen) penStrokeWidth = value;
            if (toolMode == EditMode.Mosaic)
            {
                mosaicBrushSize = value;
                DisposeMosaicPreviewCache();
            }
        }

        private void UndoSelectionOperation()
        {
            if (operations.Count == 0) return;
            redoOperations.Add(operations[^1]);
            operations.RemoveAt(operations.Count - 1);
            Invalidate();
        }

        private void RedoSelectionOperation()
        {
            if (redoOperations.Count == 0) return;
            operations.Add(redoOperations[^1]);
            redoOperations.RemoveAt(redoOperations.Count - 1);
            Invalidate();
        }

        private void ResetSelectionOperations()
        {
            operations.Clear();
            redoOperations.Clear();
            Invalidate();
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            BuildSelectionBackgroundCache();
            Refresh();
            Bounds = targetBounds;
            SetWindowPos(Handle, HwndTopmost, targetBounds.Left, targetBounds.Top,
                targetBounds.Width, targetBounds.Height, SwpShowWindow);
            BuildSelectionBackgroundCache();
            Refresh();
            BringToFront();
            SetForegroundWindow(Handle);
            Activate();
            Focus();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                selectionTooltipTimer.Dispose();
                selectionBackgroundBitmap?.Dispose();
                mosaicPreviewBitmap?.Dispose();
                preview.Dispose();
            }
            base.Dispose(disposing);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            HideBeforeClose();
            base.OnFormClosing(e);
        }

        private void HideBeforeClose()
        {
            if (closingHidden || !IsHandleCreated) return;
            closingHidden = true;
            try
            {
                SetWindowPos(Handle, HwndNoTopmost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpNoActivate);
                if (previousForegroundWindow != 0 &&
                    previousForegroundWindow != Handle &&
                    IsWindow(previousForegroundWindow))
                {
                    SetForegroundWindow(previousForegroundWindow);
                }
                ShowWindow(Handle, SwHide);
                SetWindowPos(Handle, HwndTopmost, -32000, -32000,
                    Math.Max(1, Width), Math.Max(1, Height), SwpHideWindow | SwpNoActivate);
                Hide();
            }
            catch
            {
            }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            e.Graphics.Clear(Color.Black);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
            e.Graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            if (selectionBackgroundBitmap is null ||
                selectionBackgroundBitmap.Width != preview.Width ||
                selectionBackgroundBitmap.Height != preview.Height)
            {
                BuildSelectionBackgroundCache();
            }
            if (selectionBackgroundBitmap is not null)
            {
                e.Graphics.DrawImage(selectionBackgroundBitmap, imageBounds);
            }
            else
            {
                e.Graphics.DrawImage(preview, imageBounds);
                using SolidBrush shade = new(Color.FromArgb(96, 0, 0, 0));
                e.Graphics.FillRectangle(shade, ClientRectangle);
            }

            if (!selectionReady && !dragging)
            {
                string hint = CaptureText.Get(CaptureString.SelectHint);
                using Font hintFont = new(CaptureText.FontFamily, 13.0f, FontStyle.Regular, GraphicsUnit.Point);
                SizeF hintSize = e.Graphics.MeasureString(hint, hintFont);
                System.Drawing.RectangleF hintBox = new(24, 24, hintSize.Width + 28, hintSize.Height + 16);
                using SolidBrush hintBack = new(Color.FromArgb(190, 24, 24, 24));
                using SolidBrush hintText = new(Color.White);
                e.Graphics.FillRectangle(hintBack, hintBox);
                e.Graphics.DrawString(hint, hintFont, hintText, hintBox.X + 14, hintBox.Y + 8);
            }

            System.Drawing.Rectangle selection = CurrentSelection();
            if (selection.Width > 0 && selection.Height > 0)
            {
                using Region oldClip = e.Graphics.Clip;
                e.Graphics.SetClip(selection);
                e.Graphics.DrawImage(preview, imageBounds);
                DrawPresetPreview(e.Graphics, selection);
                e.Graphics.Clip = oldClip;
                using Pen border = new(Color.FromArgb(0, 120, 215), 2.5f);
                e.Graphics.DrawRectangle(border, selection);
                using Pen inner = new(Color.White, 1.0f);
                e.Graphics.DrawRectangle(inner, System.Drawing.Rectangle.Inflate(selection, -2, -2));
                DrawSelectionHandles(e.Graphics, selection);
                DrawSelectionOperations(e.Graphics);
                if (editDragging)
                {
                    System.Drawing.Rectangle editRect = CurrentEditSelection();
                    editRect.Intersect(selection);
                    if (mode == EditMode.Mosaic)
                    {
                        using Region oldEditClip = e.Graphics.Clip;
                        e.Graphics.SetClip(InsetRectangle(selection, Math.Max(2, mosaicBrushSize / 2 + 2)));
                        DrawMosaicPreview(e.Graphics, ClientToImage(editRect), editRect, false);
                        e.Graphics.Clip = oldEditClip;
                    }
                    else if (mode == EditMode.Pen)
                    {
                        using Region oldEditClip = e.Graphics.Clip;
                        e.Graphics.SetClip(selection);
                        DrawClientPolyline(e.Graphics, currentPenPoints, AnnotationColors[annotationColorIndex], penStrokeWidth);
                        e.Graphics.Clip = oldEditClip;
                    }
                    else if (mode == EditMode.Ellipse)
                    {
                        using Pen editPen = new(AnnotationColors[annotationColorIndex], shapeStrokeWidth);
                        e.Graphics.DrawEllipse(editPen, editRect);
                    }
                    else
                    {
                        using Pen editPen = new(AnnotationColors[annotationColorIndex], shapeStrokeWidth);
                        e.Graphics.DrawRectangle(editPen, editRect);
                    }
                }
            }
            if (selectionReady)
            {
                DrawSelectionToolbar(e.Graphics);
                DrawOptionsPopover(e.Graphics);
            }
        }

        private void DrawSelectionToolbar(Graphics graphics)
        {
            if (toolbarBounds.Width <= 0 || toolbarBounds.Height <= 0) return;
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using GraphicsPath shadowPath = RoundedRect(new System.Drawing.Rectangle(toolbarBounds.Left + 2, toolbarBounds.Top + 4, toolbarBounds.Width - 4, toolbarBounds.Height - 4), (toolbarBounds.Height - 4) / 2);
            using SolidBrush shadow = new(Color.FromArgb(80, 0, 0, 0));
            graphics.FillPath(shadow, shadowPath);

            System.Drawing.Rectangle pill = new(toolbarBounds.Left, toolbarBounds.Top, toolbarBounds.Width - 1, toolbarBounds.Height - 1);
            using GraphicsPath path = RoundedRect(pill, pill.Height / 2);
            using Region oldClip = graphics.Clip;
            graphics.SetClip(path);
            DrawToolbarBackdrop(graphics, pill);
            graphics.Clip = oldClip;

            using SolidBrush fill = new(Color.FromArgb(178, 27, 30, 33));
            using Pen border = new(Color.FromArgb(95, 255, 255, 255), 1.0f);
            graphics.FillPath(fill, path);
            graphics.DrawPath(border, path);

            foreach (ToolbarItem item in toolbarItems)
            {
                if (item.Action is ToolbarAction.Undo or ToolbarAction.PresetLow or ToolbarAction.Save)
                {
                    DrawToolbarSeparator(graphics, item.Rect.Left - 11);
                }

                bool selected = IsToolbarItemSelected(item.Action);
                bool hot = toolbarItems.IndexOf(item) == hoveredToolbarItem || toolbarItems.IndexOf(item) == pressedToolbarItem;
                Color iconColor = ToolbarIconColor(item.Action, selected);
                if (hot || selected)
                {
                    int size = hot ? 38 : 34;
                    int cx = item.Rect.Left + item.Rect.Width / 2;
                    int cy = item.Rect.Top + item.Rect.Height / 2;
                    using GraphicsPath hover = RoundedRect(new System.Drawing.Rectangle(cx - size / 2, cy - size / 2, size, size), size / 2);
                    using SolidBrush hoverBrush = new(Color.FromArgb(hot ? 44 : 28, selected ? 64 : 255, selected ? 178 : 255, selected ? 255 : 255));
                    graphics.FillPath(hoverBrush, hover);
                }

                System.Drawing.Rectangle iconRect = new(item.Rect.Left + 10, item.Rect.Top + 8, 26, 26);
                if (item.Action == ToolbarAction.Color)
                {
                    iconColor = AnnotationColors[annotationColorIndex];
                }
                PillButton.DrawIcon(graphics, iconRect, item.Icon, iconColor);
            }
        }

        private void DrawToolbarBackdrop(Graphics graphics, System.Drawing.Rectangle pill)
        {
            try
            {
                System.Drawing.Rectangle imageRect = ClientToImage(pill);
                if (imageRect.Width <= 0 || imageRect.Height <= 0) return;
                int smallW = Math.Max(1, pill.Width / 10);
                int smallH = Math.Max(1, pill.Height / 10);
                using Bitmap small = new(smallW, smallH, PixelFormat.Format32bppArgb);
                using (Graphics smallGraphics = Graphics.FromImage(small))
                {
                    smallGraphics.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    smallGraphics.DrawImage(preview, new System.Drawing.Rectangle(0, 0, smallW, smallH), imageRect, GraphicsUnit.Pixel);
                }
                using Bitmap blurred = new(pill.Width, pill.Height, PixelFormat.Format32bppArgb);
                using (Graphics blurGraphics = Graphics.FromImage(blurred))
                {
                    blurGraphics.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    blurGraphics.DrawImage(small, new System.Drawing.Rectangle(0, 0, blurred.Width, blurred.Height));
                    using SolidBrush dim = new(Color.FromArgb(135, 0, 0, 0));
                    blurGraphics.FillRectangle(dim, 0, 0, blurred.Width, blurred.Height);
                }
                graphics.DrawImageUnscaled(blurred, pill.Location);
            }
            catch
            {
            }
        }

        private void DrawToolbarSeparator(Graphics graphics, int x)
        {
            using Pen pen = new(Color.FromArgb(70, 255, 255, 255), 1.0f);
            graphics.DrawLine(pen, x, toolbarBounds.Top + 19, x, toolbarBounds.Bottom - 19);
        }

        private bool IsToolbarItemSelected(ToolbarAction action)
        {
            return action switch
            {
                ToolbarAction.ToolMarker => mode == EditMode.Marker,
                ToolbarAction.ToolEllipse => mode == EditMode.Ellipse,
                ToolbarAction.ToolPen => mode == EditMode.Pen,
                ToolbarAction.ToolMosaic => mode == EditMode.Mosaic,
                ToolbarAction.PresetLow => selectedPreset == AdjustmentPreset.Low,
                ToolbarAction.PresetBalanced => selectedPreset == AdjustmentPreset.Balanced,
                ToolbarAction.PresetHigh => selectedPreset == AdjustmentPreset.High,
                _ => false
            };
        }

        private static Color ToolbarIconColor(ToolbarAction action, bool selected)
        {
            if (action == ToolbarAction.Cancel) return Color.FromArgb(255, 92, 92);
            if (action == ToolbarAction.Copy) return Color.FromArgb(68, 214, 111);
            if (selected) return Color.FromArgb(64, 178, 255);
            return Color.FromArgb(225, 229, 232);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            if (e.Button != MouseButtons.Left) return;
            if (optionsVisible)
            {
                pressedOptionItem = HitTestOptions(e.Location);
                if (pressedOptionItem >= 0)
                {
                    Capture = true;
                    Invalidate(InflateRectangle(optionsBounds, 8));
                    return;
                }

                if (!toolbarBounds.Contains(e.Location))
                {
                    HideOptionsPopover();
                }
            }
            if (selectionReady && toolbarBounds.Contains(e.Location))
            {
                pressedToolbarItem = HitTestToolbar(e.Location);
                if (pressedToolbarItem >= 0)
                {
                    Capture = true;
                    Invalidate(toolbarBounds);
                    return;
                }
            }
            if (selectionReady && mode != EditMode.None && CurrentSelection().Contains(e.Location))
            {
                editDragging = true;
                editStart = e.Location;
                editCurrent = e.Location;
                currentPenPoints.Clear();
                currentStrokeInside = CurrentSelection().Contains(e.Location);
                if (mode == EditMode.Pen && currentStrokeInside) currentPenPoints.Add(e.Location);
                Capture = true;
                Invalidate();
                return;
            }
            selectionReady = false;
            HideToolbarFeedback();
            operations.Clear();
            redoOperations.Clear();
            DisposeMosaicPreviewCache();
            dragging = true;
            dragStart = e.Location;
            dragCurrent = e.Location;
            Capture = true;
            Invalidate();
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            UpdateSelectionCursor(e.Location);
            if (optionsVisible)
            {
                int optionHover = HitTestOptions(e.Location);
                if (optionHover != hoveredOptionItem)
                {
                    hoveredOptionItem = optionHover;
                    Invalidate(InflateRectangle(optionsBounds, 8));
                }
            }
            if (!dragging && !editDragging && selectionReady)
            {
                int hover = HitTestToolbar(e.Location);
                if (hover != hoveredToolbarItem)
                {
                    hoveredToolbarItem = hover;
                    UpdateToolbarTooltip(hover);
                    Invalidate(toolbarBounds);
                }
            }
            if (editDragging)
            {
                editCurrent = e.Location;
                if (mode == EditMode.Pen)
                {
                    bool inside = CurrentSelection().Contains(e.Location);
                    if (inside)
                    {
                        if (!currentStrokeInside && currentPenPoints.Count > 0)
                        {
                            currentPenPoints.Add(new System.Drawing.Point(int.MinValue, int.MinValue));
                        }
                        if (ShouldAppendStrokePoint(e.Location))
                        {
                            currentPenPoints.Add(e.Location);
                        }
                    }
                    currentStrokeInside = inside;
                }
                Invalidate();
                return;
            }
            if (!dragging) return;
            System.Drawing.Rectangle oldSelection = CurrentSelection();
            dragCurrent = e.Location;
            InvalidateSelectionChange(oldSelection, CurrentSelection());
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            if (!mouseInputReady) return;
            if (pressedOptionItem >= 0 && e.Button == MouseButtons.Left)
            {
                int pressed = pressedOptionItem;
                pressedOptionItem = -1;
                Capture = false;
                if (pressed == HitTestOptions(e.Location) && pressed >= 0 && pressed < optionItems.Count)
                {
                    optionItems[pressed].Click();
                }
                else
                {
                    Invalidate(InflateRectangle(optionsBounds, 8));
                }
                return;
            }
            if (pressedToolbarItem >= 0 && e.Button == MouseButtons.Left)
            {
                int pressed = pressedToolbarItem;
                pressedToolbarItem = -1;
                Capture = false;
                if (pressed == HitTestToolbar(e.Location))
                {
                    ExecuteToolbarAction(toolbarItems[pressed]);
                }
                Invalidate();
                return;
            }
            if (editDragging && e.Button == MouseButtons.Left)
            {
                editDragging = false;
                Capture = false;
                editCurrent = e.Location;
                System.Drawing.Rectangle activeSelection = CurrentSelection();
                if (mode == EditMode.Pen)
                {
                    List<System.Drawing.Point> points = currentPenPoints
                        .Where(point => IsPathBreak(point) || activeSelection.Contains(point))
                        .Select(point => IsPathBreak(point) ? point : ClientPointToImage(point))
                        .ToList();
                    if (points.Count(point => !IsPathBreak(point)) > 1)
                    {
                        operations.Add(new EditOperation(
                            EditOperationType.Pen,
                            System.Drawing.Rectangle.Empty,
                            AnnotationColors[annotationColorIndex],
                            points,
                            string.Empty,
                            penStrokeWidth));
                        redoOperations.Clear();
                    }
                    currentPenPoints.Clear();
                    currentStrokeInside = false;
                }
                else
                {
                    System.Drawing.Rectangle editRect = CurrentEditSelection();
                    editRect.Intersect(activeSelection);
                    if (editRect.Width >= 4 && editRect.Height >= 4)
                    {
                        EditOperationType operationType = mode switch
                        {
                            EditMode.Ellipse => EditOperationType.Ellipse,
                            EditMode.Mosaic => EditOperationType.Mosaic,
                            _ => EditOperationType.Marker
                        };
                        int strokeWidth = mode == EditMode.Mosaic ? mosaicBrushSize : shapeStrokeWidth;
                        operations.Add(new EditOperation(operationType, ClientToImage(editRect), AnnotationColors[annotationColorIndex], null, string.Empty, strokeWidth));
                        redoOperations.Clear();
                    }
                }
                Invalidate();
                return;
            }
            if (!dragging || e.Button != MouseButtons.Left) return;
            dragging = false;
            Capture = false;
            dragCurrent = e.Location;
            System.Drawing.Rectangle selected = CurrentSelection();
            if (selected.Width < 4 || selected.Height < 4)
            {
                selectionReady = false;
                HideToolbarFeedback();
                Invalidate();
                return;
            }
            SelectedImageRegion = ClientToImage(selected);
            selectionReady = true;
            DisposeMosaicPreviewCache();
            LayoutSelectionToolbar(selected);
            Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            Cursor = Cursors.Cross;
            base.OnMouseLeave(e);
        }

        private void CommitSelection(SelectionCommitAction action)
        {
            if (!selectionReady || SelectedImageRegion.Width <= 0 || SelectedImageRegion.Height <= 0) return;
            CommitAction = action;
            HideBeforeClose();
            DialogResult = DialogResult.OK;
            Close();
        }

        private void LayoutSelectionToolbar(System.Drawing.Rectangle selection)
        {
            int width = 820;
            int height = 62;
            int left = selection.Left + (selection.Width - width) / 2;
            left = Math.Clamp(left, 12, Math.Max(12, ClientSize.Width - width - 12));
            int top = selection.Bottom + 10;
            if (top + height > ClientSize.Height - 12)
            {
                top = selection.Top - height - 10;
            }
            top = Math.Clamp(top, 12, Math.Max(12, ClientSize.Height - height - 12));
            toolbarBounds = new System.Drawing.Rectangle(left, top, width, height);
            LayoutToolbarItems();
        }

        private void LayoutToolbarItems()
        {
            toolbarItems.Clear();
            if (toolbarBounds.Width <= 0 || toolbarBounds.Height <= 0) return;
            int x = toolbarBounds.Left + 14;
            int y = toolbarBounds.Top + (toolbarBounds.Height - 42) / 2;
            void Add(ToolbarAction action, ButtonIcon icon, string tooltip)
            {
                toolbarItems.Add(new ToolbarItem(action, icon, new System.Drawing.Rectangle(x, y, 46, 42), tooltip));
                x += 54;
            }
            void Sep() => x += 13;

            Add(ToolbarAction.Cancel, ButtonIcon.Cancel, CaptureText.Get(CaptureString.ToolbarCancel));
            Add(ToolbarAction.ToolMarker, ButtonIcon.Marker, CaptureText.Get(CaptureString.ToolbarMarker));
            Add(ToolbarAction.ToolEllipse, ButtonIcon.Ellipse, CaptureText.Get(CaptureString.ToolbarEllipse));
            Add(ToolbarAction.ToolPen, ButtonIcon.Pen, CaptureText.Get(CaptureString.ToolbarPen));
            Add(ToolbarAction.ToolMosaic, ButtonIcon.Mosaic, CaptureText.Get(CaptureString.ToolbarMosaic));
            Add(ToolbarAction.Color, ButtonIcon.Color, CaptureText.Get(CaptureString.ToolbarColor));
            Sep();
            Add(ToolbarAction.Undo, ButtonIcon.Undo, CaptureText.Get(CaptureString.ToolbarUndo));
            Add(ToolbarAction.Redo, ButtonIcon.Redo, CaptureText.Get(CaptureString.ToolbarRedo));
            Add(ToolbarAction.Reset, ButtonIcon.Reset, CaptureText.Get(CaptureString.ToolbarReset));
            Sep();
            Add(ToolbarAction.PresetLow, ButtonIcon.Low, CaptureText.Get(CaptureString.ToolbarHdrLow));
            Add(ToolbarAction.PresetBalanced, ButtonIcon.Balanced, CaptureText.Get(CaptureString.ToolbarHdrBalanced));
            Add(ToolbarAction.PresetHigh, ButtonIcon.High, CaptureText.Get(CaptureString.ToolbarHdrHigh));
            Sep();
            Add(ToolbarAction.Save, ButtonIcon.Save, CaptureText.Get(CaptureString.ToolbarSave));
            Add(ToolbarAction.Copy, ButtonIcon.Done, CaptureText.Get(CaptureString.ToolbarCopy));
        }

        private int HitTestToolbar(System.Drawing.Point point)
        {
            if (!toolbarBounds.Contains(point)) return -1;
            for (int i = 0; i < toolbarItems.Count; i++)
            {
                if (toolbarItems[i].Rect.Contains(point)) return i;
            }
            return -1;
        }

        private void UpdateSelectionCursor(System.Drawing.Point point)
        {
            if (dragging || editDragging)
            {
                if (Cursor != Cursors.Cross) Cursor = Cursors.Cross;
                return;
            }

            Cursor desired = Cursors.Cross;
            if (selectionReady)
            {
                if (HitTestOptions(point) >= 0 || HitTestToolbar(point) >= 0)
                {
                    desired = Cursors.Hand;
                }
                else if ((optionsVisible && optionsBounds.Contains(point)) || toolbarBounds.Contains(point))
                {
                    desired = Cursors.Default;
                }
            }

            if (Cursor != desired) Cursor = desired;
        }

        private void HideToolbarFeedback()
        {
            hoveredToolbarItem = -1;
            pressedToolbarItem = -1;
            toolbarTooltipActive = false;
            selectionTooltipTimer.Stop();
            selectionTooltip.Visible = false;
            HideOptionsPopover();
        }

        private void UpdateToolbarTooltip(int hover)
        {
            toolbarTooltipActive = hover >= 0;
            selectionTooltip.Visible = false;
            selectionTooltipTimer.Stop();
            if (hover < 0)
            {
                return;
            }
            ToolbarItem item = toolbarItems[hover];
            selectionTooltip.Text = item.Tooltip;
            toolbarTooltipAnchor = new System.Drawing.Point(item.Rect.Left + item.Rect.Width / 2, item.Rect.Top);
            selectionTooltipTimer.Start();
        }

        private void ExecuteToolbarAction(ToolbarItem item)
        {
            switch (item.Action)
            {
                case ToolbarAction.Cancel:
                    CancelSelection();
                    break;
                case ToolbarAction.ToolMarker:
                    SelectToolbarTool(EditMode.Marker, item.Rect);
                    break;
                case ToolbarAction.ToolEllipse:
                    SelectToolbarTool(EditMode.Ellipse, item.Rect);
                    break;
                case ToolbarAction.ToolPen:
                    SelectToolbarTool(EditMode.Pen, item.Rect);
                    break;
                case ToolbarAction.ToolMosaic:
                    SelectToolbarTool(EditMode.Mosaic, item.Rect);
                    break;
                case ToolbarAction.Color:
                    ShowColorOptions(item.Rect);
                    break;
                case ToolbarAction.Undo:
                    UndoSelectionOperation();
                    break;
                case ToolbarAction.Redo:
                    RedoSelectionOperation();
                    break;
                case ToolbarAction.Reset:
                    ResetSelectionOperations();
                    break;
                case ToolbarAction.PresetLow:
                    SetToolbarPreset(AdjustmentPreset.Low);
                    break;
                case ToolbarAction.PresetBalanced:
                    SetToolbarPreset(AdjustmentPreset.Balanced);
                    break;
                case ToolbarAction.PresetHigh:
                    SetToolbarPreset(AdjustmentPreset.High);
                    break;
                case ToolbarAction.Save:
                    CommitSelection(SelectionCommitAction.Save);
                    break;
                case ToolbarAction.Copy:
                    CommitSelection(SelectionCommitAction.Copy);
                    break;
            }
        }

        private void SelectToolbarTool(EditMode nextMode, System.Drawing.Rectangle anchor)
        {
            bool alreadyActive = mode == nextMode;
            mode = nextMode;
            HideOptionsPopover();
            if (alreadyActive)
            {
                ShowToolOptions(nextMode, anchor);
            }
            Invalidate();
        }

        private void SetToolbarPreset(AdjustmentPreset preset)
        {
            selectedPreset = preset;
            Invalidate();
        }

        private void DrawSelectionOperations(Graphics graphics)
        {
            System.Drawing.Rectangle selection = CurrentSelection();
            foreach (EditOperation operation in operations)
            {
                System.Drawing.Rectangle rect = ImageToClient(operation.Rect);
                if (operation.Type == EditOperationType.Marker)
                {
                    using Pen pen = new(operation.Color, operation.StrokeWidth);
                    graphics.DrawRectangle(pen, rect);
                }
                else if (operation.Type == EditOperationType.Ellipse)
                {
                    using Pen pen = new(operation.Color, operation.StrokeWidth);
                    graphics.DrawEllipse(pen, rect);
                }
                else if (operation.Type == EditOperationType.Pen && operation.Points is not null)
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(selection);
                    DrawClientPolyline(graphics, operation.Points.Select(point => IsPathBreak(point) ? point : ImagePointToClient(point)).ToList(), operation.Color, operation.StrokeWidth);
                    graphics.Clip = oldClip;
                }
                else if (operation.Type == EditOperationType.Mosaic && operation.Points is not null)
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(InsetRectangle(selection, Math.Max(2, operation.StrokeWidth / 2 + 2)));
                    DrawMosaicBrushPreview(graphics, operation.Points.Select(point => IsPathBreak(point) ? point : ImagePointToClient(point)).ToList(), operation.StrokeWidth);
                    graphics.Clip = oldClip;
                }
                else
                {
                    using Region oldClip = graphics.Clip;
                    graphics.SetClip(InsetRectangle(selection, Math.Max(2, operation.StrokeWidth / 2 + 2)));
                    DrawMosaicPreview(graphics, operation.Rect, rect, false);
                    graphics.Clip = oldClip;
                }
            }
        }

        private void DrawPresetPreview(Graphics graphics, System.Drawing.Rectangle selection)
        {
            if (selectedPreset == AdjustmentPreset.Balanced) return;
            Color overlay = selectedPreset == AdjustmentPreset.High
                ? Color.FromArgb(34, 255, 244, 210)
                : Color.FromArgb(50, 0, 0, 0);
            using SolidBrush brush = new(overlay);
            graphics.FillRectangle(brush, selection);
        }

        private static void DrawClientPolyline(Graphics graphics, IReadOnlyList<System.Drawing.Point> points, Color color, int width)
        {
            if (points.Count < 2) return;
            using Pen pen = new(color, Math.Max(1, width)) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            List<System.Drawing.Point> segment = new();
            foreach (System.Drawing.Point point in points)
            {
                if (IsPathBreak(point))
                {
                    if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
                    segment.Clear();
                    continue;
                }
                segment.Add(point);
            }
            if (segment.Count > 1) graphics.DrawLines(pen, segment.ToArray());
        }

        private void DrawMosaicBrushPreview(Graphics graphics, IReadOnlyList<System.Drawing.Point> points, int brushSize)
        {
            if (points.Count == 0) return;
            int radius = Math.Max(4, brushSize / 2);
            using Region oldClip = graphics.Clip;
            using GraphicsPath path = BuildBrushPath(points, radius);
            System.Drawing.Rectangle selection = CurrentSelection();
            System.Drawing.Rectangle bounds = System.Drawing.Rectangle.Round(path.GetBounds());
            bounds.Intersect(InsetRectangle(selection, Math.Max(2, brushSize / 2 + 2)));
            if (bounds.Width <= 0 || bounds.Height <= 0)
            {
                graphics.Clip = oldClip;
                return;
            }

            graphics.SetClip(path);
            int block = Math.Max(10, brushSize);
            for (int y = bounds.Top; y < bounds.Bottom; y += block)
            {
                for (int x = bounds.Left; x < bounds.Right; x += block)
                {
                    int w = Math.Min(block, bounds.Right - x);
                    int h = Math.Min(block, bounds.Bottom - y);
                    using SolidBrush brush = new(PseudoPreviewBlockColor(selection, x, y, w, h));
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
            graphics.Clip = oldClip;
        }

        private bool ShouldAppendStrokePoint(System.Drawing.Point point)
        {
            int minDistance = mode == EditMode.Mosaic
                ? Math.Max(3, mosaicBrushSize / 6)
                : Math.Max(2, penStrokeWidth);
            if (currentPenPoints.Count == 0) return true;
            System.Drawing.Point previous = currentPenPoints[^1];
            if (IsPathBreak(previous)) return true;
            int dx = point.X - previous.X;
            int dy = point.Y - previous.Y;
            return (dx * dx + dy * dy) >= minDistance * minDistance;
        }

        private Color PseudoPreviewBlockColor(System.Drawing.Rectangle selection, int x, int y, int blockWidth, int blockHeight)
        {
            double u = selection.Width <= 1 ? 0.0 : (x + blockWidth * 0.5 - selection.Left) / Math.Max(1.0, selection.Width - 1.0);
            double v = selection.Height <= 1 ? 0.0 : (y + blockHeight * 0.5 - selection.Top) / Math.Max(1.0, selection.Height - 1.0);
            u = Math.Clamp(u, 0.0, 1.0);
            v = Math.Clamp(v, 0.0, 1.0);
            Color top = PreviewColorAt(selection.Left + (int)Math.Round(u * Math.Max(0, selection.Width - 1)), selection.Top);
            Color bottom = PreviewColorAt(selection.Left + (int)Math.Round(u * Math.Max(0, selection.Width - 1)), selection.Bottom - 1);
            Color left = PreviewColorAt(selection.Left, selection.Top + (int)Math.Round(v * Math.Max(0, selection.Height - 1)));
            Color right = PreviewColorAt(selection.Right - 1, selection.Top + (int)Math.Round(v * Math.Max(0, selection.Height - 1)));
            double horizontalWeight = Math.Clamp(0.5 + (Math.Min(u, 1.0 - u) - Math.Min(v, 1.0 - v)), 0.15, 0.85);
            double verticalWeight = 1.0 - horizontalWeight;
            int noise = DeterministicNoise(x / Math.Max(1, blockWidth), y / Math.Max(1, blockHeight));
            byte r = QuantizeColor((long)(horizontalWeight * Lerp(left.R, right.R, u) + verticalWeight * Lerp(top.R, bottom.R, v) + noise));
            byte g = QuantizeColor((long)(horizontalWeight * Lerp(left.G, right.G, u) + verticalWeight * Lerp(top.G, bottom.G, v) + noise));
            byte b = QuantizeColor((long)(horizontalWeight * Lerp(left.B, right.B, u) + verticalWeight * Lerp(top.B, bottom.B, v) + noise));
            return Color.FromArgb(r, g, b);
        }

        private Color PreviewColorAt(int clientX, int clientY)
        {
            System.Drawing.Point imagePoint = ClientPointToImage(new System.Drawing.Point(clientX, clientY));
            return preview.GetPixel(imagePoint.X, imagePoint.Y);
        }

        private Color AveragePreviewBlock(int x, int y, int blockWidth, int blockHeight)
        {
            long rSum = 0;
            long gSum = 0;
            long bSum = 0;
            int samples = 0;
            int step = Math.Max(1, Math.Min(blockWidth, blockHeight) / 4);
            for (int py = y; py < y + blockHeight; py += step)
            {
                for (int px = x; px < x + blockWidth; px += step)
                {
                    System.Drawing.Point imagePoint = ClientPointToImage(new System.Drawing.Point(px, py));
                    Color color = preview.GetPixel(imagePoint.X, imagePoint.Y);
                    rSum += color.R;
                    gSum += color.G;
                    bSum += color.B;
                    samples++;
                }
            }
            samples = Math.Max(1, samples);
            return Color.FromArgb(QuantizeColor(rSum / samples), QuantizeColor(gSum / samples), QuantizeColor(bSum / samples));
        }

        private Bitmap? GetMosaicPreviewBitmap(System.Drawing.Rectangle selection, int brushSize)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            selection.Intersect(imageBounds);
            if (selection.Width <= 0 || selection.Height <= 0) return null;
            System.Drawing.Rectangle imageSelection = ClientToImage(selection);
            if (mosaicPreviewBitmap is not null &&
                mosaicPreviewSelection == imageSelection &&
                mosaicPreviewBrushSize == brushSize)
            {
                return mosaicPreviewBitmap;
            }

            DisposeMosaicPreviewCache();
            using Bitmap crop = preview.Clone(imageSelection, PixelFormat.Format32bppArgb);
            int pixelSize = Math.Max(18, brushSize * 2);
            using Bitmap pixelatedCrop = CreatePseudoPixelatedBitmap(crop, pixelSize);
            Bitmap cached = new(selection.Width, selection.Height, PixelFormat.Format32bppArgb);
            cached.SetResolution(96.0f, 96.0f);
            using (Graphics cachedGraphics = Graphics.FromImage(cached))
            {
                cachedGraphics.InterpolationMode = InterpolationMode.NearestNeighbor;
                cachedGraphics.PixelOffsetMode = PixelOffsetMode.Half;
                cachedGraphics.DrawImage(pixelatedCrop, new Rectangle(0, 0, cached.Width, cached.Height));
            }
            mosaicPreviewSelection = imageSelection;
            mosaicPreviewBrushSize = brushSize;
            mosaicPreviewBitmap = cached;
            return mosaicPreviewBitmap;
        }

        private void DisposeMosaicPreviewCache()
        {
            mosaicPreviewBitmap?.Dispose();
            mosaicPreviewBitmap = null;
            mosaicPreviewSelection = System.Drawing.Rectangle.Empty;
            mosaicPreviewBrushSize = 0;
        }

        private void DrawMosaicPreview(Graphics graphics, System.Drawing.Rectangle imageRect, System.Drawing.Rectangle clientRect, bool drawBorder = false)
        {
            if (imageRect.Width <= 0 || imageRect.Height <= 0 || clientRect.Width <= 0 || clientRect.Height <= 0) return;
            int imageBlock = Math.Max(10, Math.Max(12, mosaicBrushSize));
            int clientBlock = Math.Max(6, (int)Math.Round(imageBlock * (clientRect.Width / Math.Max(1.0, (double)imageRect.Width))));
            using Region oldClip = graphics.Clip;
            graphics.SetClip(clientRect);
            for (int y = clientRect.Top; y < clientRect.Bottom; y += clientBlock)
            {
                for (int x = clientRect.Left; x < clientRect.Right; x += clientBlock)
                {
                    int w = Math.Min(clientBlock, clientRect.Right - x);
                    int h = Math.Min(clientBlock, clientRect.Bottom - y);
                    using SolidBrush brush = new(PseudoPreviewBlockColor(clientRect, x, y, w, h));
                    graphics.FillRectangle(brush, x, y, w, h);
                }
            }
            graphics.Clip = oldClip;
            if (drawBorder)
            {
                using Pen pen = new(Color.White, 1.5f);
                graphics.DrawRectangle(pen, clientRect);
            }
        }

        private System.Drawing.Rectangle CurrentEditSelection()
        {
            int left = Math.Clamp(Math.Min(editStart.X, editCurrent.X), 0, ClientSize.Width);
            int top = Math.Clamp(Math.Min(editStart.Y, editCurrent.Y), 0, ClientSize.Height);
            int right = Math.Clamp(Math.Max(editStart.X, editCurrent.X), 0, ClientSize.Width);
            int bottom = Math.Clamp(Math.Max(editStart.Y, editCurrent.Y), 0, ClientSize.Height);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private static System.Drawing.Point ClampPointToRectangle(System.Drawing.Point point, System.Drawing.Rectangle rect)
        {
            if (rect.Width <= 0 || rect.Height <= 0) return point;
            return new System.Drawing.Point(
                Math.Clamp(point.X, rect.Left, rect.Right - 1),
                Math.Clamp(point.Y, rect.Top, rect.Bottom - 1));
        }

        private static System.Drawing.Rectangle InsetRectangle(System.Drawing.Rectangle rect, int inset)
        {
            if (rect.Width <= inset * 2 || rect.Height <= inset * 2) return rect;
            return System.Drawing.Rectangle.Inflate(rect, -inset, -inset);
        }

        private static System.Drawing.Rectangle InflateRectangle(System.Drawing.Rectangle rect, int amount)
        {
            return System.Drawing.Rectangle.Inflate(rect, amount, amount);
        }

        private System.Drawing.Rectangle ImageToClient(System.Drawing.Rectangle rect)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / Math.Max(1.0, (double)preview.Width);
            double scaleY = imageBounds.Height / Math.Max(1.0, (double)preview.Height);
            int x = imageBounds.X + (int)Math.Round(rect.X * scaleX);
            int y = imageBounds.Y + (int)Math.Round(rect.Y * scaleY);
            int right = imageBounds.X + (int)Math.Round(rect.Right * scaleX);
            int bottom = imageBounds.Y + (int)Math.Round(rect.Bottom * scaleY);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Point ClientPointToImage(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = preview.Width / Math.Max(1.0, (double)imageBounds.Width);
            double scaleY = preview.Height / Math.Max(1.0, (double)imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((point.X - imageBounds.X) * scaleX), 0, preview.Width - 1);
            int y = Math.Clamp((int)Math.Round((point.Y - imageBounds.Y) * scaleY), 0, preview.Height - 1);
            return new System.Drawing.Point(x, y);
        }

        private System.Drawing.Point ImagePointToClient(System.Drawing.Point point)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            double scaleX = imageBounds.Width / Math.Max(1.0, (double)preview.Width);
            double scaleY = imageBounds.Height / Math.Max(1.0, (double)preview.Height);
            int x = imageBounds.X + (int)Math.Round(point.X * scaleX);
            int y = imageBounds.Y + (int)Math.Round(point.Y * scaleY);
            return new System.Drawing.Point(x, y);
        }

        private static void DrawSelectionHandles(Graphics graphics, System.Drawing.Rectangle selection)
        {
            using SolidBrush brush = new(Color.White);
            int size = 8;
            int half = size / 2;
            System.Drawing.Point[] points =
            {
                new(selection.Left, selection.Top),
                new(selection.Left + selection.Width / 2, selection.Top),
                new(selection.Right, selection.Top),
                new(selection.Right, selection.Top + selection.Height / 2),
                new(selection.Right, selection.Bottom),
                new(selection.Left + selection.Width / 2, selection.Bottom),
                new(selection.Left, selection.Bottom),
                new(selection.Left, selection.Top + selection.Height / 2)
            };
            foreach (System.Drawing.Point point in points)
            {
                graphics.FillRectangle(brush, point.X - half, point.Y - half, size, size);
            }
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (!mouseInputReady) return;
            if (e.Control && e.KeyCode == Keys.Z)
            {
                UndoSelectionOperation();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.Control && e.KeyCode == Keys.Y)
            {
                RedoSelectionOperation();
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }
            if (e.KeyCode == Keys.Escape)
            {
                CancelSelection();
            }
            base.OnKeyDown(e);
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WmHotkey && m.WParam.ToInt32() == EscapeHotkeyId)
            {
                CancelSelection();
                return;
            }

            base.WndProc(ref m);
        }

        private void CancelSelection()
        {
            HideBeforeClose();
            DialogResult = DialogResult.Cancel;
            Close();
        }

        private System.Drawing.Rectangle CurrentSelection()
        {
            int left = Math.Clamp(Math.Min(dragStart.X, dragCurrent.X), 0, ClientSize.Width);
            int top = Math.Clamp(Math.Min(dragStart.Y, dragCurrent.Y), 0, ClientSize.Height);
            int right = Math.Clamp(Math.Max(dragStart.X, dragCurrent.X), 0, ClientSize.Width);
            int bottom = Math.Clamp(Math.Max(dragStart.Y, dragCurrent.Y), 0, ClientSize.Height);
            return System.Drawing.Rectangle.FromLTRB(left, top, right, bottom);
        }

        private System.Drawing.Rectangle ClientToImage(System.Drawing.Rectangle rect)
        {
            System.Drawing.Rectangle imageBounds = PreviewImageBounds();
            rect.Intersect(imageBounds);
            double scaleX = preview.Width / Math.Max(1.0, (double)imageBounds.Width);
            double scaleY = preview.Height / Math.Max(1.0, (double)imageBounds.Height);
            int x = Math.Clamp((int)Math.Round((rect.X - imageBounds.X) * scaleX), 0, preview.Width - 1);
            int y = Math.Clamp((int)Math.Round((rect.Y - imageBounds.Y) * scaleY), 0, preview.Height - 1);
            int right = Math.Clamp((int)Math.Round((rect.Right - imageBounds.X) * scaleX), x + 1, preview.Width);
            int bottom = Math.Clamp((int)Math.Round((rect.Bottom - imageBounds.Y) * scaleY), y + 1, preview.Height);
            return System.Drawing.Rectangle.FromLTRB(x, y, right, bottom);
        }

        private System.Drawing.Rectangle PreviewImageBounds()
        {
            if (preview.Width <= 0 || preview.Height <= 0 || ClientSize.Width <= 0 || ClientSize.Height <= 0)
            {
                return ClientRectangle;
            }
            double ratio = Math.Min(ClientSize.Width / (double)preview.Width, ClientSize.Height / (double)preview.Height);
            int width = Math.Max(1, (int)Math.Round(preview.Width * ratio));
            int height = Math.Max(1, (int)Math.Round(preview.Height * ratio));
            int x = (ClientSize.Width - width) / 2;
            int y = (ClientSize.Height - height) / 2;
            return new System.Drawing.Rectangle(x, y, width, height);
        }
    }

    private static void ApplyDarkWindowFrame(nint hwnd)
    {
        if (hwnd == 0) return;

        int enabled = 1;
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, ref enabled, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkModeLegacy, ref enabled, sizeof(int));

        int captionColor = ColorRef(Color.FromArgb(15, 17, 18));
        int borderColor = ColorRef(Color.FromArgb(55, 60, 64));
        int textColor = ColorRef(Color.FromArgb(235, 239, 242));
        _ = DwmSetWindowAttribute(hwnd, DwmwaCaptionColor, ref captionColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaBorderColor, ref borderColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaTextColor, ref textColor, sizeof(int));
    }

    private static int ColorRef(Color color)
    {
        return color.R | (color.G << 8) | (color.B << 16);
    }

    [DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern bool IsWindow(nint hwnd);

    [DllImport("user32.dll")]
    private static extern bool SetWindowDisplayAffinity(nint hwnd, uint affinity);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(nint hwnd, int id, uint fsModifiers, int vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(nint hwnd, int id);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(nint hwnd, nint hwndInsertAfter, int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(nint hwnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(nint hwnd);

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(nint hwnd, int attribute, ref int attributeValue, int attributeSize);

}








