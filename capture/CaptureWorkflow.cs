using System.Drawing;
using System.Drawing.Imaging;
using Windows.Graphics.Capture;
using Windows.Graphics.DirectX;
using Windows.Graphics.DirectX.Direct3D11;

internal static partial class Program
{
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
            // Keep selection on the fast compositor preview; the WGC HDR frame is for final output only.
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

}
