using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

internal static partial class Program
{
    private sealed partial class RegionSelectionForm
    {
        private Bitmap? selectionBackgroundBitmap;

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

    }
}
