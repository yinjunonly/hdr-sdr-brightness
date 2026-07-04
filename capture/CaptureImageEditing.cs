using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

internal static partial class Program
{
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

}
